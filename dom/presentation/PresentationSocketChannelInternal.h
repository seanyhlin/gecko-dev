/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationSocketChannelInternal_h__
#define mozilla_dom_PresentationSocketChannelInternal_h__

#include "nsCycleCollectionParticipant.h"
#include "nsIPresentationSessionTransport.h"
#include "PresentationCommon.h"

BEGIN_PRESENTATION_NAMESPACE

class PresentationSessionInternal;

class PresentationSocketChannelInternal MOZ_FINAL
  : public nsIPresentationSessionTransportListener
{
public:
  class NotifyCloseRunnable MOZ_FINAL : public nsRunnable
  {
  public:
    NotifyCloseRunnable(PresentationSocketChannelInternal* aPSCInternal)
      : mPSCInternal(aPSCInternal)
    {
    }
    virtual ~NotifyCloseRunnable() {}
    NS_IMETHOD
    Run() {
      if (mPSCInternal) {
        mPSCInternal->NotifyClosed(NS_OK);
      }
      return NS_OK;
    }
  private:
    PresentationSocketChannelInternal* mPSCInternal;
  };

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(PresentationSocketChannelInternal,
                                           nsIPresentationSessionTransportListener)
  NS_DECL_NSIPRESENTATIONSESSIONTRANSPORTLISTENER
  PresentationSocketChannelInternal();

  nsresult
  SetupSocketChannel(PresentationSessionInternal* aSession,
                     nsIPresentationSessionTransport* aPSTransport);

  void
  TeardownSocketChannel();

  nsresult
  PostMessage(const nsACString& aMsg, bool aIsBinary);

private:
  virtual ~PresentationSocketChannelInternal();

  nsCOMPtr<nsIPresentationSessionTransport> mPSTransport;
  nsRefPtr<PresentationSessionInternal> mSessionInternal;
  bool mIsTearingdown;
};

END_PRESENTATION_NAMESPACE

#endif
