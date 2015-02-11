/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTArray.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "nsIPresentationRequestCallback.h"
#include "PresentationChild.h"
#include "PresentationIPCService.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::presentation;

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Presentation", args);
#else
#define LOG(args...)  printf(args);
#endif

namespace {

PresentationChild* sPresentationChild;
static bool sIsActorDead = false;

} // anonymous

/* static */ already_AddRefed<PresentationIPCService>
PresentationIPCService::Create()
{
  if (sIsActorDead) {
    return nullptr;  
  }

  LOG("[IPCService] %s", __FUNCTION__);
  ContentChild* contentChild = ContentChild::GetSingleton();
  if (NS_WARN_IF(!contentChild)) {
    return nullptr;
  }

  nsRefPtr<PresentationIPCService> service = new PresentationIPCService();
  sPresentationChild = new PresentationChild(service);
  contentChild->SendPPresentationConstructor(sPresentationChild);
  return service.forget();
}

/* virtual */
PresentationIPCService::~PresentationIPCService()
{
  LOG("[IPCService] %s", __FUNCTION__);
  mSessionListeners.Clear();
  sPresentationChild = nullptr;
}

/* virtual */ nsresult
PresentationIPCService::StartSessionInternal(const nsAString& aUrl,
                                             const nsAString& aSessionId,
                                             const nsAString& aOrigin,
                                             nsIPresentationRequestCallback* aCallback)
{
  LOG("[IPCService] %s", __FUNCTION__);
  return SendRequest(aCallback,
                     StartSessionRequest(nsString(aUrl), nsString(aSessionId), nsString(aOrigin)));
}

/* virtual */ nsresult
PresentationIPCService::JoinSessionInternal(const nsAString& aUrl,
                                            const nsAString& aSessionId,
                                            const nsAString& aOrigin)
{
  return NS_OK;
}

/* virtual */ nsresult
PresentationIPCService::SendMessageInternal(const nsAString& aSessionId,
                                            nsIInputStream* aStream)
{
  if (aSessionId.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }
  NS_ENSURE_ARG(aStream);

  mozilla::ipc::OptionalInputStreamParams stream;
  nsTArray<mozilla::ipc::FileDescriptor> fds;
  SerializeInputStream(aStream, stream, fds);

  MOZ_ASSERT(fds.IsEmpty());

  return SendRequest(nullptr, SendMessageRequest(nsString(aSessionId), stream));
}

/* virtual */ nsresult
PresentationIPCService::CloseSessionInternal(const nsAString& aSessionId)
{
  LOG("[IPCService] %s", __FUNCTION__);
  if (aSessionId.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  return SendRequest(nullptr, CloseSessionRequest(nsString(aSessionId)));
}

nsresult
PresentationIPCService::SendRequest(nsIPresentationRequestCallback* aCallback,
                                    const PresentationRequest& aRequest)
{
  if (sPresentationChild) {
    PresentationRequestChild* actor = new PresentationRequestChild(aCallback);
    sPresentationChild->SendPPresentationRequestConstructor(actor, aRequest);
  }
  return NS_OK;
}

/* virtual */ void
PresentationIPCService::RegisterListener(nsIPresentationListener* aListener)
{
  LOG("[IPCService] %s", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());
  mListeners.AppendElement(aListener);
  if (sPresentationChild) {
    NS_WARN_IF(!sPresentationChild->SendRegisterHandler());
  }
}

/* virtual */ void
PresentationIPCService::UnregisterListener(nsIPresentationListener* aListener)
{
  LOG("[IPCService] %s", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());
  mListeners.RemoveElement(aListener);
  if (sPresentationChild) {
    NS_WARN_IF(!sPresentationChild->SendUnregisterHandler());
  }
}

/* virtual */ void
PresentationIPCService::RegisterSessionListener(const nsAString& aSessionId,
                                                nsIPresentationSessionListener* aListener)
{
  LOG("[IPCService] %s", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());
  mSessionListeners.Put(aSessionId, aListener);
  if (sPresentationChild) {
    NS_WARN_IF(!sPresentationChild->SendRegisterSessionHandler(nsString(aSessionId)));
  }
}

/* virtual */ void
PresentationIPCService::UnregisterSessionListener(const nsAString& aSessionId,
                                                  nsIPresentationSessionListener* aListener)
{
  LOG("[IPCService] %s", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());

  mSessionListeners.Remove(aSessionId);
  if (sPresentationChild) {
    NS_WARN_IF(!sPresentationChild->SendUnregisterSessionHandler(nsString(aSessionId)));
  }
}

nsresult
PresentationIPCService::NotifySessionStateChange(const nsAString& aSessionId,
                                                 uint16_t aState,
                                                 nsresult aReason)
{
  LOG("[IPCService] %s", __FUNCTION__);
  nsCOMPtr<nsIPresentationSessionListener> listener;
  if (NS_WARN_IF(!mSessionListeners.Get(aSessionId, getter_AddRefs(listener)))) {
    return NS_OK;
  }

  if (aState == nsIPresentationSessionListener::STATE_DISCONNECTED) {
    mSessionListeners.Remove(aSessionId);
  }

  return listener->NotifyStateChange(aSessionId, aState, aReason);
}

nsresult
PresentationIPCService::NotifyMessage(const nsAString& aSessionId,
                                      const nsACString& aData)
{
  nsCOMPtr<nsIPresentationSessionListener> listener;
  if (NS_WARN_IF(!mSessionListeners.Get(aSessionId, getter_AddRefs(listener)))) {
    return NS_OK;
  }

  return listener->NotifyMessage(aSessionId, aData);
}

void
PresentationIPCService::NotifyDeadActor()
{
  LOG("[IPCService] %s", __FUNCTION__);
  sIsActorDead = true;
  sPresentationChild = nullptr;
}
