#ifndef mozilla_dom_presentationmanager_h__
#define mozilla_dom_presentationmanager_h__

#include "mozilla/Attributes.h"
#include "mozilla/dom/FakePresentationStuff.h"
#include "mozilla/dom/presentation/PresentationSessionRequest.h"
#include "mozilla/dom/NavigatorPresentationParent.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"

#include "nsIObserver.h"
#include "nsIPresentationDevicePrompt.h"
#include "nsRefPtrHashtable.h"
#include "nsClassHashtable.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#include "PresentationCommon.h"
#include "PresentationSessionInternal.h"

// {dfe4f3f7-1094-44ca-a6f5-3be6452d21c5}
#define NS_PRESENTATIONMANAGER_CID {0xdfe4f3f7, 0x1094, 0x44ca, \
  {0xa6, 0xf5, 0x3b, 0xe6, 0x45, 0x2d, 0x21, 0xc5}}
#define NS_PRESENTATIONMANAGER_CONTRACTID "@mozilla.org/dom/presentationmanager;1"


BEGIN_PRESENTATION_NAMESPACE

class IPresentationManager
{
public:
  virtual ~IPresentationManager() { }

  virtual bool
  StartPresentationSession(const nsACString& aOrigin,
                           const nsACString& aURL,
                           const nsACString& aLastSessionId) = 0;
  virtual bool
  JoinPresentationSession(const nsACString& aOrigin,
                          const nsACString& aURL,
                          const nsACString& aLastSessionId) = 0;

  virtual void
  AddObserver(PresentationEventObserver* aObserver) = 0;

  virtual void
  RemoveObserver(PresentationEventObserver* aObserver) = 0;
};

class ICallbackDeviceSelection
{
public:
  virtual ~ICallbackDeviceSelection() { }

  virtual nsresult
  CallbackFromDeviceRequest(nsIPresentationDevice* aDevice,
                            const nsACString& aOrigin,
                            const nsACString& aRequestURL,
                            const nsACString& aLastSessionId) = 0;
};

class IManageSessionInternal
{
public:
  virtual ~IManageSessionInternal() { }

  virtual PresentationSessionInternal*
  GetPSInternal(const nsACString& aURL,
                const nsACString& aSessionId,
                const nsACString& aDeviceId) = 0;
  virtual void
  ReleasePSInternal(const nsACString& aURL,
                    const nsACString& aSessionId,
                    const nsACString& aDeviceId) = 0;
};

class PresentationManager MOZ_FINAL : public IPresentationManager
                                    , public ICallbackDeviceSelection
                                    , public IManageSessionInternal
                                    , public nsIObserver
{
  friend class AvailabilityNotifier;
  class PresentationFindSessionRunnable MOZ_FINAL : public nsRunnable
  {
  public:
    PresentationFindSessionRunnable(const nsACString& aOrigin,
                                    const nsACString& aURL,
                                    const nsACString& aSessionId)
      : mOrigin(aOrigin)
      , mURL(aURL)
      , mSessionId(aSessionId)
    {
    }
    virtual ~PresentationFindSessionRunnable() {}
    NS_IMETHOD Run() {
      PresentationManager* pm = PresentationManager::GetInstance();
      nsCString deviceId;
      nsCString deviceName;
      nsCString deviceType;
      pm->FindBestMatchSessionDevice(mURL,
                                     mSessionId,
                                     &deviceId,
                                     &deviceName,
                                     &deviceType);
      pm->NotifyDeviceSelected(mOrigin,
                               mURL,
                               mSessionId,
                               deviceId,
                               deviceName,
                               deviceType);
      return NS_OK;
    }
  private:
    nsCString mOrigin;
    nsCString mURL;
    nsCString mSessionId;
  };

  class DestroySessionInternalRunnable MOZ_FINAL : public nsRunnable
  {
  public:
    DestroySessionInternalRunnable(const nsACString& aURL,
                                   const nsACString& aSessionId,
                                   const nsACString& aDeviceId)
      : mURL(aURL)
      , mSessionId(aSessionId)
      , mDeviceId(aDeviceId)
    {
    }
    virtual ~DestroySessionInternalRunnable() {}
    NS_IMETHOD Run() {
      PresentationManager* pm = PresentationManager::GetInstance();
      bool ret = pm->ReleasePSInternal_Impl(mURL,
                                            mSessionId,
                                            mDeviceId);
      return ret ? NS_OK : NS_ERROR_FAILURE ;
    }
  private:
    nsCString mURL;
    nsCString mSessionId;
    nsCString mDeviceId;
  };


public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  // IPresentationManager
  virtual bool
  StartPresentationSession(const nsACString& aOrigin,
                           const nsACString& aURL,
                           const nsACString& aLastSessionId) MOZ_OVERRIDE;
  virtual bool
  JoinPresentationSession(const nsACString& aOrigin,
                          const nsACString& aURL,
                          const nsACString& aLastSessionId) MOZ_OVERRIDE;
  virtual void
  AddObserver(PresentationEventObserver* aObserver) MOZ_OVERRIDE;
  virtual void
  RemoveObserver(PresentationEventObserver* aObserver) MOZ_OVERRIDE;

  // IManageSessionInternal
  virtual PresentationSessionInternal*
  GetPSInternal(const nsACString& aURL,
                const nsACString& aSessionId,
                const nsACString& aDeviceId) MOZ_OVERRIDE;
  virtual void
  ReleasePSInternal(const nsACString& aURL,
                    const nsACString& aSessionId,
                    const nsACString& aDeviceId) MOZ_OVERRIDE;

  // ICallbackDeviceSelection
  nsresult
  CallbackFromDeviceRequest(nsIPresentationDevice* aDevice,
                            const nsACString& aOrigin,
                            const nsACString& aRequestURL,
                            const nsACString& aLastSessionId) MOZ_OVERRIDE;

  static PresentationManager*
  GetInstance();

  void
  FindBestMatchSessionDevice(const nsACString& aURL,
                             const nsACString& aSessionId,
                             nsACString* aDeviceId,
                             nsACString* aDeviceName,
                             nsACString* aDeviceType);

protected:
  PresentationManager();
  virtual ~PresentationManager() MOZ_OVERRIDE;

private:
  typedef nsTArray<nsRefPtr<PresentationSessionInternal>> SessionInternalArray;

  bool
  Init();

  nsresult
  HandleShutdown();

  static PLDHashOperator
  ClearSessionInternalArray(const nsACString& key,
                            nsAutoPtr<SessionInternalArray>& array,
                            void* cx);

  void
  GetUniqueSessionId(nsACString& aSessionId);

  bool
  ReleasePSInternal_Impl(const nsACString& aURL,
                         const nsACString& aSessionId,
                         const nsACString& aDeviceId);

  inline void
  NotifyPresentationEvent(PresentationEvent aEvent);

  void
  NotifyDeviceAvailable(bool aAvailable);

  void
  NotifyDeviceSelected(const nsACString& aOrigin,
                       const nsACString& aURL,
                       const nsACString& aSessionId,
                       const nsACString& aId,
                       const nsACString& aName,
                       const nsACString& aType);

  nsresult
  HandleCreateSessionRequest(nsRefPtr<PresentationSessionRequest> &aReq);

  static StaticRefPtr<PresentationManager> sPresentationManager;
  PresentationEventObserverList mObserverList;
  nsClassHashtable<nsCStringHashKey, SessionInternalArray> mSessionInternalStore;

  bool
  TestAvailability();
  bool
  TestJoinSession();

  bool mEnableTest;
  nsRefPtr<AvailabilityNotifier> mAvailableNotify;
  nsCOMPtr<nsITimer> mTimerAvailable;
  nsCOMPtr<nsIPresentationDevice> mTestJoinDevice;
};

END_PRESENTATION_NAMESPACE

#endif
