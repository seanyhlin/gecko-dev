#include "mozilla/dom/PresentationUtil.h"
#include "PresentationSessionParent.h"

BEGIN_PRESENTATION_NAMESPACE

bool
PresentationSessionParent::RecvMessage(const nsCString& aInfoMsg,
                                       const bool& aIsBinary)
{
  MOZ_ASSERT(mPSInternal, "PresentaionSessionInternal should exist !!!");
  return mPSInternal->PostMessageInfo(aInfoMsg, aIsBinary);
}

bool
PresentationSessionParent::RecvBinaryStream(const InputStreamParams& aStream,
                                            const uint32_t& aLength)
{
  MOZ_ASSERT(mPSInternal, "PresentaionSessionInternal should exist !!!");
  // TODO : Not implemented yet.
  return false;
}

bool
PresentationSessionParent::RecvEstablishConnection()
{
  MOZ_ASSERT(mPSInternal, "PresentaionSessionInternal should exist !!!");
  return mPSInternal->StartConnection();
}

bool
PresentationSessionParent::RecvCloseConnection()
{
  MOZ_ASSERT(mPSInternal, "PresentaionSessionInternal should exist !!!");
  return mPSInternal->CloseConnection();
}

bool
PresentationSessionParent::RecvConnectionStatus(nsCString* aConnectStatus)
{
  MOZ_ASSERT(mPSInternal, "PresentaionSessionInternal should exist !!!");
  if (mPSInternal->State() == PresentationSessionState::Connected) {
    *aConnectStatus = NS_LITERAL_CSTRING("connected");
  } else {
    *aConnectStatus = NS_LITERAL_CSTRING("disconnected");
  }
  return true;
}

void
PresentationSessionParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

PresentationSessionParent::PresentationSessionParent(const nsCString& aURL,
                                                     const nsCString& aSessionId,
                                                     const nsCString& aDeviceId)
  : mURL(aURL)
  , mSessionId(aSessionId)
  , mDeviceId(aDeviceId)
  , mPSInternal(nullptr)
  , mCallback(nullptr)
{
    MOZ_COUNT_CTOR(PresentationSessionParent);
    mPSInternal = GetIManageSessionInternal()->GetPSInternal(mURL,
                                                             mSessionId,
                                                             mDeviceId);
    NS_ENSURE_TRUE_VOID(mPSInternal);
    mCallback = new PresentationSessionCallback(this);
    NS_ENSURE_TRUE_VOID(mCallback);
    mPSInternal->SetCallback(mCallback);
}

MOZ_IMPLICIT PresentationSessionParent::~PresentationSessionParent()
{
    MOZ_COUNT_DTOR(PresentationSessionParent);
    mPSInternal->SetCallback(nullptr);
    mPSInternal = nullptr;
}

void
PresentationSessionParent::NotifyStateChanged(PresentationSessionState aState)
{
  bool ret = false;
  if (aState == PresentationSessionState::Connected) {
    ret = SendUpdateConnectionStatus(NS_LITERAL_CSTRING("connected"));
  } else if (aState == PresentationSessionState::Disconnected) {
    ret = SendUpdateConnectionStatus(NS_LITERAL_CSTRING("disconnected"));
  } else {
    ret = SendUpdateConnectionStatus(NS_LITERAL_CSTRING("unknown"));
  }
  NS_WARN_IF_FALSE(ret, "SendUpdateConnectionStatus to Child failed !!");
}

void
PresentationSessionParent::NotifyChannelStatus(EChannelState aChannelState)
{
  bool ret = SendUpdateChannelStatus((uint32_t)aChannelState);
  NS_WARN_IF_FALSE(ret, "SendUpdateChannelStatus to Child failed !!");
}

void
PresentationSessionParent::NotifyMessageRecieved(const nsACString& aMsg, bool aIsBinary)
{
  bool ret = SendMessage(nsCString(aMsg), aIsBinary);
  NS_WARN_IF_FALSE(ret, "SendMessage to Child failed !!");
}

END_PRESENTATION_NAMESPACE
