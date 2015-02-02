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
  mActorDestroyed = false;
  mService = nullptr;
}

bool
PresentationParent::RecvPPresentationRequestConstructor(
  PPresentationRequestParent* aActor,
  const PresentationRequest& aRequest)
{
  PresentationRequestParent* actor = static_cast<PresentationRequestParent*>(aActor);

  switch (aRequest.type()) {
    case PresentationRequest::TStartSessionRequest:
      return actor->DoRequest(aRequest.get_StartSessionRequest());
    case PresentationRequest::TJoinSessionRequest:
      return actor->DoRequest(aRequest.get_JoinSessionRequest());
    case PresentationRequest::TSendMessageRequest:
      return actor->DoRequest(aRequest.get_SendMessageRequest());
    case PresentationRequest::TCloseSessionRequest:
      return actor->DoRequest(aRequest.get_CloseSessionRequest());
    default:
      MOZ_CRASH("Unknown PresentationRequest type");
      break;
  }

  return false;
}

PPresentationRequestParent*
PresentationParent::AllocPPresentationRequestParent(
  const PresentationRequest& aRequest)
{
  PresentationRequestParent* actor = new PresentationRequestParent(mService);
  NS_ADDREF(actor);
  return actor;
}

bool
PresentationParent::DeallocPPresentationRequestParent(
  PPresentationRequestParent* aActor)
{
  static_cast<PresentationRequestParent*>(aActor)->Release();
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

bool
PresentationRequestParent::DoRequest(const StartSessionRequest& aRequest)
{
  MOZ_ASSERT(mService);
  nsresult rv = mService->StartSessionInternal(aRequest.url(), aRequest.sessionId(), aRequest.origin(), this);
  return NS_SUCCEEDED(rv);
}

bool
PresentationRequestParent::DoRequest(const JoinSessionRequest& aRequest)
{
  MOZ_ASSERT(mService);
  nsresult rv = mService->JoinSessionInternal(aRequest.url(), aRequest.sessionId(), aRequest.origin());
  return NS_SUCCEEDED(rv);
}

bool
PresentationRequestParent::DoRequest(const SendMessageRequest& aRequest)
{
  MOZ_ASSERT(mService);
  nsTArray<mozilla::ipc::FileDescriptor> fds;
  nsCOMPtr<nsIInputStream> stream = DeserializeInputStream(aRequest.data(), fds);
  NS_WARN_IF(!stream);

  nsresult rv = mService->SendMessageInternal(aRequest.sessionId(), stream);
  return NS_SUCCEEDED(rv);
}

bool
PresentationRequestParent::DoRequest(const CloseSessionRequest& aRequest)
{
  MOZ_ASSERT(mService);
  nsresult rv = mService->CloseSessionInternal(aRequest.sessionId());
  return NS_SUCCEEDED(rv);
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
  if (mActorDestroyed) {
    return NS_ERROR_FAILURE;
  }

  return Send__delete__(this, aResponse) ? NS_OK : NS_ERROR_FAILURE;
}
