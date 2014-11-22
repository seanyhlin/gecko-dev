#ifndef mozilla_dom_PresentationSessionInstance_h
#define mozilla_dom_PresentationSessionInstance_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/PresentationSessionBinding.h"
#include "mozilla/ipc/InputStreamUtils.h"

#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsIPresentationDevice.h"
#include "PresentationCommon.h"
#include "PresentationSocketChannelInternal.h"

BEGIN_PRESENTATION_NAMESPACE

class PresentationSession;

class ISessionCallback
{
public:
  virtual ~ISessionCallback() {};

  virtual void
  NotifyStateChanged(PresentationSessionState aState) = 0;

  virtual void
  NotifyChannelStatus(EChannelState aChannelState) = 0;

  virtual void
  NotifyMessageRecieved(const nsACString& aData, bool aIsBinary) = 0;
};

class PresentationSessionCallback : public ISessionCallback
{
public:
  PresentationSessionCallback(ISessionCallback* aPSession);

  virtual void
  NotifyStateChanged(PresentationSessionState aState) MOZ_OVERRIDE;

  virtual void
  NotifyChannelStatus(EChannelState aChannelState) MOZ_OVERRIDE;

  virtual void
  NotifyMessageRecieved(const nsACString& aMsg, bool aIsBinary) MOZ_OVERRIDE;

private:
  ISessionCallback* mPresentationSession;
};

class ISessionInternal
{
public:
  virtual ~ISessionInternal() { }

  virtual bool
  StartConnection() = 0;

  virtual bool
  PostMessageInfo(const nsACString& aMsg, bool aIsBinary) = 0;

  virtual bool
  PostBinaryStreamInfo(nsIInputStream *aStream,
                       uint32_t aLength) = 0;

  virtual bool
  CloseConnection() = 0;

  virtual void
  SetCallback(ISessionCallback* aCallback) = 0;

  virtual PresentationSessionState
  State() = 0;
};

class PresentationSessionInternal MOZ_FINAL : public nsISupports
                                            , public ISessionInternal
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(PresentationSessionInternal)
  PresentationSessionInternal();

  // ISessionInternal
  virtual bool
  StartConnection() MOZ_OVERRIDE;
  virtual bool
  PostMessageInfo(const nsACString& aMsg, bool aIsBinary) MOZ_OVERRIDE;
  virtual bool
  PostBinaryStreamInfo(nsIInputStream *aStream,
                       uint32_t aLength) MOZ_OVERRIDE;
  virtual bool
  CloseConnection() MOZ_OVERRIDE;
  virtual void
  SetCallback(ISessionCallback* aCallback) MOZ_OVERRIDE;
  virtual PresentationSessionState
  State() MOZ_OVERRIDE { return mSessionStateInternal; }

  // callback from SocketChannel to notify state changed or msg arrived.
  void
  CallbackConnectionState(PresentationSessionState aState);
  void
  CallbackChannelStatus(EChannelState aChannelState);
  void
  CallbackMessageAvailable(const nsACString& aMsg, bool aIsBinary);

  void
  SetupPSIInfo(const nsACString& aOrigin,
               const nsACString& aURL,
               const nsACString& aSessionId,
               nsIPresentationDevice* aDevice);

  void
  GetURL(nsACString& aURL);

  void
  GetSessionId(nsACString& aSessionId);

  void
  GetDeviceId(nsACString& aDeviceId);

  void
  GetDeviceName(nsACString& aDeviceName);

  void
  GetDeviceType(nsACString& aDeviceType);

private:
  virtual ~PresentationSessionInternal();

  nsCString mOrigin;
  nsCString mURL;
  nsCString mSessionId;
  nsCString mDeviceId;
  ISessionCallback* mCallback;
  nsCOMPtr<nsIPresentationDevice> mPresentationDevice;
  nsRefPtr<PresentationSocketChannelInternal> mPSSocketChannelInternal;

  PresentationSessionState mSessionStateInternal;
};

END_PRESENTATION_NAMESPACE

#endif
