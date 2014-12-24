#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/dom/BindingUtils.h"

#include "nsIObserverService.h"
#include "nsIPresentationDeviceManager.h"
#include "nsIPropertyBag2.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsServiceManagerUtils.h"
#include "nsIPresentationControlChannel.h"

#include "FakePresentationDevice.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::presentation;

// ===== MockPresentationDevice ===== //
NS_IMPL_ISUPPORTS(MockPresentationDevice, nsIPresentationDevice)
MockPresentationDevice::~MockPresentationDevice()
{
}

NS_IMETHODIMP
MockPresentationDevice::GetName(nsACString & aName)
{
  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
MockPresentationDevice::GetId(nsACString & aId)
{
  aId.Assign(mId);
  return NS_OK;
}

NS_IMETHODIMP
MockPresentationDevice::GetType(nsACString & aType)
{
  aType.Assign(mType);
  return NS_OK;
}

NS_IMETHODIMP
MockPresentationDevice::EstablishControlChannel(const nsAString &aURL,
  const nsAString &aPId,
  nsIPresentationControlChannel** aTransport)
{
/*  nsRefPtr<MockPresentationSessionTransport> psTransport =
      new MockPresentationSessionTransport();
  psTransport.forget(aTransport);*/
  return NS_OK;
}

NS_IMETHODIMP
MockPresentationDevice::SetListener(nsIPresentationDeviceEventListener* aListener)
{
  return NS_OK;
}

NS_IMETHODIMP
MockPresentationDevice::GetListener(nsIPresentationDeviceEventListener** aListener)
{
  return NS_OK;
}
