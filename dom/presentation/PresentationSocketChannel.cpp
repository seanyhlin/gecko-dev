/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/PresentationSocketChannel.h"
#include "mozilla/dom/PresentationSocketChannelBinding.h"

#include "nsContentUtils.h"
#include "nsIDOMMessageEvent.h"
#include "nsIInputStream.h"

BEGIN_PRESENTATION_NAMESPACE

NS_IMPL_ADDREF_INHERITED(PresentationSocketChannel, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(PresentationSocketChannel, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PresentationSocketChannel)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(PresentationSocketChannel)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PresentationSocketChannel,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSession)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PresentationSocketChannel,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSession)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

PresentationSocketChannel::
  PresentationSocketChannel(nsPIDOMWindow* aWindow,
                            PresentationSession* aSession)
  : DOMEventTargetHelper(aWindow)
  , mSession(aSession)
  , mReadyState(EChannelState::CONNECTING)
  , mActiveClosing(false)
{
}

PresentationSocketChannel::~PresentationSocketChannel()
{
  mSession = nullptr;
}

JSObject*
PresentationSocketChannel::WrapObject(JSContext* aCx)
{
  return PresentationSocketChannelBinding::Wrap(aCx, this);
}

void
PresentationSocketChannel::SetReadyState(EChannelState aChannelState)
{
  mReadyState = (uint16_t) aChannelState;
}

void
PresentationSocketChannel::Send(const nsAString& aData, ErrorResult& aRv)
{
  MOZ_ASSERT(mSession, " mSession should exist !");
  NS_ConvertUTF16toUTF8 msgString(aData);
  SendMessageCombo(nullptr, msgString, msgString.Length(), false, aRv);
}
void
PresentationSocketChannel::Send(File& aData, ErrorResult& aRv)
{
  MOZ_ASSERT(mSession, " mSession should exist !");
  nsCOMPtr<nsIInputStream> msgStream;
  nsresult rv = aData.GetInternalStream(getter_AddRefs(msgStream));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  uint64_t msgLength;
  rv = aData.GetSize(&msgLength);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  if (msgLength > UINT32_MAX) {
    aRv.Throw(NS_ERROR_FILE_TOO_BIG);
    return;
  }
  SendMessageCombo(msgStream, EmptyCString(), msgLength, true, aRv);
}
void
PresentationSocketChannel::Send(const ArrayBuffer& aData, ErrorResult& aRv)
{
  MOZ_ASSERT(mSession, " mSession should exist !");
  aData.ComputeLengthAndData();
  static_assert(sizeof(*aData.Data()) == 1, "byte-sized data required");
  uint32_t len = aData.Length();
  char* data = reinterpret_cast<char*>(aData.Data());
  nsDependentCSubstring msgString(data, len);
  SendMessageCombo(nullptr, msgString, len, true, aRv);
}

void
PresentationSocketChannel::Send(const ArrayBufferView& aData, ErrorResult& aRv)
{
  MOZ_ASSERT(mSession, " mSession should exist !");
  aData.ComputeLengthAndData();
  static_assert(sizeof(*aData.Data()) == 1, "byte-sized data required");
  uint32_t len = aData.Length();
  char* data = reinterpret_cast<char*>(aData.Data());
  nsDependentCSubstring msgString(data, len);

  SendMessageCombo(nullptr, msgString, len, true, aRv);
}

void
PresentationSocketChannel::SendMessageCombo(nsIInputStream* aMsgStream,
                                            const nsACString& aMsgString,
                                            uint32_t aMsgLength,
                                            bool aIsBinary,
                                            ErrorResult& aRv)
{
  int64_t readyState = ReadyState();
  if (readyState == EChannelState::CONNECTING) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (readyState == CLOSING ||
      readyState == CLOSED) {
    return;
  }

  if (aMsgStream) {
    // TODO : (Blob stream) Not implemented yet.
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  } else {
    mSession->PostMessage(aMsgString, aIsBinary);
  }
}

void
PresentationSocketChannel::Close()
{
  MOZ_ASSERT(mSession, " mSession should exist !");
  uint16_t readyState = ReadyState();
  if (readyState == EChannelState::CLOSING ||
      readyState == EChannelState::CLOSED) {
    return;
  }

  mActiveClosing = true;
  SetReadyState(EChannelState::CLOSING);
  mSession->CloseSessionConnection();
}

nsresult
PresentationSocketChannel::NotifyMessage(const nsACString& aData, bool aIsBinary)
{
  MOZ_ASSERT(NS_IsMainThread(), "Not running on main thread");

  int16_t readyState = ReadyState();
  if (readyState == EChannelState::CLOSED) {
    NS_ERROR("Received message after CLOSED");
    return NS_ERROR_UNEXPECTED;
  }

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(GetOwner()))) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = NS_OK;
  JSContext* aCx = jsapi.cx();
  JS::Rooted<JS::Value> jsData(aCx);

  if (aIsBinary) {
    if (mPresentationBinaryType == dom::PresentationBinaryType::Blob) {
      nsContentUtils::CreateBlobBuffer(aCx, GetOwner(), aData, &jsData);
      NS_ENSURE_SUCCESS(rv, rv);
    } else if (mPresentationBinaryType == dom::PresentationBinaryType::Arraybuffer) {
      JS::Rooted<JSObject*> arrayBuf(aCx);
      rv = nsContentUtils::CreateArrayBuffer(aCx, aData, arrayBuf.address());
      NS_ENSURE_SUCCESS(rv, rv);
      jsData = OBJECT_TO_JSVAL(arrayBuf);
    } else {
      NS_RUNTIMEABORT("Unknown PresentationBinaryType type!");
      return NS_ERROR_UNEXPECTED;
    }
  } else {
    NS_ConvertUTF8toUTF16 utf16Data(aData);
    JSString* jsString;
    jsString = JS_NewUCStringCopyN(aCx, utf16Data.get(), utf16Data.Length());
    NS_ENSURE_TRUE(jsString, NS_ERROR_FAILURE);

    jsData = STRING_TO_JSVAL(jsString);
  }
  nsCOMPtr<nsIDOMEvent> event;
  rv = NS_NewDOMMessageEvent(getter_AddRefs(event), this, nullptr, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMMessageEvent> messageEvent = do_QueryInterface(event);
  rv = messageEvent->InitMessageEvent(NS_LITERAL_STRING("message"),
                                      false, false,
                                      jsData,
                                      EmptyString(),
                                      EmptyString(), nullptr);
  NS_ENSURE_SUCCESS(rv, rv);
  event->SetTrusted(true);
  return DispatchDOMEvent(nullptr, event, nullptr, nullptr);
}

void
PresentationSocketChannel::NotifyOpen()
{
  SetReadyState(EChannelState::OPEN);
  nsRefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(this, NS_LITERAL_STRING("open"), false);
  asyncDispatcher->PostDOMEvent();
}

void
PresentationSocketChannel::NotifyClose()
{
  SetReadyState(EChannelState::CLOSED);
  nsRefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(this, NS_LITERAL_STRING("close"), false);
  asyncDispatcher->PostDOMEvent();

  if (mActiveClosing) {
    mSession = nullptr;
  }
}

void
PresentationSocketChannel::NotifyError()
{
  SetReadyState(EChannelState::CLOSING);
  nsRefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(this, NS_LITERAL_STRING("error"), false);
  asyncDispatcher->PostDOMEvent();
}

END_PRESENTATION_NAMESPACE
