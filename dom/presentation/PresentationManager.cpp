#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Services.h"
#include "mozilla/dom/BindingUtils.h"

#include "nsIObserverService.h"
#include "nsIPresentationDeviceManager.h"
#include "nsIPropertyBag2.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsServiceManagerUtils.h"

#include "PresentationManager.h"

using namespace mozilla;

BEGIN_PRESENTATION_NAMESPACE

StaticRefPtr<PresentationManager> PresentationManager::sPresentationManager;

NS_IMPL_ISUPPORTS(PresentationManager, nsIObserver)

PresentationManager::PresentationManager()
  : mObserverList(PresentationEventObserverList())
{
}

PresentationManager::~PresentationManager()
{

}

bool
PresentationManager::Init()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, false);

  obs->AddObserver(this, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID, false);
  obs->AddObserver(this, PRESENTATION_DEVICE_CHANGE_TOPIC, false);
  obs->AddObserver(this, PRESENTATION_SESSION_REQUEST_TOPIC, false);

  nsCOMPtr<nsIPresentationDeviceManager> pdMgr =
        do_GetService(PRESENTATION_DEVICE_MANAGER_CONTRACTID);
  NS_ENSURE_TRUE(pdMgr, false);

  bool available = false;
  if (!NS_FAILED(pdMgr->GetDeviceAvailable(&available))) {
    NotifyDeviceAvailable(available);
  }

  mEnableTest = Preferences::GetBool("dom.presentation.test.enabled", false);
  if (mEnableTest) {
    NS_ENSURE_TRUE(TestAvailability(), false);
    NS_ENSURE_TRUE(TestJoinSession(), false);
  }
  return true;
}

PLDHashOperator
PresentationManager::ClearSessionInternalArray(const nsACString& key,
                                               nsAutoPtr<SessionInternalArray>& array,
                                               void* cx)
{
  if (array) {
    for (uint32_t i = 0; i < array->Length(); ++i) {
      if (array->ElementAt(i)) {
        array->ElementAt(i)->CloseConnection();
        array->RemoveElementAt(i);
      }
    }
    array->Clear();
  }
  return PL_DHASH_REMOVE;
}

nsresult
PresentationManager::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID);
    obs->RemoveObserver(this, PRESENTATION_DEVICE_CHANGE_TOPIC);
    obs->RemoveObserver(this, PRESENTATION_SESSION_REQUEST_TOPIC);
  }

  mSessionInternalStore.Enumerate(ClearSessionInternalArray, this);
  mSessionInternalStore.Clear();

  if (mEnableTest) {
    mAvailableNotify = nullptr;
    if (mTimerAvailable) {
      mTimerAvailable->Cancel();
      mTimerAvailable = nullptr;
    }
  }
  return NS_OK;
}

void
PresentationManager::AddObserver(PresentationEventObserver* aObserver)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  mObserverList.AddObserver(aObserver);
}

void
PresentationManager::RemoveObserver(PresentationEventObserver* aObserver)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  mObserverList.RemoveObserver(aObserver);
}

void
PresentationManager::NotifyDeviceAvailable(bool aAvailable)
{
  PresentationEvent pEvent;
  pEvent.eventType = EPresentationEvent::DeviceAvailableChanged;
  pEvent.available = aAvailable;
  NotifyPresentationEvent(pEvent);
}

void
PresentationManager::NotifyDeviceSelected(const nsACString& aOrigin,
                                          const nsACString& aURL,
                                          const nsACString& aSessionId,
                                          const nsACString& aId,
                                          const nsACString& aName,
                                          const nsACString& aType)
{
  PresentationEvent pEvent;
  pEvent.eventType = EPresentationEvent::DeviceSelected;
  pEvent.deviceInfo.mOrigin = aOrigin;
  pEvent.deviceInfo.mURL = aURL;
  pEvent.deviceInfo.mSessionId = aSessionId;
  pEvent.deviceInfo.mId = aId;
  pEvent.deviceInfo.mName = aName;
  pEvent.deviceInfo.mType = aType;
  NotifyPresentationEvent(pEvent);
}

inline void
PresentationManager::NotifyPresentationEvent(PresentationEvent aEvent)
{
  mObserverList.Broadcast(aEvent);
}

bool
PresentationManager::StartPresentationSession(const nsACString& aOrigin,
                                              const nsACString& aURL,
                                              const nsACString& aLastSessionId)
{
  nsCOMPtr<nsIPresentationDevicePrompt> pdPrompt =
          do_GetService(PRESENTATION_DEVICE_PROMPT_CONTRACTID);

  nsRefPtr<nsPresentationDeviceRequest> request =
      new nsPresentationDeviceRequest();
  NS_ENSURE_TRUE(request, false);

  request->SetupRequest(static_cast<ICallbackDeviceSelection*>(this),
                        aOrigin,
                        aURL,
                        aLastSessionId);
  if (mEnableTest) {
    nsRefPtr<MockPresentationDevice> testDevice = new MockPresentationDevice();
    request->Select(testDevice);
  } else if (!mEnableTest && pdPrompt) {
    pdPrompt->PromptDeviceSelection(request);
  } else {
    return false;
  }
  return true;
}

bool
PresentationManager::JoinPresentationSession(const nsACString& aOrigin,
                                             const nsACString& aURL,
                                             const nsACString& aLastSessionId)
{
  PresentationFindSessionRunnable* runnable =
       new PresentationFindSessionRunnable(aOrigin,
                                           aURL,
                                           aLastSessionId);
  NS_DispatchToMainThread(runnable);
  return true;
}

// static
PresentationManager*
PresentationManager::GetInstance()
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(NS_IsMainThread());

  if (!sPresentationManager) {
    PresentationManager* manager = new PresentationManager();
    NS_ENSURE_TRUE(manager->Init(), nullptr);
    sPresentationManager = manager;
    ClearOnShutdown(&sPresentationManager);
  }
  return sPresentationManager;
}

PresentationSessionInternal*
PresentationManager::GetPSInternal(const nsACString& aURL,
                                   const nsACString& aSessionId,
                                   const nsACString& aDeviceId)
{
  nsAutoCString sessionKey;
  sessionKey = aURL + aSessionId;

  if (mSessionInternalStore.Contains(sessionKey)) {
    SessionInternalArray* arr = nullptr;
    mSessionInternalStore.Get(sessionKey, &arr);
    NS_ENSURE_TRUE(arr, nullptr);

    for (uint32_t i = 0; i < arr->Length(); ++i) {
      if (arr->ElementAt(i)) {
        nsAutoCString tmpDeviceID;
        arr->ElementAt(i)->GetDeviceId(tmpDeviceID);
        if (tmpDeviceID == aDeviceId) {
          return arr->ElementAt(i);
        }
      }
    }
  }
  return nullptr;
}

void
PresentationManager::ReleasePSInternal(const nsACString& aURL,
                                       const nsACString& aSessionId,
                                       const nsACString& aDeviceId)
{
  DestroySessionInternalRunnable* runnable =
      new DestroySessionInternalRunnable(aURL,
                                         aSessionId,
                                         aDeviceId);
  NS_DispatchToMainThread(runnable);
}

bool
PresentationManager::ReleasePSInternal_Impl(const nsACString& aURL,
                                            const nsACString& aSessionId,
                                            const nsACString& aDeviceId)
{
  nsAutoCString sessionKey;
  sessionKey = aURL + aSessionId;
  if (mSessionInternalStore.Contains(sessionKey)) {
    SessionInternalArray* arr = nullptr;
    mSessionInternalStore.Get(sessionKey, &arr);
    NS_ENSURE_TRUE(arr, false);

    for (uint32_t i = 0; i < arr->Length(); ++i) {
      if (arr->ElementAt(i)) {
        nsAutoCString tmpDeviceID;
        arr->ElementAt(i)->GetDeviceId(tmpDeviceID);
        if (tmpDeviceID == aDeviceId) {
          arr->RemoveElementAt(i);
          break;
        }
      }
    }
    if (arr->Length() == 0) {
      mSessionInternalStore.Remove(sessionKey);
    }
  }
  return false;
}

NS_IMETHODIMP
PresentationManager::Observe(nsISupports* aSubject,
                             const char* aTopic,
                             const char16_t* aData)
{
  nsresult rv = NS_OK;
  if (!strcmp(aTopic, PRESENTATION_DEVICE_CHANGE_TOPIC)) {
    // EqualsLiteral
    bool available = !NS_strcmp(aData,
        MOZ_UTF16("dom.presentation.available-devices"))? true : false;
    NotifyDeviceAvailable(available);
  } else if (!strcmp(aTopic, PRESENTATION_SESSION_REQUEST_TOPIC)) {
    // do_QueryInterface
    // rv = HandleSessionRequest(aSubject)
    nsRefPtr<PresentationSessionRequest> psRequest =
      static_cast<PresentationSessionRequest*>(aSubject);
    rv = HandleCreateSessionRequest(psRequest);
  } else if (!strcmp(aTopic, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID)) {
    return HandleShutdown();
  } else {
    MOZ_ASSERT(false, "PresentationManager got unexpected topic!");
    return NS_ERROR_UNEXPECTED;
  }
  return rv;
}

void
PresentationManager::GetUniqueSessionId(nsACString& aSessionId)
{
  nsAutoCString sessionId;
  nsAutoString hostName;
  nsCOMPtr<nsIPropertyBag2> infoService = do_GetService("@mozilla.org/system-info;1");
  if (infoService) {
    infoService->GetPropertyAsAString(NS_LITERAL_STRING("host"), hostName);
  }
  uint64_t time = PR_IntervalNow();
  std::stringstream ss;
  ss << time;
  sessionId = NS_ConvertUTF16toUTF8(hostName) + nsCString(ss.str().c_str());
  aSessionId.Assign(sessionId);
}

nsresult
PresentationManager::CallbackFromDeviceRequest(nsIPresentationDevice* aDevice,
                                               const nsACString& aOrigin,
                                               const nsACString& aRequestURL,
                                               const nsACString& aLastSessionId)
{
  // There should be always a device selection when calling StartSession.
  nsAutoCString deviceId;
  nsAutoCString deviceName;
  nsAutoCString deviceType;
  nsAutoCString sessionId;

  if (aDevice) {
    // A device is selected
    aDevice->GetId(deviceId);
    aDevice->GetName(deviceName);
    aDevice->GetType(deviceType);

    nsAutoCString sessionKey;
    if (!aLastSessionId.IsEmpty()) {
      sessionId.Assign(aLastSessionId);
    } else {
      GetUniqueSessionId(sessionId);
    }
    sessionKey = aRequestURL + sessionId;

    SessionInternalArray* arr = nullptr;
    if (!mSessionInternalStore.Contains(sessionKey)) {
      // new session in controller UA. (But there might be a matching session
      // which already exists on presenter UA)
      arr = new SessionInternalArray();
      NS_ENSURE_TRUE(arr, NS_ERROR_FAILURE);

      mSessionInternalStore.Put(sessionKey, arr);
      nsRefPtr<PresentationSessionInternal> psi = new PresentationSessionInternal();
      NS_ENSURE_TRUE(psi, NS_ERROR_FAILURE);

      psi->SetupPSIInfo(aOrigin, aRequestURL, sessionId, aDevice);
      arr->AppendElement(psi);
    } else {
      // Found an existing session on controller UA, check if the device is matched.
      // (But the session might doesn't exist on presenter for some reason)
      mSessionInternalStore.Get(sessionKey, &arr);
      NS_ENSURE_TRUE(arr, NS_ERROR_FAILURE);
      bool found = false;
      for (uint32_t i = 0; i < arr->Length(); ++i) {
        if (arr->ElementAt(i)) {
          nsAutoCString tmpDeviceID;
          arr->ElementAt(i)->GetDeviceId(tmpDeviceID);
          if (tmpDeviceID == deviceId) {
            found = true;
            break;
          }
        }
      }
      if (!found) {
        deviceType = deviceName = deviceId = NS_LITERAL_CSTRING("");
      }
    }
  } else {
    // User canceled device selection.
  }

  NotifyDeviceSelected(aOrigin,
                       aRequestURL,
                       sessionId,
                       deviceId,
                       deviceName,
                       deviceType);
  return NS_OK;
}

void
PresentationManager::FindBestMatchSessionDevice(const nsACString& aURL,
                                                const nsACString& aSessionId,
                                                nsACString* aDeviceId,
                                                nsACString* aDeviceName,
                                                nsACString* aDeviceType)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsAutoCString sessionKey;
  sessionKey = aURL + aSessionId;
  if (!mSessionInternalStore.Contains(sessionKey)) {
    // Find no previous session existing on contorller UA.
    // TODO : Maybe need to ping all reachable presentation UA
    // if one is presenting with this sessionKey. (Since we don't want to
    // pop device selection UI to user, so that no device information will be
    // provided. In this case, I think no connection should be created.)
    return;
  } else {
    // Previous sessions exist on controlling UA, pick one.
    SessionInternalArray* arr = nullptr;
    mSessionInternalStore.Get(sessionKey, &arr);
    NS_ENSURE_TRUE_VOID(arr);
    for (uint32_t i = 0; i < arr->Length(); ++i) {
      // TODO : Currently, pick the first one. Maybe pick the most recent one.
      if (arr->ElementAt(i)) {
        arr->ElementAt(i)->GetDeviceId(*aDeviceId);
        arr->ElementAt(i)->GetDeviceName(*aDeviceName);
        arr->ElementAt(i)->GetDeviceType(*aDeviceType);
        break;
      }
    }
  }
}

nsresult
PresentationManager::HandleCreateSessionRequest(nsRefPtr<PresentationSessionRequest> &aReq)
//PresentationManager::HandleCreateSessionRequest(nsISupport* aRequest)
{
  // nsCOMPtr<nsIPresentationSessionRequest> req = do_QueryInterface(aRequest);
  // if (!req) {
  //   return NS_ERROR_FAILURE;
  // }
  nsAutoString url;
  nsAutoString controllerSessionId;
  nsCOMPtr<nsIPresentationSessionTransport> psTransport;
  nsCOMPtr<nsIPresentationDevice> pDevice;

  aReq->GetUrl(url);
  aReq->GetPresentationId(controllerSessionId);
  aReq->GetSessionTransport(getter_AddRefs(psTransport)); // unused
  aReq->GetDevice(getter_AddRefs(pDevice));
  NS_ENSURE_TRUE(psTransport, NS_ERROR_FAILURE); // NS_ENSURE_TRUE, deprecated
  NS_ENSURE_TRUE(pDevice, NS_ERROR_FAILURE);

  // The contollerSessionID may be a new generated one or a lastSessionID which
  // stands for an existing presentation presenting on presenter now.
  // XXX: string usage
  nsAutoCString requestURL(NS_ConvertUTF16toUTF8(url).get());
  nsAutoCString sessionId(NS_ConvertUTF16toUTF8(controllerSessionId).get());
  nsAutoCString sessionKey;
  sessionKey = requestURL + sessionId;

  SessionInternalArray* arr = nullptr;
//  bool isNewSession = false;
  if (!mSessionInternalStore.Contains(sessionKey)) {
    // No target presenting session on presenter !
    arr = new SessionInternalArray();
    NS_ENSURE_TRUE(arr, NS_ERROR_FAILURE);

    mSessionInternalStore.Put(sessionKey, arr);
    nsRefPtr<PresentationSessionInternal> psi = new PresentationSessionInternal();
    NS_ENSURE_TRUE(psi, NS_ERROR_FAILURE);

    psi->SetupPSIInfo(NS_LITERAL_CSTRING("presenter"), requestURL, sessionId, pDevice);
    arr->AppendElement(psi);
//    isNewSession = true;
  } else {
    // The request session exists on current presenter.
  }

  bool found = false;
  nsAutoCString deviceId;
  pDevice->GetId(deviceId);

  mSessionInternalStore.Get(sessionKey, &arr);
  NS_ENSURE_TRUE(arr, NS_ERROR_UNEXPECTED);
  for (uint32_t i = 0; i < arr->Length(); ++i) {
    if (arr->ElementAt(i)) {
      nsAutoCString tmpDeviceID;
      arr->ElementAt(i)->GetDeviceId(tmpDeviceID);
      if (tmpDeviceID == deviceId) {
        found = true;
        // The idea of |StartConnection| here is kinda accepting the connection.
        arr->ElementAt(i)->StartConnection();
        break;
      }
    }
  }
  NS_ENSURE_TRUE(found, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsISupportsArray> argsArray;
  nsresult rv = NS_NewISupportsArray(getter_AddRefs(argsArray));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsCString> scriptableURL (do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID));
  NS_ENSURE_TRUE(scriptableURL, NS_ERROR_FAILURE);

  scriptableURL->SetData(requestURL);
  rv = argsArray->AppendElement(scriptableURL);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsCString> scriptableSessionId (do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID));
  NS_ENSURE_TRUE(scriptableSessionId, NS_ERROR_FAILURE);

  scriptableSessionId->SetData(sessionId);
  rv = argsArray->AppendElement(scriptableSessionId);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupportsCString> scriptableDeviceId (do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID));
  NS_ENSURE_TRUE(scriptableDeviceId, NS_ERROR_FAILURE);

  scriptableDeviceId->SetData(deviceId);
  rv = argsArray->AppendElement(scriptableDeviceId);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  NS_ENSURE_TRUE(obs, NS_ERROR_UNEXPECTED);
  // Notify SystemApp to launch app and present !
  // TODO : carry |isNewSession| to identify whether to bring up new window
  // to present or connect to existing presentation.
  obs->NotifyObservers(argsArray, "presentation-request-from-manager", nullptr);
  return NS_OK;
}

bool
PresentationManager::TestAvailability()
{
  nsresult rv;
  mTimerAvailable = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_FAILED(rv)) {
    return false;
  }
  mAvailableNotify = new AvailabilityNotifier(this, true);
  rv = mTimerAvailable->InitWithCallback(mAvailableNotify, 100, nsITimer::TYPE_ONE_SHOT);
  if (NS_FAILED(rv)) {
    mTimerAvailable = nullptr;
    mAvailableNotify = nullptr;
    return false;
  }
  return true;
}

bool
PresentationManager::TestJoinSession()
{
  // Add a session for joinSession test
  nsAutoCString url(NS_LITERAL_CSTRING("http://example.org/presentation.html"));
  nsAutoCString sessionId(NS_LITERAL_CSTRING("testJoinSessionSId"));
  nsAutoCString sessionKey;
  sessionKey =  url + sessionId;
  if (!mSessionInternalStore.Contains(sessionKey)) {
    SessionInternalArray* arr = new SessionInternalArray();
    NS_ENSURE_TRUE(arr, false);

    mSessionInternalStore.Put(sessionKey, arr);
    nsRefPtr<PresentationSessionInternal> psi = new PresentationSessionInternal();
    NS_ENSURE_TRUE(psi, false);

    nsRefPtr<MockPresentationDevice> testJoinDevice = new MockPresentationDevice();
    psi->SetupPSIInfo(NS_LITERAL_CSTRING("joinOrigin"),
                      url,
                      sessionId,
                      testJoinDevice);
    arr->AppendElement(psi);
  } else {
    NS_WARNING("Test join session internal should not exist !");
    return false;
  }
  return true;
}

END_PRESENTATION_NAMESPACE
