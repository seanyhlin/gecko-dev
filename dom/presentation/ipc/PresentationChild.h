/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_presentation_PresentationChild_h
#define mozilla_dom_presentation_PresentationChild_h

#include "mozilla/dom/presentation/PPresentationChild.h"
#include "mozilla/dom/presentation/PPresentationRequestChild.h"
#include "nsIPresentationRequestCallback.h"

namespace mozilla {
namespace dom {
namespace presentation {

class PresentationIPCService;

class PresentationChild : public PPresentationChild
{
public:
  PresentationChild(PresentationIPCService* aService);
  virtual ~PresentationChild();

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual PPresentationRequestChild*
  AllocPPresentationRequestChild(const PresentationRequest& aRequest) MOZ_OVERRIDE;

  virtual bool
  DeallocPPresentationRequestChild(PPresentationRequestChild* aActor) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyAvailableChange(const bool& aAvailable) MOZ_OVERRIDE;

  virtual bool
  RecvNotifySessionReady(const nsString& aId) MOZ_OVERRIDE;

  virtual bool
  RecvNotifySessionStateChange(const nsString& aSessionId,
                               const uint16_t& aState,
                               const nsresult& aReason) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyMessage(const nsString& aSessionId,
                    const nsCString& aData) MOZ_OVERRIDE;

private:
  bool mActorDestroyed;
};

class PresentationRequestChild : public PPresentationRequestChild
{
public:
  PresentationRequestChild(nsIPresentationRequestCallback* aCallback);

protected:
  virtual ~PresentationRequestChild();

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  Recv__delete__(const PresentationResponse& aResponse) MOZ_OVERRIDE;

private:
  friend class PresentationChild;
  
  bool
  DoResponse(const PresentationErrorResponse& aResponse);
  
  bool
  DoResponse(const PresentationSuccessResponse& aResponse);

  nsCOMPtr<nsIPresentationRequestCallback> mCallback;
};

} // namespace presentation
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_presentation_PresentationChild_h
