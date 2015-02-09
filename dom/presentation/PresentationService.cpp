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
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h" // do_GetService
#include "nsXULAppAPI.h" // XRE_GetProcessType
#include "Presentation.h"
#include "PresentationService.h"
#include "PresentationSessionRequest.h"

#include "mozIApplication.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Services.h" // namespace
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/presentation/PresentationIPCService.h"
#include "mozilla/unused.h"

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
    NS_WARN_IF(NS_FAILED(sPresentationService->ReplyCallbackWithError(info, NS_LITERAL_STRING("NoControlChannel"))));
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
  SessionInfo* info;
  if (!sPresentationService->mSessionInfo.Get(mId, &info)) {
    return NS_OK;
  }

  NS_WARN_IF(NS_FAILED(sPresentationService->ReplyCallbackWithError(info, NS_LITERAL_STRING("UserCanceled"))));
  sPresentationService->mSessionInfo.Remove(mId);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(PresentationService::PresentationDeviceRequest,
                  nsIPresentationDeviceRequest)

////////////////////////////////////////////////////////////////////////////////
// PresentationService

NS_IMPL_ISUPPORTS(PresentationService, nsIObserver)

/* static */ already_AddRefed<PresentationService>
PresentationService::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<PresentationService> service;
  if (!sPresentationService) {
    service = PresentationService::Create();
    if (NS_WARN_IF(!service)) {
      return nullptr;
    }
    sPresentationService = service;
    ClearOnShutdown(&sPresentationService);
  } else {
    service = sPresentationService.get();
  }

  return service.forget();
}

/* static */ already_AddRefed<PresentationService>
PresentationService::Create()
{
  nsRefPtr<PresentationService> service;
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    service = PresentationIPCService::Create();
  } else {
    service = new PresentationService();
    if (NS_WARN_IF(!service->Init())) {
      return nullptr;
    }
  }

  if (!service) {
    return nullptr;
  }
  return service.forget();
}

PresentationService::PresentationService()
  : mAvailable(false)
  , mPendingSessionReady(false)
  , mSessionInfo(128)
{
}

/* virtual */ PresentationService::~PresentationService()
{
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

  nsresult rv = deviceManager->GetDeviceAvailable(&mAvailable);
  return (NS_WARN_IF(NS_FAILED(rv))) ? false : true;
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
  obs->RemoveObserver(this, PRESENTATION_SESSION_REQUEST_TOPIC);

  mListeners.Clear();
  mSessionInfo.Clear();

  return NS_OK;
}

void
PresentationService::NotifyAvailableListeners(bool aAvailable)
{
  LOG("[Service] %s", __FUNCTION__);
  if (!mListeners.Length()) {
    LOG("[Service] No listenr is registered.");
  }

  nsTObserverArray<nsCOMPtr<nsIPresentationListener> >::ForwardIterator iter(mListeners);
  while (iter.HasMore()) {
    nsIPresentationListener* listener = iter.GetNext();
    listener->NotifyAvailableChange(aAvailable);
  }
}

void
PresentationService::NotifySessionReady(const nsAString& aId)
{
  LOG("[Service] %s", __FUNCTION__);
  if (!mListeners.Length()) {
    mPendingSessionReady = true;
    mPendingSessionId = aId;
    return;
  }

  nsTObserverArray<nsCOMPtr<nsIPresentationListener> >::ForwardIterator iter(mListeners);
  while (iter.HasMore()) {
    nsIPresentationListener* listener = iter.GetNext();
    listener->NotifySessionReady(aId);
  }
}

nsresult
PresentationService::HandleDeviceChange()
{
  LOG("[Service] %s", __FUNCTION__);
  nsCOMPtr<nsIPresentationDeviceManager> deviceManager =
    do_GetService(PRESENTATION_DEVICE_MANAGER_CONTRACTID);
  if (NS_WARN_IF(!deviceManager)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  bool available;
  nsresult rv = deviceManager->GetDeviceAvailable(&available);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (available != mAvailable) {
    mAvailable = available;
    NotifyAvailableListeners(mAvailable);
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
  NS_WARN_IF(!mSessionInfo.Put(id, info, fallible));

  nsAutoString url;
  request->GetUrl(url);
  if (url.Find("app://") == 0) {
    int32_t offset = url.RFindChar('/');
    if (NS_WARN_IF(offset == kNotFound)) {
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
  } else if (!strcmp(aTopic, "profile-after-change")) {
    return NS_OK;
  }

  MOZ_ASSERT(false, "Unexpected topic for PresentationService");
  return NS_ERROR_UNEXPECTED;
}

bool
PresentationService::FindAppOnDevice(const nsAString& aUrl)
{
  LOG("[Service] %s", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIAppsService> appService = do_GetService("@mozilla.org/AppsService;1");
  if (NS_WARN_IF(!appService)) {
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
PresentationService::ReplyCallbackWithError(SessionInfo* aInfo,
                                            const nsAString& aError)
{
  NS_ENSURE_ARG(aInfo);

  nsresult rv = NS_ERROR_FAILURE;
  if (aInfo->callback) {
    rv = aInfo->callback->NotifyError(aError);
    aInfo->callback = nullptr;
  }

  return rv;
}

nsresult
PresentationService::ReplyCallback(SessionInfo* aInfo)
{
  NS_ENSURE_ARG(aInfo);

  nsresult rv = NS_ERROR_FAILURE;
  if (aInfo->callback) {
    rv = aInfo->callback->NotifySuccess();
    aInfo->callback = nullptr;
  }

  return rv;
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

  SessionInfo* info = mSessionInfo.Get(aSessionId);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}

/* virtual */ nsresult
PresentationService::SendMessageInternal(const nsAString& aSessionId,
                                         nsIInputStream* aStream)
{
  LOG("[Service] %s", __FUNCTION__);
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
  LOG("[Service] %s", __FUNCTION__);

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
  LOG("[Service] %s", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());
  mListeners.AppendElement(aListener);

  if (mPendingSessionReady) {
    mPendingSessionReady = false;
    NotifySessionReady(mPendingSessionId);
    mPendingSessionId.Truncate();
  }
}

/* virtual */ void
PresentationService::UnregisterListener(nsIPresentationListener* aListener)
{
  LOG("[Service] %s", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());
  mListeners.RemoveElement(aListener);
}

/* virtual */ void
PresentationService::RegisterSessionListener(const nsAString& aSessionId,
                                             nsIPresentationSessionListener* aListener)
{
  LOG("[Service] %s", __FUNCTION__);
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
  LOG("[Service] %s", __FUNCTION__);
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
  NS_ENSURE_ARG(aSession);

  SessionInfo* info = mSessionInfo.Get(aSession->Id());
  NS_ENSURE_ARG(info);

  info->isSessionComplete = true;

  // Session transport has been established, so notify the listener of the state
  // change and invoke callback if any.
  nsCOMPtr<nsIPresentationSessionListener> listener = info->listener;
  if (listener) {
    nsresult rv = listener->NotifyStateChange(aSession->Id(),
                                              nsIPresentationSessionListener::STATE_CONNECTED,
                                              NS_OK);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (info->callback) {
    NS_WARN_IF(NS_FAILED(ReplyCallback(info)));
  }

  return NS_OK;
}

nsresult
PresentationService::OnSessionClose(Session* aSession, nsresult aReason)
{
  LOG("[Service] %s", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aSession);

  nsString sessionId(aSession->Id());
  SessionInfo* info = mSessionInfo.Get(sessionId);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // The session is closed, so notify session listeners.
  nsCOMPtr<nsIPresentationSessionListener> listener = info->listener;
  NS_WARN_IF(!listener);
  if (listener) {
    nsresult rv = listener->NotifyStateChange(sessionId,
                                              nsIPresentationSessionListener::STATE_DISCONNECTED,
                                              aReason);
    NS_ENSURE_SUCCESS(rv, rv);

    info->listener = nullptr;
  }

  if (!info->isRequester) {
    // Responder.
    info->session = nullptr;
    info->device = nullptr;
  } else {
    // Requester - remove the session since it hasn't been successfully set up.
    if (info->callback && NS_FAILED(aReason)) {
      NS_WARN_IF(NS_FAILED(ReplyCallbackWithError(info,
                                                  NS_LITERAL_STRING("EstablishSessionFailed"))));
    }
  }

  // remove the session once it gets closed successfully.
  if (NS_SUCCEEDED(aReason)) {
    if (info->device) {
      info->device = nullptr;
    }
    if (info->session) {
      info->session = nullptr;
    }
    mSessionInfo.Remove(aSession->Id());
  }

  return NS_OK;
}

nsresult
PresentationService::OnSessionMessage(Session* aSession, const nsACString& aMessage)
{
  LOG("[Service] %s", __FUNCTION__);
  NS_ENSURE_ARG(aSession);

  SessionInfo* info = mSessionInfo.Get(aSession->Id());
  NS_ENSURE_TRUE(info, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(info->listener, NS_ERROR_NOT_AVAILABLE);

  return info->listener->NotifyMessage(aSession->Id(), aMessage);
}
