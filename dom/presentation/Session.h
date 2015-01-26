/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_presentation_Session_h
#define mozilla_dom_presentation_Session_h

#include "nsIPresentationControlChannel.h"
#include "nsIPresentationSessionTransport.h"
#include "nsCOMPtr.h"
#include "nsRefPtr.h"
#include "nsString.h"

class nsIInputStream;

namespace mozilla {
namespace dom {
namespace presentation {

class PresentationService;

class Session : public nsIPresentationSessionTransportCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONSESSIONTRANSPORTCALLBACK

  static already_AddRefed<Session> CreateRequester(const nsAString& aId,
                                                   PresentationService* aService,
                                                   nsIPresentationControlChannel* aControlChannel);
  static already_AddRefed<Session> CreateResponder(const nsAString& aId,
                                                   PresentationService* aService,
                                                   nsIPresentationControlChannel* aControlChannel);

  const nsAString& Id() const {
    return mId;
  }

  void Send(nsIInputStream* aData);
  void Close(nsresult aReason);

protected:
  explicit Session(const nsAString& aId,
                   PresentationService* aService)
    : mId(aId)
    , mService(aService)
  {
  }

  virtual ~Session() {}

  nsCOMPtr<nsIPresentationSessionTransport> mTransport;

private:
  nsString mId;
  nsRefPtr<PresentationService> mService;
};


} // namespace presentation
} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_presentation_Session_h */
