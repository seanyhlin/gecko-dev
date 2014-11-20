/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PresentationSessionRequest.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(PresentationSessionRequest, nsIPresentationSessionRequest)

PresentationSessionRequest::PresentationSessionRequest(nsIPresentationDevice* aDevice,
                                                       const nsAString& aUrl,
                                                       const nsAString& aPresentationId,
                                                       nsIPresentationSessionTransport* aSessionTransport)
  : mUrl(aUrl)
  , mPresentationId(aPresentationId)
  , mDevice(aDevice)
  , mSessionTransport(aSessionTransport)
{
}

PresentationSessionRequest::~PresentationSessionRequest()
{
}

// nsIPresentationSessionRequest

NS_IMETHODIMP
PresentationSessionRequest::GetDevice(nsIPresentationDevice** aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);

  nsCOMPtr<nsIPresentationDevice> device = mDevice;
  device.forget(aRetVal);

  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionRequest::GetUrl(nsAString& aRetVal)
{
  aRetVal = mUrl;

  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionRequest::GetPresentationId(nsAString& aRetVal)
{
  aRetVal = mPresentationId;

  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionRequest::GetSessionTransport(nsIPresentationSessionTransport** aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);

  nsCOMPtr<nsIPresentationSessionTransport> sessionTransport = mSessionTransport;
  sessionTransport.forget(aRetVal);

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
