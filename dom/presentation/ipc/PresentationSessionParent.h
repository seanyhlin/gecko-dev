#ifndef mozilla_dom_PresentationSessionParent_h
#define mozilla_dom_PresentationSessionParent_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/PPresentationSessionParent.h"
#include "mozilla/dom/PresentationCommon.h"
#include "mozilla/dom/PresentationSessionInternal.h"

BEGIN_PRESENTATION_NAMESPACE

class PresentationSessionParent : public PPresentationSessionParent
                                , public ISessionCallback
{
public:
  // PPresentationSessionParent
  virtual bool
  RecvMessage(const nsCString& aInfoMsg, const bool& aIsBinary) MOZ_OVERRIDE;
  virtual bool
  RecvEstablishConnection() MOZ_OVERRIDE;
  virtual bool
  RecvCloseConnection() MOZ_OVERRIDE;
  virtual bool
  RecvBinaryStream(const InputStreamParams& aStream,
                   const uint32_t& aLength) MOZ_OVERRIDE;

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  PresentationSessionParent(const nsCString& aURL,
                            const nsCString& aSessionId,
                            const nsCString& aDeviceId);
  virtual ~PresentationSessionParent();

  // ISessionCallback
  virtual void
  NotifyStateChanged(PresentationSessionState aState) MOZ_OVERRIDE;
  virtual void
  NotifyChannelStatus(EChannelState aChannelState) MOZ_OVERRIDE;
  virtual void
  NotifyMessageRecieved(const nsACString& aMsg, bool aIsBinary) MOZ_OVERRIDE;

  virtual bool
  RecvConnectionStatus(nsCString* aConnectStatus);
private:
  nsCString mURL;
  nsCString mSessionId;
  nsCString mDeviceId;
  PresentationSessionInternal* mPSInternal;
  nsAutoPtr<PresentationSessionCallback> mCallback;
};

END_PRESENTATION_NAMESPACE

#endif
