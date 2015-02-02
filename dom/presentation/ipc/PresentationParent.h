/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_presentation_presentationparent_h__
#define mozilla_dom_presentation_presentationparent_h__

#include "mozilla/dom/presentation/PPresentationParent.h"
#include "mozilla/dom/presentation/PPresentationRequestParent.h"
#include "nsIPresentationRequestCallback.h"

namespace mozilla {
namespace dom {
namespace presentation {

class PresentationService;

class PresentationParent MOZ_FINAL : public PPresentationParent
                                   , public nsIPresentationListener
                                   , public nsIPresentationSessionListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONLISTENER
  NS_DECL_NSIPRESENTATIONSESSIONLISTENER

  PresentationParent();

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvPPresentationRequestConstructor(PPresentationRequestParent* aActor,
                                      const PresentationRequest& aRequest) MOZ_OVERRIDE;

  virtual PPresentationRequestParent*
  AllocPPresentationRequestParent(const PresentationRequest& aRequest) MOZ_OVERRIDE;

  virtual bool
  DeallocPPresentationRequestParent(PPresentationRequestParent* aActor) MOZ_OVERRIDE;

  virtual bool
  Recv__delete__() MOZ_OVERRIDE;

  virtual bool
  RecvRegisterHandler() MOZ_OVERRIDE;

  virtual bool
  RecvUnregisterHandler() MOZ_OVERRIDE;

  virtual bool
  RecvRegisterSessionHandler(const nsString& aSessionId) MOZ_OVERRIDE;

  virtual bool
  RecvUnregisterSessionHandler(const nsString& aSessionId) MOZ_OVERRIDE;

  bool
  Init(PresentationService* aService);

private:
  virtual ~PresentationParent();

  bool mActorDestroyed;
  nsRefPtr<PresentationService> mService;
};

class PresentationRequestParent : public PPresentationRequestParent
                                , public nsIPresentationRequestCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONREQUESTCALLBACK

protected:
  PresentationRequestParent(PresentationService* aService);
  virtual ~PresentationRequestParent();

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  nsresult
  SendResponse(const PresentationResponse& aResponse);

private:
  friend class PresentationParent;

  bool
  DoRequest(const StartSessionRequest& aRequest);

  bool
  DoRequest(const JoinSessionRequest& aRequest);

  bool
  DoRequest(const SendMessageRequest& aRequest);

  bool
  DoRequest(const CloseSessionRequest& aRequest);

  bool mActorDestroyed;
  nsRefPtr<PresentationService> mService;
};

} // namespace presentation
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_presentation_presentationparent_h__
