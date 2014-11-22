
#include "mozilla/Assertions.h"
#include "mozilla/dom/PresentationSessionBinding.h"

#include "PresentationManager.h"
#include "PresentationSessionInternal.h"

BEGIN_PRESENTATION_NAMESPACE

PresentationSessionCallback::PresentationSessionCallback(ISessionCallback* aPSession)
  : mPresentationSession(aPSession) {
}

void
PresentationSessionCallback::NotifyStateChanged(PresentationSessionState aState)
{
  mPresentationSession->NotifyStateChanged(aState);
}

void
PresentationSessionCallback::NotifyChannelStatus(EChannelState aChannelState)
{
  mPresentationSession->NotifyChannelStatus(aChannelState);
}

void
PresentationSessionCallback::NotifyMessageRecieved(const nsACString& aData,
                                                   bool aIsBinary)
{
  mPresentationSession->NotifyMessageRecieved(aData, aIsBinary);
}



NS_IMPL_CYCLE_COLLECTING_ADDREF(PresentationSessionInternal)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PresentationSessionInternal)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PresentationSessionInternal)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(PresentationSessionInternal)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(PresentationSessionInternal)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(PresentationSessionInternal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(PresentationSessionInternal)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

PresentationSessionInternal::PresentationSessionInternal()
  : mCallback(nullptr)
  , mPresentationDevice(nullptr)
  , mPSSocketChannelInternal(nullptr)
  , mSessionStateInternal(PresentationSessionState::Disconnected)
{

}

PresentationSessionInternal::~PresentationSessionInternal()
{
  mPresentationDevice = nullptr;
  mPSSocketChannelInternal = nullptr;
  mCallback = nullptr;
}

void
PresentationSessionInternal::SetupPSIInfo(const nsACString& aOrigin,
                                          const nsACString& aURL,
                                          const nsACString& aSessionId,
                                          nsIPresentationDevice* aDevice)
{
  mOrigin = aOrigin;
  mURL = aURL;
  mSessionId = aSessionId;
  if (aDevice) {
    mPresentationDevice = aDevice;
    mPresentationDevice->GetId(mDeviceId);
  }
}

void
PresentationSessionInternal::GetURL(nsACString& aURL)
{
  aURL.Assign(mURL);
}
void
PresentationSessionInternal::GetSessionId(nsACString& aSessionId)
{
  aSessionId.Assign(mSessionId);
}

void
PresentationSessionInternal::GetDeviceId(nsACString& aDeviceId)
{
  aDeviceId.Assign(mDeviceId);
}
void
PresentationSessionInternal::GetDeviceName(nsACString& aDeviceName)
{
  NS_ENSURE_TRUE_VOID(mPresentationDevice);
  mPresentationDevice->GetName(aDeviceName);
}
void
PresentationSessionInternal::GetDeviceType(nsACString& aDeviceType)
{
  NS_ENSURE_TRUE_VOID(mPresentationDevice);
  mPresentationDevice->GetType(aDeviceType);
}

void
PresentationSessionInternal::SetCallback(ISessionCallback* aCallback)
{
  mCallback = aCallback;
}

bool
PresentationSessionInternal::StartConnection()
{
  MOZ_ASSERT(mPresentationDevice, "Presentation Device should exist !!");
  MOZ_ASSERT(!mPSSocketChannelInternal, "SocketChannelInternal should be nullptr!");

  mPSSocketChannelInternal = new PresentationSocketChannelInternal();
  MOZ_ASSERT(mPSSocketChannelInternal);

  nsresult rv;
  nsCOMPtr<nsIPresentationSessionTransport> psTransport;
  rv = mPresentationDevice->EstablishSessionTransport(NS_ConvertUTF8toUTF16(mURL),
                                                      NS_ConvertUTF8toUTF16(mSessionId),
                                                      getter_AddRefs(psTransport));
  rv = mPSSocketChannelInternal->SetupSocketChannel(this, psTransport);
  if (NS_FAILED(rv)) {
    NS_WARNING(" PresentationSocketChannel setup failed, unable to communicate !");
    return false;
  }
  return true;
}

bool
PresentationSessionInternal::PostMessageInfo(const nsACString& aMsg,
                                             bool aIsBinary)
{
  MOZ_ASSERT(mPSSocketChannelInternal);
  nsresult rv = mPSSocketChannelInternal->PostMessage(aMsg, aIsBinary);
  if (NS_FAILED(rv)) {
    NS_WARNING(" PresentationSocketChannel PostMessageInfo failed !");
    return false;
  }
  return true;
}

bool
PresentationSessionInternal::PostBinaryStreamInfo(nsIInputStream *aStream,
                                                  uint32_t aLength)
{
  // TODO : Not implemented yet.
  return false;
}

bool
PresentationSessionInternal::CloseConnection()
{
  mSessionStateInternal = PresentationSessionState::Disconnected;
  if (mPSSocketChannelInternal) {
    mPSSocketChannelInternal->TeardownSocketChannel();
  }
  return true;
}

void
PresentationSessionInternal::CallbackConnectionState(PresentationSessionState aState)
{
  MOZ_ASSERT(mCallback);
  if (mSessionStateInternal != PresentationSessionState::Disconnected &&
      aState == PresentationSessionState::Disconnected &&
      mPSSocketChannelInternal) {
    mPSSocketChannelInternal->TeardownSocketChannel();
  }
  mSessionStateInternal = aState;
  mCallback->NotifyStateChanged(aState);
}

void
PresentationSessionInternal::CallbackChannelStatus(EChannelState aChannelState)
{
  MOZ_ASSERT(mCallback);
  mCallback->NotifyChannelStatus(aChannelState);

  if (aChannelState == EChannelState::CLOSED) {
    IManageSessionInternal* iManageSessionInternal = GetIManageSessionInternal();
    if (iManageSessionInternal) {
      iManageSessionInternal->ReleasePSInternal(mURL, mSessionId, mDeviceId);
    }
  }
}

void
PresentationSessionInternal::CallbackMessageAvailable(const nsACString& aMsg,
                                                      bool aIsBinary)
{
  MOZ_ASSERT(mCallback);
  mCallback->NotifyMessageRecieved(aMsg, aIsBinary);
}

END_PRESENTATION_NAMESPACE
