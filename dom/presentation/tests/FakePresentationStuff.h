#ifndef mozilla_dom_fakepresentationmanager_h__
#define mozilla_dom_fakepresentationmanager_h__

#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"

#include "nsIPresentationDevice.h"
#include "nsIPresentationSessionTransport.h"
#include "nsThreadUtils.h"

#include "PresentationCommon.h"
#include "mozilla/dom/PresentationManager.h"

BEGIN_PRESENTATION_NAMESPACE

class PresentationManager;

class MockPresentationDevice : public nsIPresentationDevice
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONDEVICE
  MockPresentationDevice()
    : mId(NS_LITERAL_CSTRING("testID"))
    , mName(NS_LITERAL_CSTRING("testName"))
    , mType(NS_LITERAL_CSTRING("testType"))
  {}
private:
  virtual ~MockPresentationDevice();
  nsCString mId;
  nsCString mName;
  nsCString mType;
};

class MockPresentationSessionTransport : public nsIPresentationSessionTransport
                                       , public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONSESSIONTRANSPORT
  NS_DECL_NSITIMERCALLBACK
  MockPresentationSessionTransport();

protected:
  static void
  NotifyOpen(nsITimer* aTimer, void* aSelf);
  static void
  NotifyClose(nsITimer* aTimer, void* aSelf);
  static void
  NotifyMessage(nsITimer* aTimer, void* aSelf);

  nsIPresentationSessionTransportListener* mListener;

private:
  virtual
  ~MockPresentationSessionTransport();

  void
  CreateNotifyOpenedTimer();
  void
  CreateNotifyClosedTimer();
  void
  CreateNotifyMessageBackTimer();

  nsCOMPtr<nsITimer> mTimerOpened;
  nsCOMPtr<nsITimer> mTimerClosed;
  nsCOMPtr<nsITimer> mTimerMessage;

  int32_t mStage;
};

class AvailabilityNotifier MOZ_FINAL : public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

  AvailabilityNotifier(PresentationManager* aPMgr,
                       bool aAvailable)
    : mPresentationManager(aPMgr)
    , mAvailable(aAvailable)
  {}

private:
  PresentationManager* mPresentationManager;
  bool mAvailable;
  ~AvailabilityNotifier() { mPresentationManager = nullptr; }
};

END_PRESENTATION_NAMESPACE

#endif
