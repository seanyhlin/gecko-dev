/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MainThreadUtils.h" // NS_IsMainThread
#include "nsCRTGlue.h" // NS_strcmp
#include "nsIAppsService.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMTCPSocket.h"
#include "nsIObserverService.h"
#include "nsIPresentationControlChannel.h"
#include "nsIPresentationDeviceManager.h" // Topic
#include "nsITimer.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h" // do_GetService
#include "nsXULAppAPI.h" // XRE_GetProcessType
#include "Presentation.h"
#include "PresentationService.h"
#include "PresentationSessionRequest.h"

#include "mozIApplication.h"
#include "mozilla/Services.h" // namespace
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/presentation/PresentationIPCService.h"
#include "mozilla/unused.h"

#include "FakePresentationDevice.h"
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Presentation", args);
#else
#define LOG(args...)  printf(args);
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
  LOG("[Service] %s", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aDevice);

  // Update SesisonInfo.device when user selects a device.
  SessionInfo* info;
  if (!sPresentationService->mSessionInfo.Get(mId, &info)) {
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
    return NS_OK;
  }

  // Create requester and then update SessionInfo.
  info->session = Session::CreateRequester(mId, sPresentationService, ctrlChannel);

  return NS_OK;
}

NS_IMETHODIMP
PresentationService::PresentationDeviceRequest::Cancel()
{
  LOG("[Service] %s", __FUNCTION__);
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

  return new PresentationService();
}

PresentationService::PresentationService()
  : mAvailable(false)
  , mSessionInfo(128)
{
  NS_WARN_IF_FALSE(Init(), "Failed to initialize PresentationService.");
}

/* virtual */ PresentationService::~PresentationService()
{
  mListeners.Clear();
  mSessionInfo.Clear();
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
  LOG("[Service] %s", __FUNCTION__);

  // Receives a session request on receiver side.
  nsCOMPtr<nsIPresentationSessionRequest> request(do_QueryInterface(aSubject));
  if (NS_WARN_IF(!request)) {
    return NS_OK;
  }

  nsAutoString id;
  nsCOMPtr<nsIPresentationControlChannel> ctrlChannel;
  request->GetPresentationId(id);
  request->GetControlChannel(getter_AddRefs(ctrlChannel));
  MOZ_ASSERT(ctrlChannel);
  nsRefPtr<Session> session = Session::CreateResponder(id, this, ctrlChannel);

  // Create a responder and SessionInfo object
  nsCOMPtr<nsIPresentationDevice> device;
  request->GetDevice(getter_AddRefs(device));
  SessionInfo* info = new SessionInfo(session, device);

  const fallible_t fallible = fallible_t();
  MOZ_ASSERT(mSessionInfo.Put(id, info, fallible));

  nsAutoString url;
  request->GetUrl(url);
  if (url.Find("app://") == 0) {
    int32_t offset = url.RFindChar('/');
    if (offset == kNotFound) {
      ctrlChannel->Close(NS_ERROR_UNEXPECTED);
      return NS_OK;
    }
    url = Substring(url, 0, offset);
    url.AppendLiteral("/manifest.webapp");

    if (!FindAppOnDevice(url)) {
      ctrlChannel->Close(NS_ERROR_UNEXPECTED);
      return NS_OK;
    }
  }

  nsCOMPtr<nsIObserverService> obs = GetObserverService();
  if (obs) {
    obs->NotifyObservers(aSubject, "presentation-launch-receiver", nullptr);
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

nsresult
PresentationService::ReplyRequestWithError(const nsAString& aId,
                                           const nsAString& aError)
{
  printf("ReplyRequestWithError\n");
  SessionInfo* info = mSessionInfo.Get(aId);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (NS_WARN_IF(!info->callback) ||
      NS_WARN_IF(NS_FAILED(info->callback->NotifyError(aError)))) {
    return NS_ERROR_FAILURE;
  }

  info->callback = nullptr;
  return NS_OK;
}

/* virtual */ nsresult
PresentationService::StartSessionInternal(const nsAString& aUrl,
                                          const nsAString& aId,
                                          const nsAString& aOrigin,
                                          nsIPresentationRequestCallback* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCallback);
  LOG("[Service] %s", __FUNCTION__);

  // HACK
/*  if (!mAvailable) {
    aCallback->NotifyError(NS_LITERAL_STRING("NoAvailableDevice"));
    return NS_OK;
  }*/

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

  SessionInfo* info = mSessionInfo.Get(aSessionId);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

/* virtual */ nsresult
PresentationService::SendMessageInternal(const nsAString& aSessionId,
                                         nsIInputStream* aStream)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aSessionId.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }
  NS_ENSURE_ARG(aStream);

  SessionInfo* info = mSessionInfo.Get(aSessionId);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  NS_ENSURE_TRUE(info, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(info->session, NS_ERROR_NOT_AVAILABLE);

  return info->session->Send(aStream);
}

/* virtual */ nsresult
PresentationService::CloseSessionInternal(const nsAString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aSessionId.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  SessionInfo* info = mSessionInfo.Get(aSessionId);
  NS_ENSURE_TRUE(info, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(info->session, NS_ERROR_NOT_AVAILABLE);

  return info->session->Close(NS_OK);
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

/* virtual */ void
PresentationService::RegisterSessionListener(const nsAString& aSessionId,
                                             nsIPresentationSessionListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_WARN_IF(!aListener);

  SessionInfo* info = mSessionInfo.Get(aSessionId);
  if (NS_WARN_IF(!info)) {
    return;
  }

  info->listener = aListener;
  NS_ENSURE_TRUE_VOID(info->listener);

  if (info->isSessionComplete) {
    // The transport might become ready before the listener has registered, so
    // notify the listener of the state change.
    nsresult rv = aListener->NotifyStateChange(aSessionId,
                                               nsIPresentationSessionListener::STATE_CONNECTED,
                                               NS_OK);
    NS_ENSURE_SUCCESS_VOID(rv);
  }
}

/* virtual */ void
PresentationService::UnregisterSessionListener(const nsAString& aSessionId,
                                               nsIPresentationSessionListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());

  SessionInfo* info = mSessionInfo.Get(aSessionId);
  if (NS_WARN_IF(!info)) {
    return;
  }

  info->listener = nullptr;
}

nsresult
PresentationService::OnSessionComplete(Session* aSession)
{
  LOG("[Service] %s", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(aSession);

  SessionInfo* info = mSessionInfo.Get(aSession->Id());
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  info->isSessionComplete = true;

  // Session transport has been established, so notify the listener of the state
  // change and invoke callback if any.
  nsCOMPtr<nsIPresentationSessionListener> listener = info->listener;
  NS_WARN_IF(!listener);
  if (listener) {
    nsresult rv = listener->NotifyStateChange(aSession->Id(),
                                              nsIPresentationSessionListener::STATE_CONNECTED,
                                              NS_OK);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (info->callback) {
    return info->callback->NotifySuccess();
  }

  return NS_OK;
}

nsresult
PresentationService::OnSessionClose(Session* aSession, nsresult aReason)
{
  LOG("[Service] %s", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(aSession);

  SessionInfo* info = mSessionInfo.Get(aSession->Id());
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // The session is closed, so notify session listeners.
  nsCOMPtr<nsIPresentationSessionListener> listener = info->listener;
  NS_WARN_IF(!listener);
  if (listener) {
    nsresult rv = listener->NotifyStateChange(aSession->Id(),
                                              nsIPresentationSessionListener::STATE_DISCONNECTED,
                                              aReason);
    NS_ENSURE_SUCCESS(rv, rv);

    info->listener = nullptr;
  }

  if (!info->isRequester) {
    info->session = nullptr;
    info->device = nullptr;
  } else if (info->callback) {
    ReplyRequestWithError(aSession->Id(), NS_LITERAL_STRING("FailedToEstablishSession"));
    info->callback = nullptr;
  }

  if (NS_SUCCEEDED(aReason)) {
    mSessionInfo.Remove(aSession->Id());
  }

  return NS_OK;
}

nsresult
PresentationService::OnSessionMessage(Session* aSession, const nsACString& aMessage)
{
  NS_ENSURE_ARG_POINTER(aSession);

  SessionInfo* info = mSessionInfo.Get(aSession->Id());
  NS_ENSURE_TRUE(info, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(info->listener, NS_ERROR_NOT_AVAILABLE);

  return info->listener->NotifyMessage(aSession->Id(), aMessage);
}
