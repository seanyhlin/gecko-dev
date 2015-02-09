/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StaticPtr.h"
#include "PresentationChild.h"
#include "PresentationIPCService.h"
#include "nsThreadUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::presentation;

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Presentation", args);
#else
#define LOG(args...)  printf(args);
#endif

PresentationChild::PresentationChild(PresentationIPCService* aService)
  : mActorDestroyed(false)
  , mService(aService)
{
  LOG("[Child] %s", __FUNCTION__);
  MOZ_COUNT_CTOR(PresentationChild);
}

PresentationChild::~PresentationChild()
{
  LOG("[Child] %s", __FUNCTION__);
  MOZ_COUNT_DTOR(PresentationChild);

  if (!mActorDestroyed) {
    Send__delete__(this);
  }
  mService = nullptr;
}

void
PresentationChild::ActorDestroy(ActorDestroyReason aWhy)
{
  LOG("[Child] %s", __FUNCTION__);
  mActorDestroyed = true;
  mService->NotifyDeadActor();
  mService = nullptr;
}

PPresentationRequestChild*
PresentationChild::AllocPPresentationRequestChild(const PresentationRequest& aRequest)
{
  LOG("[Child] %s", __FUNCTION__);
  MOZ_CRASH("");
  return nullptr;
}

bool
PresentationChild::DeallocPPresentationRequestChild(PPresentationRequestChild* aActor)
{
  LOG("[Child] %s", __FUNCTION__);
  delete aActor;
  return true;
}

bool
PresentationChild::RecvNotifyAvailableChange(const bool& aAvailable)
{
  LOG("[Child] %s", __FUNCTION__);
  MOZ_ASSERT(mService);
  mService->NotifyAvailableListeners(aAvailable);

  return true;
}

bool
PresentationChild::RecvNotifySessionReady(const nsString& aId)
{
  LOG("[Child] %s", __FUNCTION__);
  MOZ_ASSERT(mService);
  mService->NotifySessionReady(nsString(aId));

  return true;
}

bool
PresentationChild::RecvNotifySessionStateChange(const nsString& aSessionId,
                                                const uint16_t& aState,
                                                const nsresult& aReason)
{
  LOG("[Child] %s", __FUNCTION__);
  MOZ_ASSERT(mService);
  if (mService) {
    mService->NotifySessionStateChange(aSessionId, aState, aReason);
  }

  return true;
}

bool
PresentationChild::RecvNotifyMessage(const nsString& aSessionId,
                                     const nsCString& aData)
{
  LOG("[Child] %s", __FUNCTION__);
  MOZ_ASSERT(mService);
  if (mService) {
    mService->NotifyMessage(aSessionId, aData);
  }

  return true;
}

// PresentationRequestChild

PresentationRequestChild::PresentationRequestChild(nsIPresentationRequestCallback* aCallback)
  : mCallback(aCallback)
{
  LOG("[RequestChild] %s", __FUNCTION__);
}

PresentationRequestChild::~PresentationRequestChild()
{
  LOG("[RequestChild] %s", __FUNCTION__);
  mCallback = nullptr;
}

void
PresentationRequestChild::ActorDestroy(ActorDestroyReason aWhy)
{
  LOG("[RequestChild] %s", __FUNCTION__);
  mCallback = nullptr;
}

bool
PresentationRequestChild::Recv__delete__(const PresentationResponse& aResponse)
{
  LOG("[RequestChild] %s", __FUNCTION__);
  switch (aResponse.type()) {
    case PresentationResponse::TPresentationSuccessResponse:
      return DoResponse(aResponse.get_PresentationSuccessResponse());
    case PresentationResponse::TPresentationErrorResponse:
      return DoResponse(aResponse.get_PresentationErrorResponse());
    default:
      MOZ_CRASH("Unknown type!");
  }
  return true;
}

bool
PresentationRequestChild::DoResponse(const PresentationSuccessResponse& aResponse)
{
  LOG("[RequestChild] %s, PresentationSuccessResponse", __FUNCTION__);
  if (mCallback) {
    mCallback->NotifySuccess();
  }
  return true;
}

bool
PresentationRequestChild::DoResponse(const PresentationErrorResponse& aResponse)
{
  LOG("[RequestChild] %s, PresentationErrorResponse", __FUNCTION__);
  if (mCallback) {
    mCallback->NotifyError(aResponse.error());
  }
  return true;
}
