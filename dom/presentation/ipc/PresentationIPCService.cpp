/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTArray.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/StaticPtr.h"
#include "PresentationChild.h"
#include "PresentationIPCService.h"

using namespace mozilla::dom;
using namespace mozilla::dom::presentation;

namespace {

PresentationChild* sPresentationChild;

} // anonymous

/* static */ PresentationIPCService*
PresentationIPCService::Create()
{
  ContentChild* contentChild = ContentChild::GetSingleton();
  if (!NS_WARN_IF(contentChild)) {
    return nullptr;
  }

  PresentationIPCService* service = new PresentationIPCService();
  sPresentationChild = new PresentationChild(service);
  contentChild->SendPPresentationConstructor(sPresentationChild);
  return service;
}

PresentationIPCService::~PresentationIPCService()
{
  mListeners.Clear();
}

/* virtual */ void
PresentationIPCService::RegisterListener(nsIPresentationListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  mListeners.AppendElement(aListener);
}

/* virtual */ void
PresentationIPCService::UnregisterListener(nsIPresentationListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  mListeners.RemoveElement(aListener);
}

/* virtual */ nsresult
PresentationIPCService::StartSessionInternal(const nsAString& aUrl,
                                             const nsAString& aSessionId,
                                             const nsAString& aOrigin,
                                             nsIPresentationServiceCallback* aCallback)
{
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

nsresult
PresentationIPCService::SendRequest(nsIPresentationServiceCallback* aCallback,
                                    const PresentationRequest& aRequest)
{
  if (!sPresentationChild) {
    NS_WARNING("");
    return NS_ERROR_FAILURE;
  }
  PresentationRequestChild* actor = new PresentationRequestChild(aCallback);
  sPresentationChild->SendPPresentationRequestConstructor(actor, aRequest);
  return NS_OK;
}
