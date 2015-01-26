/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_presentation_PresentationIPCService_h
#define mozilla_dom_presentation_PresentationIPCService_h

#include "mozilla/Attributes.h"
#include "nsAutoPtr.h"
#include "PresentationService.h"

namespace mozilla {
namespace dom {
namespace presentation {

class PresentationRequest;

class PresentationIPCService MOZ_FINAL : public PresentationService
{
public:
  static PresentationIPCService* Create();

  virtual nsresult
  StartSessionInternal(const nsAString& aUrl,
                       const nsAString& aSessionId,
                       const nsAString& aOrigin,
                       nsIPresentationRequestCallback* aCallback);

  virtual nsresult
  JoinSessionInternal(const nsAString& aUrl,
                      const nsAString& aSessionId,
                      const nsAString& aOrigin);

  virtual void
  RegisterListener(nsIPresentationListener* aListener);

  virtual void
  UnregisterListener(nsIPresentationListener* aListener);

private:
  PresentationIPCService() { }
  virtual ~PresentationIPCService();

  nsresult
  SendRequest(nsIPresentationRequestCallback* aCallback,
              const PresentationRequest& aRequest);
};

} // namespace presentation
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_presentation_PresentationIPCService_h
