/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/InputStreamUtils.h"
#include "nsIPresentationDeviceManager.h"
#include "nsServiceManagerUtils.h"
#include "PresentationParent.h"
#include "PresentationService.h"

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Presentation", args);
#else
#define LOG(args...)  printf(args);
#endif

using namespace mozilla::dom;
using namespace mozilla::dom::presentation;

NS_IMPL_ISUPPORTS(PresentationParent, nsIPresentationListener, nsIPresentationSessionListener)

PresentationParent::PresentationParent()
  : mActorDestroyed(false)
{
  LOG("[Parent] %s", __FUNCTION__);
  MOZ_COUNT_CTOR(PresentationParent);
}

/* virtual */ PresentationParent::~PresentationParent()
{
  LOG("[Parent] %s", __FUNCTION__);
  MOZ_COUNT_DTOR(PresentationParent);
}

void
PresentationParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOG("[Parent] %s", __FUNCTION__);
  mActorDestroyed = true;
  mService->UnregisterListener(this);
  mService = nullptr;
}

bool
PresentationParent::RecvPPresentationRequestConstructor(
  PPresentationRequestParent* aActor,
  const PresentationRequest& aRequest)
{
  LOG("[Parent] %s", __FUNCTION__);
  PresentationRequestParent* actor = static_cast<PresentationRequestParent*>(aActor);

  nsresult rv = NS_ERROR_FAILURE;
  switch (aRequest.type()) {
    case PresentationRequest::TStartSessionRequest:
      rv = actor->DoRequest(aRequest.get_StartSessionRequest());
      break;
    case PresentationRequest::TJoinSessionRequest:
      rv = actor->DoRequest(aRequest.get_JoinSessionRequest());
      break;
    case PresentationRequest::TSendMessageRequest:
      rv = actor->DoRequest(aRequest.get_SendMessageRequest());
      break;
    case PresentationRequest::TCloseSessionRequest:
      rv = actor->DoRequest(aRequest.get_CloseSessionRequest());
      break;
    default:
      MOZ_CRASH("Unknown PresentationRequest type");
      break;
  }

  return NS_SUCCEEDED(rv);
}

PPresentationRequestParent*
PresentationParent::AllocPPresentationRequestParent(
  const PresentationRequest& aRequest)
{
  LOG("[Parent] %s", __FUNCTION__);
  MOZ_ASSERT(mService);
  nsRefPtr<PresentationRequestParent> actor = new PresentationRequestParent(mService);
  return actor.forget().take();
}

bool
PresentationParent::DeallocPPresentationRequestParent(
  PPresentationRequestParent* aActor)
{
  LOG("[Parent] %s", __FUNCTION__);
  PresentationRequestParent* actor = static_cast<PresentationRequestParent*>(aActor);
  NS_RELEASE(actor);
  return true;
}

bool
PresentationParent::Recv__delete__()
{
  LOG("[Parent] %s", __FUNCTION__);
  return true;
}

bool
PresentationParent::RecvRegisterHandler()
{
  LOG("[Parent] %s", __FUNCTION__);
  MOZ_ASSERT(mService);
  mService->RegisterListener(this);
  return true;
}

bool
PresentationParent::RecvUnregisterHandler()
{
  LOG("[Parent] %s", __FUNCTION__);
  MOZ_ASSERT(mService);
  mService->UnregisterListener(this);
  return true;
}

/* virtual */ bool
PresentationParent::RecvRegisterSessionHandler(const nsString& aSessionId)
{
  LOG("[Parent] %s", __FUNCTION__);
  MOZ_ASSERT(mService);
  mService->RegisterSessionListener(aSessionId, this);
  return true;
}

/* virtual */ bool
PresentationParent::RecvUnregisterSessionHandler(const nsString& aSessionId)
{
  LOG("[Parent] %s", __FUNCTION__);
  MOZ_ASSERT(mService);
  mService->UnregisterSessionListener(aSessionId, this);
  return true;
}

bool
PresentationParent::Init(PresentationService* aService)
{
  LOG("[Parent] %s", __FUNCTION__);
  MOZ_ASSERT(!mService);
  mService = aService;
  return true;
}

NS_IMETHODIMP
PresentationParent::NotifyAvailableChange(bool aAvailable)
{
  LOG("[Parent] %s", __FUNCTION__);
  if (mActorDestroyed || !SendNotifyAvailableChange(aAvailable)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationParent::NotifySessionReady(const nsAString& aId)
{
  LOG("[Parent] %s", __FUNCTION__);
  if (mActorDestroyed || !SendNotifySessionReady(nsString(aId))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationParent::NotifyStateChange(const nsAString& aSessionId,
                                      uint16_t aState,
                                      nsresult aReason)
{
  LOG("[Parent] %s", __FUNCTION__);
  if (mActorDestroyed ||
      !SendNotifySessionStateChange(nsString(aSessionId), aState, aReason)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationParent::NotifyMessage(const nsAString& aSessionId,
                                  const nsACString& aData)
{
  LOG("[Parent] %s", __FUNCTION__);
  if (mActorDestroyed || !SendNotifyMessage(nsString(aSessionId), nsCString(aData))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

// PresentationRequestParent

NS_IMPL_ISUPPORTS(PresentationRequestParent, nsIPresentationRequestCallback)

PresentationRequestParent::PresentationRequestParent(PresentationService* aService)
  : mActorDestroyed(false)
  , mService(aService)
{
  LOG("[RequestParent] %s", __FUNCTION__);
}

PresentationRequestParent::~PresentationRequestParent()
{
  LOG("[RequestParent] %s", __FUNCTION__);
}

void
PresentationRequestParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOG("[RequestParent] ActorDestroy");
  mActorDestroyed = true;
  mService = nullptr;
}

nsresult
PresentationRequestParent::DoRequest(const StartSessionRequest& aRequest)
{
  LOG("[RequestParent] %s, StartSessionRequest", __FUNCTION__);
  MOZ_ASSERT(mService);
  return mService->StartSessionInternal(aRequest.url(), aRequest.sessionId(), aRequest.origin(), this);
}

nsresult
PresentationRequestParent::DoRequest(const JoinSessionRequest& aRequest)
{
  LOG("[RequestParent] %s, JoinSessionRequest", __FUNCTION__);
  MOZ_ASSERT(mService);
  return mService->JoinSessionInternal(aRequest.url(), aRequest.sessionId(), aRequest.origin());
}

nsresult
PresentationRequestParent::DoRequest(const SendMessageRequest& aRequest)
{
  LOG("[RequestParent] %s, SendMessageRequest", __FUNCTION__);
  MOZ_ASSERT(mService);
  nsTArray<mozilla::ipc::FileDescriptor> fds;
  nsCOMPtr<nsIInputStream> stream = DeserializeInputStream(aRequest.data(), fds);
  NS_WARN_IF(!stream);
  if (NS_FAILED(mService->SendMessageInternal(aRequest.sessionId(), stream))) {
    return NotifyError(NS_LITERAL_STRING("SendMessageFailed"));
  }
  return NotifySuccess();
}

nsresult
PresentationRequestParent::DoRequest(const CloseSessionRequest& aRequest)
{
  LOG("[RequestParent] DoRequest, CloseSessionRequest");
  MOZ_ASSERT(mService);
  if (NS_FAILED(mService->CloseSessionInternal(aRequest.sessionId()))) {
    return NotifyError(NS_LITERAL_STRING("CloseSessionFailed"));
  }
  return NotifySuccess();
}

NS_IMETHODIMP
PresentationRequestParent::NotifySuccess()
{
  LOG("[RequestParent] %s", __FUNCTION__);
  return SendResponse(PresentationSuccessResponse());
}

NS_IMETHODIMP
PresentationRequestParent::NotifyError(const nsAString& aError)
{
  LOG("[RequestParent] %s", __FUNCTION__);
  return SendResponse(PresentationErrorResponse(nsString(aError)));
}

nsresult
PresentationRequestParent::SendResponse(const PresentationResponse& aResponse)
{
  LOG("[RequestParent] SendResponse, mActorDestroyed: %d", mActorDestroyed);
  if (mActorDestroyed || !Send__delete__(this, aResponse)) {
    LOG("[RequestParent] actor is destroyed or failed to send delete");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}
