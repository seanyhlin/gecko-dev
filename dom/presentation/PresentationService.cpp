/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MainThreadUtils.h" // NS_IsMainThread
#include "nsCRTGlue.h" // NS_strcmp
#include "nsIAppsService.h"
#include "nsIArray.h"
#include "nsIDOMTCPSocket.h"
#include "nsIObserverService.h"
#include "nsIPresentationControlChannel.h"
#include "nsIPresentationDevice.h" // nsIPresentationDevice
#include "nsIPresentationDeviceManager.h" // Topic
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h" // do_GetService
#include "nsString.h"
#include "nsXULAppAPI.h" // XRE_GetProcessType
#include "NavigatorPresentation.h"
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
// PresentationDeviceRequest

PresentationService::PresentationDeviceRequest::PresentationDeviceRequest(
  const nsAString& aUrl,
  const nsAString& aId,
  const nsAString& aOrigin,
  nsIPresentationServiceCallback* aCallback)
  : mRequestUrl(aUrl)
  , mId(aId)
  , mOrigin(aOrigin)
  , mCallback(aCallback)
{
  MOZ_ASSERT(mCallback);
}

PresentationService::PresentationDeviceRequest::~PresentationDeviceRequest()
{
  mCallback = nullptr;
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
  MOZ_ASSERT(aDevice);

  nsCOMPtr<nsIPresentationControlChannel> ctrlChannel;
  nsresult rv = aDevice->EstablishControlChannel(mRequestUrl, mId,
                                                 getter_AddRefs(ctrlChannel));
  if (NS_FAILED(rv)) {
    mCallback->NotifyError(NS_LITERAL_STRING("NoControlChannel"));
    return NS_OK;
  }

  sPresentationService->mRequester =
    new SessionRequester(mId, aDevice, ctrlChannel, sPresentationService);

//  SessionInfo* info = new SessionInfo(aDevice, transport);
//  sPresentationService->AddSessionInfo(mId, info);

  return NS_OK;
}

NS_IMETHODIMP
PresentationService::PresentationDeviceRequest::Cancel()
{
  if (mCallback) {
    mCallback->NotifyError(NS_LITERAL_STRING("UserCanceled"));
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(PresentationService::PresentationDeviceRequest,
                  nsIPresentationDeviceRequest) 

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

// HACK
class Callback : public nsITimerCallback
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
};

NS_IMETHODIMP
Callback::Notify(nsITimer* aTimer)
{
  LOG("Callback::Notify\n");
  sPresentationService->NotifyAvailableListeners(true);
  return NS_OK;
};

NS_IMPL_ISUPPORTS(Callback, nsITimerCallback)

PresentationService::PresentationService()
  : mAvailable(false)
  , mSessionInfo(128)
  , mLastUniqueId(0)
  , mRequester(nullptr)
  , mResponder(nullptr)
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

    nsAutoString id, url;
    nsCOMPtr<nsIPresentationControlChannel> ctrlChannel;
    nsCOMPtr<nsIPresentationDevice> device;
    request->GetPresentationId(id);
    request->GetUrl(url);
    request->GetControlChannel(getter_AddRefs(ctrlChannel));
    request->GetDevice(getter_AddRefs(device));

    mResponder = new SessionResponder(id, device, ctrlChannel, sPresentationService);

    if (!FindAppOnDevice(url)) {
      // On Sender side, PresentationService::OnSessionFailure() should be called.
      ctrlChannel->Close();
    } else {
      nsCOMPtr<nsIObserverService> obs = GetObserverService();
      if (!obs) {
        return NS_OK;
      }

      obs->NotifyObservers(nullptr, "presentation-launch-receiver", url.get());
    }
  } else if (!strcmp(aTopic, "presentation-receiver-launched")) {
    nsCOMPtr<nsIFrameLoader> frameLoader = do_QueryInterface(aSubject);
    if (NS_WARN_IF(!frameLoader)) {
      return NS_OK;
    }
    nsresult rv =
      frameLoader->ActivateFrameEvent(NS_LITERAL_STRING("DOMContentLoaded"),
                                      false);
    // AddEventListener
    mozilla::unused << NS_WARN_IF(NS_FAILED(rv));
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
                                          nsIPresentationServiceCallback* aCallback)
{
  LOG("StartSessionInternal\n");
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCallback);

  // Pop up a prompt and ask user to select a device.
  nsCOMPtr<nsIPresentationDeviceRequest> request =
    new PresentationDeviceRequest(aUrl, aId, aOrigin, aCallback);

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

/* static */ bool
PresentationService::AddSessionInfo(const nsAString& aKey,
                                    SessionInfo* aInfo)
{
  mSessionInfo.Put(aKey, aInfo);
  return true;
}

PresentationService::SessionInfo*
PresentationService::GetSessionInfo(const nsAString& aUrl,
                                    const nsAString& aId)
{
//  nsAutoString key = GenerateUniqueId(aUrl, aId);
//  nsString key = aUrl;
  SessionInfo* info;
  if (!mSessionInfo.Get(aUrl, &info)) {
    return nullptr;
  }
  return info;
}

void
PresentationService::OnSessionComplete(SessionTransport* aTransport)
{

}

void
PresentationService::OnSessionFailure()
{

}

void
PresentationService::OnReceiverReady()
{

}
