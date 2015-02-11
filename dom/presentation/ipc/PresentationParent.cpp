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

using namespace mozilla::dom;
using namespace mozilla::dom::presentation;

NS_IMPL_ISUPPORTS(PresentationParent, nsIPresentationListener, nsIPresentationSessionListener)

PresentationParent::PresentationParent()
  : mActorDestroyed(false)
{
  MOZ_COUNT_CTOR(PresentationParent);
}

/* virtual */ PresentationParent::~PresentationParent()
{
  MOZ_COUNT_DTOR(PresentationParent);
}

void
PresentationParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mActorDestroyed = true;
  mService->UnregisterListener(this);
  mService = nullptr;
}

bool
PresentationParent::RecvPPresentationRequestConstructor(
  PPresentationRequestParent* aActor,
  const PresentationRequest& aRequest)
{
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
  MOZ_ASSERT(mService);
  nsRefPtr<PresentationRequestParent> actor = new PresentationRequestParent(mService);
  return actor.forget().take();
}

bool
PresentationParent::DeallocPPresentationRequestParent(
  PPresentationRequestParent* aActor)
{
  PresentationRequestParent* actor = static_cast<PresentationRequestParent*>(aActor);
  NS_RELEASE(actor);
  return true;
}

bool
PresentationParent::Recv__delete__()
{
  return true;
}

bool
PresentationParent::RecvRegisterHandler()
{
  MOZ_ASSERT(mService);
  mService->RegisterListener(this);
  return true;
}

bool
PresentationParent::RecvUnregisterHandler()
{
  MOZ_ASSERT(mService);
  mService->UnregisterListener(this);
  return true;
}

/* virtual */ bool
PresentationParent::RecvRegisterSessionHandler(const nsString& aSessionId)
{
  MOZ_ASSERT(mService);
  mService->RegisterSessionListener(aSessionId, this);
  return true;
}

/* virtual */ bool
PresentationParent::RecvUnregisterSessionHandler(const nsString& aSessionId)
{
  MOZ_ASSERT(mService);
  mService->UnregisterSessionListener(aSessionId, this);
  return true;
}

bool
PresentationParent::Init(PresentationService* aService)
{
  MOZ_ASSERT(!mService);
  mService = aService;
  return true;
}

NS_IMETHODIMP
PresentationParent::NotifyAvailableChange(bool aAvailable)
{
  if (mActorDestroyed || !SendNotifyAvailableChange(aAvailable)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationParent::NotifySessionReady(const nsAString& aId)
{
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
}

PresentationRequestParent::~PresentationRequestParent()
{
}

void
PresentationRequestParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mActorDestroyed = true;
  mService = nullptr;
}

nsresult
PresentationRequestParent::DoRequest(const StartSessionRequest& aRequest)
{
  MOZ_ASSERT(mService);
  return mService->StartSessionInternal(aRequest.url(), aRequest.sessionId(), aRequest.origin(), this);
}

nsresult
PresentationRequestParent::DoRequest(const JoinSessionRequest& aRequest)
{
  MOZ_ASSERT(mService);
  return mService->JoinSessionInternal(aRequest.url(), aRequest.sessionId(), aRequest.origin());
}

nsresult
PresentationRequestParent::DoRequest(const SendMessageRequest& aRequest)
{
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
  MOZ_ASSERT(mService);
  if (NS_FAILED(mService->CloseSessionInternal(aRequest.sessionId()))) {
    return NotifyError(NS_LITERAL_STRING("CloseSessionFailed"));
  }
  return NotifySuccess();
}

NS_IMETHODIMP
PresentationRequestParent::NotifySuccess()
{
  return SendResponse(PresentationSuccessResponse());
}

NS_IMETHODIMP
PresentationRequestParent::NotifyError(const nsAString& aError)
{
  return SendResponse(PresentationErrorResponse(nsString(aError)));
}

nsresult
PresentationRequestParent::SendResponse(const PresentationResponse& aResponse)
{
  if (mActorDestroyed || !Send__delete__(this, aResponse)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}
