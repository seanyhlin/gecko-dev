#include "nsIPresentationDevice.h"

#include "PresentationDeviceRequest.h"
#include "PresentationManager.h"

BEGIN_PRESENTATION_NAMESPACE

NS_IMPL_ISUPPORTS(nsPresentationDeviceRequest, nsIPresentationDeviceRequest)

nsPresentationDeviceRequest::nsPresentationDeviceRequest()
  : mICallback(nullptr)
{

}

nsPresentationDeviceRequest::~nsPresentationDeviceRequest()
{
  mICallback = nullptr;
}

void
nsPresentationDeviceRequest::SetupRequest(ICallbackDeviceSelection* aICallback,
                                          const nsACString& aOrigin,
                                          const nsACString& aRequestURL,
                                          const nsACString& aLastSessionId)
{
  mICallback = aICallback;
  mOrigin = aOrigin;
  mRequestURL = aRequestURL;
  mLastSessionId = aLastSessionId;
}

NS_IMETHODIMP
nsPresentationDeviceRequest::Select(nsIPresentationDevice* aDevice)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mICallback);
  nsresult rv = NS_OK;
  rv = mICallback->CallbackFromDeviceRequest(aDevice,
                                             mOrigin,
                                             mRequestURL,
                                             mLastSessionId);
  return rv;
}

NS_IMETHODIMP
nsPresentationDeviceRequest::Cancel()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv = NS_OK;
  rv = mICallback->CallbackFromDeviceRequest(nullptr,
                                             mOrigin,
                                             mRequestURL,
                                             mLastSessionId);
  return rv;
}

NS_IMETHODIMP
nsPresentationDeviceRequest::GetOrigin(nsAString& aOrigin)
{
  aOrigin.Assign(NS_ConvertUTF8toUTF16(mOrigin));
  return NS_OK;
}

NS_IMETHODIMP
nsPresentationDeviceRequest::GetRequestURL(nsAString & aRequestURL)
{
  aRequestURL.Assign(NS_ConvertUTF8toUTF16(mRequestURL));
  return NS_OK;
}

END_PRESENTATION_NAMESPACE
