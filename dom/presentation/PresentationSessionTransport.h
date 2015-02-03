/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_presentation_PresentationSessionTransport_h__
#define mozilla_dom_presentation_PresentationSessionTransport_h__

#include "nsCOMPtr.h"
#include "nsITransport.h"
#include "nsIAsyncInputStream.h"
#include "nsIStreamListener.h"
#include "nsRefPtr.h"
#include "nsIPresentationSessionTransport.h"

class nsIPresentationChannelDescription;
class nsISocketTransport;
class nsIMultiplexInputStream;
class nsIInputStream;
class nsIOutputStream;

namespace mozilla {
namespace dom {
namespace presentation {

class PresentationSessionTransport MOZ_FINAL : public nsIPresentationSessionTransport
                                             , public nsITransportEventSink
                                             , public nsIInputStreamCallback
                                             , public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONSESSIONTRANSPORT
  NS_DECL_NSITRANSPORTEVENTSINK
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  // for receiver
  explicit PresentationSessionTransport(nsISocketTransport* aTransport,
                                        nsIPresentationSessionTransportCallback* aCallback);

  // for sender
  explicit PresentationSessionTransport(nsISocketTransport* aTransport,
                                        nsIPresentationSessionTransportCallback* aCallback,
                                        bool aDummy);

private:
  virtual ~PresentationSessionTransport();
  void Init();

  nsCOMPtr<nsISocketTransport> mTransport;
  nsRefPtr<nsIPresentationSessionTransportCallback> mCallback;
  nsCOMPtr<nsIAsyncInputStream> mInputStream;
  nsCOMPtr<nsIOutputStream> mOutputStream;
  nsCOMPtr<nsIMultiplexInputStream> mMultiplexStream;
};

} // namespace presentation
} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_presentation_PresentationSessionTransport_h__ */
