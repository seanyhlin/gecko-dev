#ifndef mozilla_dom_PresentationSessionChild_h
#define mozilla_dom_PresentationSessionChild_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/PPresentationSessionChild.h"
#include "mozilla/dom/PresentationCommon.h"
#include "mozilla/ipc/InputStreamUtils.h"

BEGIN_PRESENTATION_NAMESPACE

class ISessionCallback;

class PresentationSessionChild : public PPresentationSessionChild
                               , public ISessionInternal
{
public:
  PresentationSessionChild(const nsACString& aURL,
                           const nsACString& aSessionId,
                           const nsACString& aDeviceId);
  virtual ~PresentationSessionChild();

  // PPresentationSessionChild
  virtual bool
  RecvUpdateConnectionStatus(const nsCString& aStateMsg);
  virtual bool
  RecvUpdateChannelStatus(const uint32_t& aChannelStatus);
  virtual bool
  RecvMessage(const nsCString& aInfoMsg, const bool& aIsBinary);

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
  State();

private:
  ISessionCallback* mCallback;
};

END_PRESENTATION_NAMESPACE

#endif
