#include "mozilla/dom/PresentationSessionInternal.h"
#include "mozilla/dom/TabChild.h"
#include "PresentationSessionChild.h"

BEGIN_PRESENTATION_NAMESPACE

bool
PresentationSessionChild::RecvUpdateConnectionStatus(const nsCString& aStateMsg)
{
  MOZ_ASSERT(mCallback, "Callback to PresentaionSession should exist !!");
  if (aStateMsg == "connected") {
    mCallback->NotifyStateChanged(PresentationSessionState::Connected);
  } else if (aStateMsg == "disconnected") {
    mCallback->NotifyStateChanged(PresentationSessionState::Disconnected);
  } else {
    // No need to callback
  }
  return true;
}

bool
PresentationSessionChild::RecvUpdateChannelStatus(const uint32_t& aChannelStatus)
{
  MOZ_ASSERT(mCallback, "Callback to PresentaionSession should exist !!");
  uint32_t status = aChannelStatus;
  mCallback->NotifyChannelStatus((EChannelState)status);
  return true;
}

bool
PresentationSessionChild::RecvMessage(const nsCString& aInfoMsg,
                                      const bool& aIsBinary)
{
  MOZ_ASSERT(mCallback, "Callback to PresentaionSession should exist !!");
  mCallback->NotifyMessageRecieved(aInfoMsg, aIsBinary);
  return true;
}

MOZ_IMPLICIT
PresentationSessionChild::PresentationSessionChild(const nsACString& aURL,
                                                   const nsACString& aSessionId,
                                                   const nsACString& aDeviceId)
{
  MOZ_COUNT_CTOR(PresentationSessionChild);
  nsRefPtr<TabChild> tabChild = static_cast<TabChild*>(Manager());
  if (tabChild) {
    tabChild->SendPPresentationSessionConstructor(this,
                                                  nsCString(aURL),
                                                  nsCString(aSessionId),
                                                  nsCString(aDeviceId));
  }
}

MOZ_IMPLICIT PresentationSessionChild::~PresentationSessionChild()
{
  MOZ_COUNT_DTOR(PresentationSessionChild);
  mCallback = nullptr;
}

void
PresentationSessionChild::SetCallback(ISessionCallback* aCallback)
{
  mCallback = aCallback;
}

bool
PresentationSessionChild::StartConnection()
{
  return SendEstablishConnection();
}

bool
PresentationSessionChild::PostMessageInfo(const nsACString& aMsg, bool aIsBinary)
{
  return SendMessage(nsCString(aMsg), aIsBinary);
}

bool
PresentationSessionChild::PostBinaryStreamInfo(nsIInputStream *aStream,
                                               uint32_t aLength)
{
  // TODO : Not implemented yet.
  return false;
}

bool
PresentationSessionChild::CloseConnection()
{
  return SendCloseConnection();
}

PresentationSessionState
PresentationSessionChild::State()
{
  nsCString status;
  bool ret = SendConnectionStatus(&status);
  if (!ret) {
    NS_WARNING(" SendConnectionStatus failed !!");
  }
  if (status == "connected") {
    return PresentationSessionState::Connected;
  }
  return PresentationSessionState::Disconnected;
}

END_PRESENTATION_NAMESPACE
