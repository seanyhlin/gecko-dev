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

#include "FakePresentationStuff.h"

using namespace mozilla;

BEGIN_PRESENTATION_NAMESPACE

// ===== MockPresentationSessionTransport ===== //
NS_IMPL_ISUPPORTS(MockPresentationSessionTransport,
                  nsIPresentationSessionTransport,
                  nsITimerCallback)
MockPresentationSessionTransport::MockPresentationSessionTransport()
{
  mStage = Preferences::GetInt("dom.presentation.test.stage", -1);
}
MockPresentationSessionTransport::~MockPresentationSessionTransport()
{
  mTimerOpened = nullptr;
  mTimerClosed = nullptr;
  mTimerMessage = nullptr;
}

NS_IMETHODIMP
MockPresentationSessionTransport::Notify(nsITimer* timer)
{
  return NS_OK;
}

NS_IMETHODIMP
MockPresentationSessionTransport::PostMessage(const nsAString& aMsg)
{
  CreateNotifyMessageBackTimer();
  if (mStage == 3) {
    CreateNotifyClosedTimer();
  }
  return NS_OK;
}

NS_IMETHODIMP
MockPresentationSessionTransport::PostBinaryMessage(const nsACString& aMsg)
{
  CreateNotifyMessageBackTimer();
  if (mStage == 3) {
    CreateNotifyClosedTimer();
  }
  return NS_OK;
}

NS_IMETHODIMP
MockPresentationSessionTransport::PostBinaryStream(nsIInputStream*)
{
  return NS_OK;
}

NS_IMETHODIMP
MockPresentationSessionTransport::Close()
{
  CreateNotifyClosedTimer();
  return NS_OK;
}

NS_IMETHODIMP
MockPresentationSessionTransport::GetListener(nsIPresentationSessionTransportListener**
                                              aListener)
{
  return NS_OK;
}

NS_IMETHODIMP
MockPresentationSessionTransport::SetListener(nsIPresentationSessionTransportListener*
                                              aListener)
{
  mListener = aListener;
  if (!mListener) {
    if (mTimerOpened) {
      mTimerOpened->Cancel();
      mTimerOpened = nullptr;
    }
    if (mTimerClosed) {
      mTimerClosed->Cancel();
      mTimerClosed = nullptr;
    }
    if (mTimerMessage) {
      mTimerMessage->Cancel();
      mTimerMessage = nullptr;
    }
    return NS_OK;
  }

  if (mStage == 1 || mStage == 2 || mStage == 3) {
    CreateNotifyOpenedTimer();
  }

  if (mStage == 1) {
    CreateNotifyClosedTimer();
  }
  return NS_OK;
}

void
MockPresentationSessionTransport::CreateNotifyOpenedTimer()
{
  nsresult rv;
  mTimerOpened = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING(" MockPresentationSessionTransport can't create timer ! ");
  }
  rv = mTimerOpened->InitWithFuncCallback(this->NotifyOpen,
                                          this,
                                          0,
                                          nsITimer::TYPE_ONE_SHOT);
  if (NS_FAILED(rv)) {
    mTimerOpened = nullptr;
  }
}

void
MockPresentationSessionTransport::CreateNotifyClosedTimer()
{
  nsresult rv;
  mTimerClosed = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING(" MockPresentationSessionTransport can't create timer ! ");
  }
  rv = mTimerClosed->InitWithFuncCallback(this->NotifyClose,
                                          this,
                                          1000,
                                          nsITimer::TYPE_ONE_SHOT);
  if (NS_FAILED(rv)) {
    mTimerClosed = nullptr;
  }
}

void
MockPresentationSessionTransport::CreateNotifyMessageBackTimer()
{
  nsresult rv;
  mTimerMessage = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING(" MockPresentationSessionTransport can't create timer ! ");
  }
  rv = mTimerMessage->InitWithFuncCallback(this->NotifyMessage,
                                          this,
                                          1000,
                                          nsITimer::TYPE_ONE_SHOT);
  if (NS_FAILED(rv)) {
    mTimerMessage = nullptr;
  }
}

void
MockPresentationSessionTransport::NotifyOpen(nsITimer* aTimer, void* aSelf)
{
  nsRefPtr<MockPresentationSessionTransport> self =
      static_cast<MockPresentationSessionTransport*>(aSelf);
  if (self) {
    self->mListener->NotifyOpened();
  }
}

void
MockPresentationSessionTransport::NotifyClose(nsITimer* aTimer, void* aSelf)
{
  nsRefPtr<MockPresentationSessionTransport> self =
      static_cast<MockPresentationSessionTransport*>(aSelf);
  if (self) {
    self->mListener->NotifyClosed(NS_OK);
  }
}

void
MockPresentationSessionTransport::NotifyMessage(nsITimer* aTimer, void* aSelf)
{
  nsRefPtr<MockPresentationSessionTransport> self =
      static_cast<MockPresentationSessionTransport*>(aSelf);
  if (self) {
    self->mListener->OnMessage(NS_LITERAL_STRING("HelloBack"));
  }
}

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
MockPresentationDevice::EstablishSessionTransport(const nsAString &aURL,
  const nsAString &aPId,
  nsIPresentationSessionTransport** aTransport)
{
  nsRefPtr<MockPresentationSessionTransport> psTransport =
      new MockPresentationSessionTransport();
  psTransport.forget(aTransport);
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

// ===== AvailabilityNotifier ===== //
NS_IMPL_ISUPPORTS(AvailabilityNotifier, nsITimerCallback)
NS_IMETHODIMP
AvailabilityNotifier::Notify(nsITimer* timer)
{
  if (mPresentationManager) {
    mPresentationManager->NotifyDeviceAvailable(mAvailable);
  }
  return NS_OK;
}

END_PRESENTATION_NAMESPACE
