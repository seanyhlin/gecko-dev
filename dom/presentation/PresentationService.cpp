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
  const nsAString& aOrigin)
  : mRequestUrl(aUrl)
  , mId(aId)
  , mOrigin(aOrigin)
{
}

PresentationService::PresentationDeviceRequest::~PresentationDeviceRequest()
{
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
  SessionInfo* info;
  if (!sPresentationService->GetSessionInfo(mId, &info)) {
    return NS_OK;
  }
  info->device = aDevice;

  // Establish control channel. If we failed to do so, the callback is called
  // with an error message.
  nsCOMPtr<nsIPresentationControlChannel> ctrlChannel;
  if (NS_FAILED(aDevice->EstablishControlChannel(mRequestUrl, mId,
                                                 getter_AddRefs(ctrlChannel)))) {
    sPresentationService->ReplyRequestWithError(mId,NS_LITERAL_STRING("NoControlChannel"));
    sPresentationService->mSessionInfo.Remove(mId);
  }

   // Create requester and then update SessionInfo.
  info->session = Session::CreateRequester(mId, sPresentationService, ctrlChannel);

  return NS_OK;
}

NS_IMETHODIMP
PresentationService::PresentationDeviceRequest::Cancel()
{
  sPresentationService->ReplyRequestWithError(mId, NS_LITERAL_STRING("UserCanceled"));
  sPresentationService->mSessionInfo.Remove(mId);
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

PresentationService::PresentationService()
  : mAvailable(false)
  , mSessionInfo(128)
  , mLastUniqueId(0)
{
  NS_WARN_IF_FALSE(Init(), "Failed to initialize PresentationService.");
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
nsresult
PresentationService::HandleDeviceChange()
{
  nsCOMPtr<nsIPresentationDeviceManager> deviceManager =
    do_GetService(PRESENTATION_DEVICE_MANAGER_CONTRACTID);
  if (deviceManager) {
    bool available;
    deviceManager->GetDeviceAvailable(&available);
    if (available != mAvailable) {
      NotifyAvailableListeners(available);
    }
  }
  return NS_OK;
}

nsresult
PresentationService::HandleSessionRequest(nsISupports* aSubject)
{
  // Receives a session request on receiver side.
  nsCOMPtr<nsIPresentationSessionRequest> request(do_QueryInterface(aSubject));
  if (NS_WARN_IF(!request)) {
    return NS_OK;
  }

  nsAutoString id;
  nsCOMPtr<nsIPresentationControlChannel> ctrlChannel;
  request->GetPresentationId(id);
  request->GetControlChannel(getter_AddRefs(ctrlChannel));
  nsRefPtr<Session> session = Session::CreateResponder(id, this, ctrlChannel);

  // Create a responder and SessionInfo object
  nsCOMPtr<nsIPresentationDevice> device;
  request->GetDevice(getter_AddRefs(device));
  SessionInfo* info = new SessionInfo(session, device);
  mSessionInfo.Put(id, info);

  nsAutoString url;
  request->GetUrl(url);
  if (!FindAppOnDevice(url)) {
    // nsIPresentationControlChannel::NotifyClosed() is called on server side.
    ctrlChannel->Close(NS_OK);
  } else {
    nsCOMPtr<nsIObserverService> obs = GetObserverService();
    if (obs) {
      obs->NotifyObservers(aSubject, "presentation-launch-receiver", nullptr);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationService::Observe(nsISupports* aSubject,
                             const char* aTopic,
                             const char16_t* aData)
{
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    return HandleShutdown();
  } else if (!strcmp(aTopic, PRESENTATION_DEVICE_CHANGE_TOPIC)) {
    return HandleDeviceChange();
  } else if (!strcmp(aTopic, PRESENTATION_SESSION_REQUEST_TOPIC)) {
    return HandleSessionRequest(aSubject);
  }

  MOZ_ASSERT(false, "Unexpected topic for PresentationService");
  return NS_ERROR_UNEXPECTED;
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

void
PresentationService::ReplyRequestWithError(const nsAString& aId,
                                           const nsAString& aError)
{
  SessionInfo* info;
  if (!GetSessionInfo(aId, &info)) {
    return;
  }

  if (!info->callback) {
    NS_WARNING("Failed to get request callback");
    return;
  }

  info->callback->NotifyError(aError);
  info->callback = nullptr;
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

  if (!mAvailable) {
    aCallback->NotifyError(NS_LITERAL_STRING("NoAvailableDevice"));
    return NS_OK;
  }

  // Create SessionInfo and set SessionInfo.callback. The callback is called
  // when the request is finished.
  SessionInfo* info = new SessionInfo(aCallback);
  mSessionInfo.Put(aId, info);

  // Pop up a prompt and ask user to select a device.
  nsCOMPtr<nsIPresentationDeviceRequest> request =
    new PresentationDeviceRequest(aUrl, aId, aOrigin);

  nsCOMPtr<nsIPresentationDevicePrompt> prompt =
    do_GetService(PRESENTATION_DEVICE_PROMPT_CONTRACTID);
  if (NS_WARN_IF(!prompt)) {
    return NS_ERROR_FAILURE;
  }
  prompt->PromptDeviceSelection(request);

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

bool
PresentationService::GetSessionInfo(const nsAString& aId, SessionInfo** aInfo)
{
  if (!mSessionInfo.Get(aId, aInfo)) {
    NS_WARNING("Failed to get sesison info");
    return false;
  }
  return true;
}

void
PresentationService::OnSessionComplete(Session* aSession)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSession);

  SessionInfo* info;
  if (!GetSessionInfo(aSession->Id(), &info)) {
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
  MOZ_ASSERT(aSession);

  SessionInfo* info;
  if (!GetSessionInfo(aSession->Id(), &info)) {
    return;
  }

  // The session is closed, so notify session listeners.
  if (info->listener) {
    info->listener->NotifyStateChange(aSession->Id(),
                                      (uint16_t)PresentationSessionState::Disconnected,
                                      aReason);
    info->listener = nullptr;
  }

  // Keep the session info
  if (NS_FAILED(aReason)) {
    info->session = nullptr;
    info->callback = nullptr;
    return;
  }

  if (!info->isRequester) {
    info->session = nullptr;
    info->device = nullptr;
    mSessionInfo.Remove(aSession->Id());
  } else {
    MOZ_ASSERT(info->callback);
    ReplyRequestWithError(aSession->Id(), NS_LITERAL_STRING("FailedToEstablishSession"));
  }
  mSessionInfo.Remove(aSession->Id());
}

void
PresentationService::OnSessionMessage(Session* aSession, const nsACString& aMessage)
{
  SessionInfo* info;
  if (!GetSessionInfo(aSession->Id(), &info)) {
    return;
  }
  MOZ_ASSERT(info->listener);

  info->listener->NotifyMessage(aSession->Id(), aMessage);
}
