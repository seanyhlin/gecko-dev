/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MainThreadUtils.h" // NS_IsMainThread
#include "nsCRTGlue.h" // NS_strcmp
#include "nsIAppsService.h"
#include "nsIArray.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMTCPSocket.h"
#include "nsIObserverService.h"
#include "nsIPresentationControlChannel.h"
#include "nsIPresentationDevice.h" // nsIPresentationDevice
#include "nsIPresentationDeviceManager.h" // Topic
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h" // do_GetService
#include "nsString.h"
#include "nsXULAppAPI.h" // XRE_GetProcessType
#include "Presentation.h"
#include "PresentationService.h"
#include "PresentationSessionRequest.h"

#include "mozIApplication.h"
#include "mozilla/Services.h" // namespace
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/presentation/PresentationIPCService.h"
#include "mozilla/unused.h"

#include "nsIFrameLoader.h"

#include "FakePresentationDevice.h"

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "GonkDBus", args);
#else
#define LOG(args...) printf(args);
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::presentation;
using namespace mozilla::services;

StaticRefPtr<PresentationService> sPresentationService;

////////////////////////////////////////////////////////////////////////////////
// DOMContentLoadedListener

class DOMContentLoadedListener MOZ_FINAL : public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
 
  DOMContentLoadedListener(nsIDOMEventTarget* aTarget,
                           Session* aSession)
    : mTarget(aTarget)
    , mHandled(false)
    , mSession(aSession)
  {
  }

  ~DOMContentLoadedListener()
  {
    mTarget = nullptr;
  }

  void
  OnTimeout();
   
private:
  nsCOMPtr<nsIDOMEventTarget> mTarget;
  bool mHandled;
  nsRefPtr<Session> mSession;
};

NS_IMPL_ISUPPORTS(DOMContentLoadedListener, nsIDOMEventListener)

NS_IMETHODIMP
DOMContentLoadedListener::HandleEvent(nsIDOMEvent* aDOMEvent)
{
  MOZ_ASSERT(sPresentationService);

//  mSessionInfo->OnReceiverReady();
  mTarget->RemoveEventListener(NS_LITERAL_STRING("DOMContentLoaded"),
                               static_cast<nsIDOMEventListener*>(this), false);
  mHandled = true;
  return NS_OK;
}

void
DOMContentLoadedListener::OnTimeout()
{
  MOZ_ASSERT(sPresentationService);

  if (!mHandled) {
//    sPresentationService->OnSessionFailure(NS_LITERAL_STRING("LoadTimeout"));
  }
}

////////////////////////////////////////////////////////////////////////////////
// PresentationDeviceRequest

PresentationService::PresentationDeviceRequest::PresentationDeviceRequest(
  const nsAString& aUrl,
  const nsAString& aId,
  const nsAString& aOrigin,
  PresentationService* aService)
  : mRequestUrl(aUrl)
  , mId(aId)
  , mOrigin(aOrigin)
  , mService(aService)
{
}

PresentationService::PresentationDeviceRequest::~PresentationDeviceRequest()
{
  mService = nullptr;
}

NS_IMETHODIMP
PresentationService::PresentationDeviceRequest::GetOrigin(nsAString& aOrigin)
{
  aOrigin = mOrigin;
  return NS_OK;
}

NS_IMETHODIMP
PresentationService::PresentationDeviceRequest::GetRequestURL(nsAString& aRequestUrl)
{
  aRequestUrl = mRequestUrl;
  return NS_OK;
}

NS_IMETHODIMP
PresentationService::PresentationDeviceRequest::Select(nsIPresentationDevice* aDevice)
{
  LOG("PresentationDeviceRequest::Select\n");
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aDevice);

  // Update SesisonInfo.device when user selects a device.
  SessionInfo* info = mService->GetSessionInfo(mId);
  if (!info) {
    return NS_OK;
  }
  info->device = aDevice;

  // Establish control channel. If we failed to do so, the callback is called
  // with an error message.
  nsCOMPtr<nsIPresentationControlChannel> ctrlChannel;
  if (NS_FAILED(aDevice->EstablishControlChannel(mRequestUrl, mId,
                                                 getter_AddRefs(ctrlChannel)))) {
    info->callback->NotifyError(NS_LITERAL_STRING("NoControlChannel"));
  }

   // Create requester and then update SessionInfo. 
  info->session = Session::CreateRequester(mId, mService, ctrlChannel);

  return NS_OK;
}

NS_IMETHODIMP
PresentationService::PresentationDeviceRequest::Cancel()
{
  SessionInfo* info = mService->GetSessionInfo(mId);
  if (NS_WARN_IF(!info)) {
    return NS_OK;
  }

  // The user cancel the prompt, thus reply the callback and reject the promise
  // then.
  MOZ_ASSERT(info->callback);
  info->callback->NotifyError(NS_LITERAL_STRING("UserCanceled"));
  return NS_OK;
}

NS_IMPL_ISUPPORTS(PresentationService::PresentationDeviceRequest,
                  nsIPresentationDeviceRequest) 

/////////////////////////////////////////////////////////////
// Callback
class Callback : public nsITimerCallback
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
};

NS_IMPL_ISUPPORTS(Callback, nsITimerCallback)

NS_IMETHODIMP
Callback::Notify(nsITimer* aTimer)
{
  LOG("Callback::Notify\n");
  sPresentationService->NotifyAvailableListeners(true);
  return NS_OK;
};

///////////////////////////////////////////////////////////////
// DOMContentLoadedTimerCallback

class DOMContentLoadedTimerCallback MOZ_FINAL : public nsITimerCallback
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

  DOMContentLoadedTimerCallback(DOMContentLoadedListener* aListener)
    : mListener(aListener)
  {
  }

  ~DOMContentLoadedTimerCallback()
  {
    mListener = nullptr;
  }

private:
  nsRefPtr<DOMContentLoadedListener> mListener;
};

NS_IMPL_ISUPPORTS(DOMContentLoadedTimerCallback, nsITimerCallback)

NS_IMETHODIMP
DOMContentLoadedTimerCallback::Notify(nsITimer* aTimer)
{
  mListener->OnTimeout();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// PresentationService

NS_IMPL_ISUPPORTS(PresentationService, nsIObserver)

/* static */ PresentationService*
PresentationService::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sPresentationService) {
    return sPresentationService;
  }

  sPresentationService = PresentationService::Create();
  if (NS_WARN_IF(!sPresentationService)) {
    return nullptr;
  }

  return sPresentationService;
}

/* static */ PresentationService*
PresentationService::Create()
{
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return PresentationIPCService::Create();
  }

  LOG("Parent\n");
  return new PresentationService();
}

PresentationService::PresentationService()
  : mAvailable(false)
  , mSessionInfo(128)
  , mLastUniqueId(0)
{
  NS_WARN_IF_FALSE(Init(), "Failed to initialize PresentationService.");

  // HACK
  nsresult rv;
  LOG("Init timer\n");
  mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  nsCOMPtr<nsITimerCallback> callback= new Callback();
  mTimer->InitWithCallback(callback, 100, nsITimer::TYPE_ONE_SHOT);
}

/* virtual */ PresentationService::~PresentationService()
{
  mListeners.Clear();
}

bool
PresentationService::Init()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sPresentationService);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return false;
  }

  obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  obs->AddObserver(this, PRESENTATION_DEVICE_CHANGE_TOPIC, false);
  obs->AddObserver(this, PRESENTATION_SESSION_REQUEST_TOPIC, false);
  obs->AddObserver(this, "presentation-receiver-ready", false);
  obs->AddObserver(this, "presentation-receiver-launched", false);

  nsCOMPtr<nsIPresentationDeviceManager> deviceManager =
    do_GetService(PRESENTATION_DEVICE_MANAGER_CONTRACTID);
  if (NS_WARN_IF(!deviceManager)) {
    return false;
  }

  deviceManager->GetDeviceAvailable(&mAvailable);
  return true;
}

nsresult
PresentationService::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return NS_ERROR_UNEXPECTED;
  }

  obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  obs->RemoveObserver(this, PRESENTATION_DEVICE_CHANGE_TOPIC);

  mSessionInfo.Clear();

  return NS_OK;
}

void
PresentationService::NotifyAvailableListeners(bool aAvailable)
{
  nsTObserverArray<nsCOMPtr<nsIPresentationListener> >::ForwardIterator iter(mListeners);
  while (iter.HasMore()) {
    nsIPresentationListener* listener = iter.GetNext();
    listener->NotifyAvailableChange(aAvailable);
  }
}

void
PresentationService::NotifySessionReady(const nsAString& aId)
{
  nsTObserverArray<nsCOMPtr<nsIPresentationListener> >::ForwardIterator iter(mListeners);
  while (iter.HasMore()) {
    nsIPresentationListener* listener = iter.GetNext();
    listener->NotifySessionReady(aId);
  }
}

void
PresentationService::NotifyStateChange(bool aConnected)
{
  // TODO
}

NS_IMETHODIMP
PresentationService::Observe(nsISupports* aSubject,
                             const char* aTopic,
                             const char16_t* aData)
{
  nsresult rv = NS_OK;
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    rv = HandleShutdown();
  } else if (!strcmp(aTopic, PRESENTATION_DEVICE_CHANGE_TOPIC)) {
    nsCOMPtr<nsIPresentationDeviceManager> deviceManager =
      do_GetService(PRESENTATION_DEVICE_MANAGER_CONTRACTID);
    if (NS_WARN_IF(deviceManager)) {
      bool available;
      deviceManager->GetDeviceAvailable(&available);
      if (available != mAvailable) {
        NotifyAvailableListeners(available);
      }
    }
  } else if (!strcmp(aTopic, "presentation-receiver-ready")) {
    // HACK
    NotifyAvailableListeners(true);
  } else if (!strcmp(aTopic, PRESENTATION_SESSION_REQUEST_TOPIC)) {
    nsCOMPtr<nsIPresentationSessionRequest> request =
      do_QueryInterface(aSubject);
    if (NS_WARN_IF(!request)) {
      return NS_OK;
    }

    nsAutoString id;
    nsCOMPtr<nsIPresentationControlChannel> ctrlChannel;
    nsCOMPtr<nsIPresentationDevice> device;
    request->GetPresentationId(id);
    request->GetControlChannel(getter_AddRefs(ctrlChannel));
    request->GetDevice(getter_AddRefs(device));

    SessionInfo* info = new SessionInfo(device);
    info->session = Session::CreateResponder(id, this, ctrlChannel);
    mSessionInfo.Put(id, info);

    nsAutoString url;
    request->GetUrl(url);
    if (!FindAppOnDevice(url)) {
      // On Sender side, PresentationService::OnSessionFailure() should be called.
      // XXX
      ctrlChannel->Close(NS_ERROR_FAILURE);
    } else {
      nsCOMPtr<nsIObserverService> obs = GetObserverService();
      if (!obs) {
        return NS_OK;
      }

      obs->NotifyObservers(aSubject, "presentation-launch-receiver", nullptr);
    }
  } else if (!strcmp(aTopic, "presentation-receiver-launched")) {
    nsCOMPtr<nsIFrameLoaderOwner> flOwner = do_QueryInterface(aSubject);
    if (NS_WARN_IF(!flOwner)) {
      return NS_OK;
    }

    nsCOMPtr<nsIFrameLoader> frameLoader;
    if (NS_WARN_IF(NS_FAILED(flOwner->GetFrameLoader(getter_AddRefs(frameLoader))))) {
      return NS_OK;
    }

    frameLoader->ActivateFrameEvent(NS_LITERAL_STRING("DOMContentLoaded"),
                                    false);
    mozilla::unused << NS_WARN_IF(NS_FAILED(rv));

    nsCOMPtr<nsIDOMEventTarget> et = do_QueryInterface(aSubject);
    if (NS_WARN_IF(!et)) {
      return NS_OK;
    }

    nsDependentString id(aData);
    SessionInfo* info = GetSessionInfo(id);
    if (NS_WARN_IF(!info)) {
      return NS_OK;
    }

    nsRefPtr<DOMContentLoadedListener> listener = new DOMContentLoadedListener(et, nullptr);
    et->AddEventListener(NS_LITERAL_STRING("DOMContentLoaded"), listener, false);

    mDOMContentLoadedTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    nsCOMPtr<nsITimerCallback> callback =
      new DOMContentLoadedTimerCallback(listener);
    mTimer->InitWithCallback(callback, 1000, nsITimer::TYPE_ONE_SHOT);
  } else {
    MOZ_ASSERT(false, "Unexpected topic for PresentationService.");
    rv = NS_ERROR_UNEXPECTED;
  }

  return rv;
}

bool
PresentationService::FindAppOnDevice(const nsAString& aUrl)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIAppsService> appService = do_GetService("@mozilla.org/AppsService;1");
  if (!appService) {
    return false;
  }

  nsCOMPtr<mozIApplication> app;
  appService->GetAppByManifestURL(aUrl, getter_AddRefs(app));
  if (NS_WARN_IF(!app)) {
    return false;
  }

  return true;
}

/* virtual */ nsresult
PresentationService::StartSessionInternal(const nsAString& aUrl,
                                          const nsAString& aId,
                                          const nsAString& aOrigin,
                                          nsIPresentationRequestCallback* aCallback)
{
  LOG("StartSessionInternal\n");
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCallback);

  // Create SessionInfo and set SessionInfo.callback. The callback is called
  // when the request is finished.
  SessionInfo* info = new SessionInfo(aCallback);
  mSessionInfo.Put(aId, info);

  // Pop up a prompt and ask user to select a device.
  nsCOMPtr<nsIPresentationDeviceRequest> request =
    new PresentationDeviceRequest(aUrl, aId, aOrigin, this);

  nsCOMPtr<nsIPresentationDevicePrompt> prompt =
    do_GetService(PRESENTATION_DEVICE_PROMPT_CONTRACTID);
  if (NS_WARN_IF(!prompt)) {
    return NS_ERROR_FAILURE;
  }
  prompt->PromptDeviceSelection(request);

  // HACK
  nsRefPtr<MockPresentationDevice> testDevice = new MockPresentationDevice();
  request->Select(testDevice);

  return NS_OK;
}

/* virtual */ nsresult
PresentationService::JoinSessionInternal(const nsAString& aUrl,
                                         const nsAString& aSessionId,
                                         const nsAString& aOrigin)
{
  MOZ_ASSERT(NS_IsMainThread());
  // FIXME
/*  nsCOMPtr<nsIPresentationDeviceRequest> request =
    new PresentationDeviceRequest(aUrl, aOrigin, nullptr);*/

  return NS_OK;
}

/* virtual */ void
PresentationService::RegisterListener(nsIPresentationListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  mListeners.AppendElement(aListener);
}

/* virtual */ void
PresentationService::UnregisterListener(nsIPresentationListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  mListeners.RemoveElement(aListener);
}

void
PresentationService::GenerateUniqueId(nsAString& aId)
{
  aId = NS_ConvertUTF8toUTF16(nsPrintfCString("%16lu", ++mLastUniqueId));
}

PresentationService::SessionInfo*
PresentationService::GetSessionInfo(const nsAString& aId)
{
  SessionInfo* info;
  if (!mSessionInfo.Get(aId, &info)) {
    return nullptr;
  }
  return info;
}

void
PresentationService::OnSessionComplete(Session* aSession)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aSession) {
    return;
  }

  SessionInfo* info = GetSessionInfo(aSession->Id());
  if (NS_WARN_IF(!info)) {
    return;
  }

  // Session transport has been established, so notify listener of the state
  // change and invoke callback if any.
  if (info->listener) {
    info->listener->NotifyStateChange(aSession->Id(),
                                      (uint16_t)PresentationSessionState::Connected,
                                      NS_OK);
  }
  if (info->callback) {
    info->callback->NotifySuccess();
  }
}

void
PresentationService::OnSessionClose(Session* aSession, nsresult aReason)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aSession) {
    return;
  }

  SessionInfo* info = GetSessionInfo(aSession->Id());
  if (NS_WARN_IF(!info)) {
    return;
  }

  // The session transport is closed, so notify listener of the state change.
  if (info->listener) {
    info->listener->NotifyStateChange(aSession->Id(),
                                      (uint16_t)PresentationSessionState::Disconnected,
                                      aReason);
  }

  // Invoke the callback with error emssage if something is wrong. Otherwise,
  // remove the SessionInfo record in service since it's closed gracefully.
  if (NS_FAILED(aReason)) {
    if (info->callback) {
      info->callback->NotifyError(NS_LITERAL_STRING("FailedToEstablishSession"));
    }
  } else {
    info->session = nullptr;
    mSessionInfo.Remove(aSession->Id());
  }
}

void
PresentationService::OnSessionMessage(Session* aSession, const nsACString& aMessage)
{
  SessionInfo* info = GetSessionInfo(aSession->Id());
  MOZ_ASSERT(info->listener);

  info->listener->NotifyMessage(aSession->Id(), aMessage);
}
