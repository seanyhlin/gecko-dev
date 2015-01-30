/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PresentationSessionTransport.h"
#include "nsIPresentationControlChannel.h"
#include "nsINetAddr.h"
#include "nsMultiplexInputStream.h"
#include "nsNetUtil.h"
#include "nsISocketTransport.h"
#include "nsISocketTransportService.h"
#include "nsStreamUtils.h"
#include "MainThreadUtils.h"
#include "nsIEventTarget.h"
#include "nsIThread.h"

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Presentation", args);
#else
#define LOG(args...)  printf(args);
#endif

namespace mozilla {
namespace dom {
namespace presentation {

NS_IMPL_ISUPPORTS(PresentationSessionTransport,
                  nsITransportEventSink,
                  nsIInputStreamCallback,
                  nsIStreamListener)

PresentationSessionTransport::PresentationSessionTransport(nsISocketTransport* aTransport, nsIPresentationSessionTransportCallback* aCallback)
  : mTransport(aTransport)
  , mCallback(aCallback)
{
  LOG("[SessionTransport] %s", __FUNCTION__);
  //XXX find correct parameter
  nsCOMPtr<nsIInputStream> inputStream;
  mTransport->OpenInputStream(0, 0, 0, getter_AddRefs(inputStream));
  mTransport->OpenOutputStream(0, 0, 0, getter_AddRefs(mOutputStream));
  mMultiplexStream = do_CreateInstance(NS_MULTIPLEXINPUTSTREAM_CONTRACTID);

  nsCOMPtr<nsIEventTarget> sts = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID);
  NS_AsyncCopy(mMultiplexStream, mOutputStream, sts, NS_ASYNCCOPY_VIA_READSEGMENTS, 4096, nullptr, nullptr, false, false, nullptr, nullptr);
  mTransport->SetEventSink(this, NS_GetCurrentThread());

  mInputStream = do_QueryInterface(inputStream);
  mInputStream->AsyncWait(this, 0, 0, NS_GetCurrentThread());
}

PresentationSessionTransport::~PresentationSessionTransport()
{
}

// nsIPresentationSessionTransport
NS_IMETHODIMP
PresentationSessionTransport::Send(nsIInputStream* aData)
{
  LOG("[SessionTransport] %s", __FUNCTION__);
  return mMultiplexStream->AppendStream(aData);
}

NS_IMETHODIMP
PresentationSessionTransport::Close(nsresult aReason)
{
  LOG("[SessionTransport] %s", __FUNCTION__);
  mTransport->Close(aReason);
  mInputStream = nullptr;
  mOutputStream = nullptr;
  return NS_OK;
}

// nsITransportEventSink
NS_IMETHODIMP
PresentationSessionTransport::OnTransportStatus(nsITransport* aTransport,
                                                nsresult aStatus,
                                                uint64_t aProgress,
                                                uint64_t aProgressMax)
{
  LOG("[SessionTransport] %s", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());

  if (aStatus == NS_NET_STATUS_CONNECTED_TO) {
    nsCOMPtr<nsIInputStreamPump> pump;
    NS_NewInputStreamPump(getter_AddRefs(pump), mInputStream);

    pump->AsyncRead(this, nullptr);

    mCallback->OnComplete();
  }
  return NS_OK;
}

// nsIInputStreamCallback
NS_IMETHODIMP
PresentationSessionTransport::OnInputStreamReady(nsIAsyncInputStream* aStream)
{
  LOG("[SessionTransport] %s", __FUNCTION__);
  uint64_t available;
  nsresult rv = aStream->Available(&available);

  if (NS_FAILED(rv)) {
    Close(rv);
  }

  return NS_OK;
}

// nsIRequestObserver
NS_IMETHODIMP
PresentationSessionTransport::OnStartRequest(nsIRequest* aRequest,
                                             nsISupports* aContext)
{
  LOG("[SessionTransport] %s", __FUNCTION__);
  //XXX do nothing?
  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionTransport::OnStopRequest(nsIRequest* aRequest,
                                            nsISupports* aContext,
                                            nsresult aStatusCode)
{
  LOG("[SessionTransport] %s", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());

  if (mCallback) {
    mCallback->OnClose(aStatusCode);
  }
  return NS_OK;
}

// nsIStreamListener
NS_IMETHODIMP
PresentationSessionTransport::OnDataAvailable(nsIRequest* aRequest,
                                              nsISupports* aContext,
                                              nsIInputStream* aStream,
                                              uint64_t aOffset,
                                              uint32_t aCount)
{
  LOG("[SessionTransport] %s", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());

  if (mCallback) {
    nsCString data;
    NS_ReadInputStreamToString(aStream, data, aCount);
    mCallback->OnData(data);
  }
  return NS_OK;
}

} // namespace presentation
} // namespace dom
} // namespace mozilla
