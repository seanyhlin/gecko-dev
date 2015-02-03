/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PresentationMessageEvent.h"
#include "mozilla/dom/PresentationStateEvent.h"
#include "PresentationService.h"
#include "PresentationSession.h"
#include "nsIDOMMessageEvent.h"
#include "nsServiceManagerUtils.h"
#include "nsStringStream.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::presentation;

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Presentation", args);
#else
#define LOG(args...)  printf(args);
#endif

NS_IMPL_CYCLE_COLLECTION_CLASS(PresentationSession)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PresentationSession, DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PresentationSession, DOMEventTargetHelper)
  tmp->Shutdown();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(PresentationSession, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(PresentationSession, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PresentationSession)
  NS_INTERFACE_MAP_ENTRY(nsIPresentationSessionListener)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

PresentationSession::PresentationSession(nsPIDOMWindow* aWindow,
                                         const nsAString& aId)
  : DOMEventTargetHelper(aWindow)
  , mId(aId)
  , mState(false)
{
}

/* virtual */ PresentationSession::~PresentationSession()
{
  Shutdown();
}

/* static */ already_AddRefed<PresentationSession>
PresentationSession::Create(nsPIDOMWindow* aWindow,
                            const nsAString& aId)
{
  LOG("[PresentationSession] %s", __FUNCTION__);
  nsRefPtr<PresentationSession> session = new PresentationSession(aWindow, aId);
  return NS_WARN_IF(!session->Init()) ? nullptr : session.forget();
}

bool
PresentationSession::Init()
{
  if (NS_WARN_IF(mId.IsEmpty())) {
    return false;
  }

  // Initialize |mOrigin|.
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  NS_ENSURE_TRUE(global, false);

  nsIPrincipal* principal = global->PrincipalOrNull();
  NS_ENSURE_TRUE(principal, false);

  nsAutoCString origin;
  nsresult rv = principal->GetOrigin(getter_Copies(origin));
  NS_ENSURE_SUCCESS(rv, false);

  mOrigin = NS_ConvertUTF8toUTF16(origin);

  // Register session listener.
  PresentationService* service = PresentationService::Get();
  NS_ENSURE_TRUE(service, false);

  service->RegisterSessionListener(mId, this);

  return true;
}

void
PresentationSession::Shutdown()
{
  PresentationService* service = PresentationService::Get();
  NS_ENSURE_TRUE_VOID(service);

  service->UnregisterSessionListener(mId, this);
}

/* virtual */ JSObject*
PresentationSession::WrapObject(JSContext* aCx)
{
  return PresentationSessionBinding::Wrap(aCx, this);
}

void
PresentationSession::GetId(nsAString& aId) const
{
  aId = mId;
}

bool
PresentationSession::State() const
{
  return mState;
}

void
PresentationSession::Send(const nsAString& aData,
                          ErrorResult& aRv)
{
  LOG("[PresentationSession] %s", __FUNCTION__);
  // Sending is not allowed if the socket is not connected.
  if (NS_WARN_IF(!mState)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsresult rv;
  nsCOMPtr<nsIStringInputStream> stream = do_CreateInstance(NS_STRINGINPUTSTREAM_CONTRACTID, &rv);
  if(NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  NS_ConvertUTF16toUTF8 msgString(aData);
  rv = stream->SetData(msgString.BeginReading(), msgString.Length());
  if(NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  PresentationService* service = PresentationService::Get();
  if(NS_WARN_IF(!service)) {
    aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  rv = service->SendMessageInternal(mId, stream);
  if(NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
  }
}

void
PresentationSession::Disconnect(ErrorResult& aRv)
{
  LOG("[PresentationSession] %s", __FUNCTION__);
  // Closing is not allowed if the socket is not connected.
  if (NS_WARN_IF(!mState)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  PresentationService* service = PresentationService::Get();
  if(NS_WARN_IF(!service)) {
    aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  nsresult rv = service->CloseSessionInternal(mId);
  if(NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
  }
}

nsresult
PresentationSession::SetState(bool aState)
{
  LOG("[PresentationSession] %s", __FUNCTION__);
  if (mState == aState) {
    return NS_OK;
  }

  mState = aState;

  PresentationStateEventInit init;
  init.mState = aState;;
  nsRefPtr<PresentationStateEvent> event =
    PresentationStateEvent::Constructor(this, NS_LITERAL_STRING("statechange"), init);

  return DispatchTrustedEvent(event);
}

NS_IMETHODIMP
PresentationSession::NotifyStateChange(const nsAString& aSessionId,
                                       uint16_t aState,
                                       nsresult aReason)
{
  LOG("[PresentationSession] %s", __FUNCTION__);
  if (mId != aSessionId) {
    return NS_OK;
  }

  NS_WARN_IF(NS_FAILED(aReason));

  if (aState == nsIPresentationSessionListener::STATE_CONNECTED) {
    return SetState(true);
  }

  if (aState == nsIPresentationSessionListener::STATE_DISCONNECTED) {
    PresentationService* service = PresentationService::Get();
    NS_ENSURE_TRUE(service, NS_ERROR_DOM_ABORT_ERR);

    service->UnregisterSessionListener(mId, this);
    return SetState(false);
  }

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
PresentationSession::NotifyMessage(const nsAString& aSessionId,
                                   const nsACString& aData)
{
  LOG("[PresentationSession] %s", __FUNCTION__);
  if (mId != aSessionId) {
    return NS_OK;
  }

  // No message should be expected when session is not connected.
  if (NS_WARN_IF(!mState)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(GetOwner()))) {
    return NS_ERROR_FAILURE;
  }
  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> jsData(cx);

  // Regard the data as string for now.
  NS_ConvertUTF8toUTF16 utf16data(aData);
  JSString* jsString = JS_NewUCStringCopyN(cx, utf16data.get(), utf16data.Length());
  NS_ENSURE_TRUE(jsString, NS_ERROR_FAILURE);

  jsData = STRING_TO_JSVAL(jsString);

  return DispatchMessageEvent(jsData);
}

nsresult
PresentationSession::DispatchMessageEvent(JS::Handle<JS::Value> aData)
{
  LOG("[PresentationSession] %s", __FUNCTION__);
/*  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv = NS_NewDOMMessageEvent(getter_AddRefs(event), this, nullptr, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMMessageEvent> messageEvent = do_QueryInterface(event);
  rv = messageEvent->InitMessageEvent(NS_LITERAL_STRING("message"),
                                      false, false, aData, mOrigin,
                                      EmptyString(), nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  event->SetTrusted(true);
  return DispatchDOMEvent(nullptr, event, nullptr, nullptr);*/
  PresentationMessageEventInit init;
  init.mData = aData;
  nsRefPtr<PresentationMessageEvent> event =
    PresentationMessageEvent::Constructor(this, NS_LITERAL_STRING("message"), init);
  return DispatchTrustedEvent(event); 
}
