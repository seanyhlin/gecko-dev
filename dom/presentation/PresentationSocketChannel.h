/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationSocketChannel_h
#define mozilla_dom_PresentationSocketChannel_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/PresentationSession.h"
#include "mozilla/dom/PresentationSocketChannelBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/ErrorResult.h"
#include "nsCOMPtr.h" 
#include "nsCycleCollectionParticipant.h"
#include "nsIPresentationSessionTransport.h"
#include "nsWrapperCache.h"

#include "PresentationCommon.h"

struct JSContext;
class nsIInputStream;

BEGIN_PRESENTATION_NAMESPACE

class File;

class PresentationSocketChannel MOZ_FINAL : public DOMEventTargetHelper
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PresentationSocketChannel,
                                           DOMEventTargetHelper)
  PresentationSocketChannel(nsPIDOMWindow* aWindow,
                            PresentationSession* aSession);

  nsresult
  NotifyMessage(const nsACString& aData, bool aIsBinary);

  void
  NotifyOpen();

  void
  NotifyClose();

  void
  NotifyError();

  // WebIDL
  nsPIDOMWindow*
  GetParentObject() const { return GetOwner(); }
  // WebIDL
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;
  // WebIDL
  void
  Send(const nsAString& aData, ErrorResult& aRv);
  void
  Send(File& aData, ErrorResult& aRv);
  void
  Send(const ArrayBuffer& aData, ErrorResult& aRv);
  void
  Send(const ArrayBufferView& aData, ErrorResult& aRv);
  // WebIDL
  void
  Close();
  // WebIDL
  PresentationBinaryType
  BinaryType() const { return mPresentationBinaryType; }
  // WebIDL
  void
  SetBinaryType(PresentationBinaryType aData) { mPresentationBinaryType = aData; }
  // WebIDL
  uint32_t
  BufferedAmount() const { return mOutgoingBufferAmount; }
  // WebIDL
  uint16_t
  ReadyState() const { return mReadyState; }
  // WebIDL
  IMPL_EVENT_HANDLER(message);
  IMPL_EVENT_HANDLER(open);
  IMPL_EVENT_HANDLER(error);
  IMPL_EVENT_HANDLER(close);

private:
  ~PresentationSocketChannel();
  void SetReadyState(EChannelState aChannelState);
  void SendMessageCombo(nsIInputStream* aMsgStream,
                        const nsACString& aMsgString,
                        uint32_t aMsgLength,
                        bool aIsBinary,
                        ErrorResult& aRv);

  nsRefPtr<PresentationSession> mSession;
  PresentationBinaryType mPresentationBinaryType;
  uint32_t mOutgoingBufferAmount;
  uint16_t mReadyState;

  bool mActiveClosing;
};

END_PRESENTATION_NAMESPACE

#endif // mozilla_dom_PresentationSocketChannel_h
