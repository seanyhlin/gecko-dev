#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"
#include "PresentationSessionInternal.h"
#include "PresentationSocketChannelInternal.h"

BEGIN_PRESENTATION_NAMESPACE

NS_IMPL_CYCLE_COLLECTING_ADDREF(PresentationSocketChannelInternal)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PresentationSocketChannelInternal)

NS_IMPL_CYCLE_COLLECTION(PresentationSocketChannelInternal,
                         mPSTransport,
                         mSessionInternal)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PresentationSocketChannelInternal)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPresentationSessionTransportListener)
NS_INTERFACE_MAP_END

PresentationSocketChannelInternal::PresentationSocketChannelInternal()
  : mPSTransport(nullptr)
  , mSessionInternal(nullptr)
  , mIsTearingdown(false)
{
}

PresentationSocketChannelInternal::~PresentationSocketChannelInternal()
{
  mPSTransport = nullptr;
  mSessionInternal = nullptr;
}

nsresult
PresentationSocketChannelInternal::
  SetupSocketChannel(PresentationSessionInternal* aSession,
                     nsIPresentationSessionTransport* aPSTransport)
{
  NS_ENSURE_TRUE(aSession, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(aPSTransport, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(!mPSTransport, NS_ERROR_UNEXPECTED);
  NS_ENSURE_TRUE(!mSessionInternal, NS_ERROR_UNEXPECTED);
  mPSTransport = aPSTransport;
  mPSTransport->SetListener(this);
  mSessionInternal = aSession;
  return NS_OK;
}

void
PresentationSocketChannelInternal::TeardownSocketChannel()
{
  // Cut off the binding between SocketChannelInternal & SessionTrasnport.
  // So notify closed to SockeChannel by SocketChannelInternal itself.
  if (mPSTransport) {
    mIsTearingdown = true;
    mPSTransport->Close();
    mPSTransport->SetListener(nullptr);
    mPSTransport = nullptr;
    NotifyCloseRunnable* runnable = new NotifyCloseRunnable(this);
    NS_DispatchToMainThread(runnable);
  }
}

nsresult
PresentationSocketChannelInternal::PostMessage(const nsACString& aMsg,
                                               bool aIsBinary)
{
  MOZ_ASSERT(mPSTransport);
  nsresult rv;
  if (aIsBinary) {
    rv = mPSTransport->PostBinaryMessage(nsCString(aMsg));
  } else {
    rv = mPSTransport->PostMessage(NS_ConvertUTF8toUTF16(aMsg));
  }
  if (NS_FAILED(rv)) {
    NS_WARNING(" Post message to device failed !! ");
  }
  return rv;
}

NS_IMETHODIMP
PresentationSocketChannelInternal::OnMessage(const nsAString& aMsg)
{
  NS_ENSURE_TRUE(mSessionInternal, NS_ERROR_NOT_AVAILABLE);

  nsAutoCString message = NS_ConvertUTF16toUTF8(aMsg);
  bool isBinary = false;
  mSessionInternal->CallbackMessageAvailable(message, isBinary);
  return NS_OK;
}

NS_IMETHODIMP
PresentationSocketChannelInternal::OnBinaryMessage(const nsACString& aMsg)
{
  NS_ENSURE_TRUE(mSessionInternal, NS_ERROR_NOT_AVAILABLE);

  nsAutoCString message(aMsg);
  bool isBinary = true;
  mSessionInternal->CallbackMessageAvailable(message, isBinary);
  return NS_OK;
}

NS_IMETHODIMP
PresentationSocketChannelInternal::NotifyOpened()
{
  NS_ENSURE_TRUE(mSessionInternal, NS_ERROR_NOT_AVAILABLE);

  mSessionInternal->CallbackConnectionState(PresentationSessionState::Connected);
  mSessionInternal->CallbackChannelStatus(EChannelState::OPEN);
  return NS_OK;
}
NS_IMETHODIMP
PresentationSocketChannelInternal::NotifyClosed(nsresult aRet)
{
  NS_ENSURE_TRUE(mSessionInternal, NS_ERROR_NOT_AVAILABLE);

  if (NS_FAILED(aRet)) {
    mSessionInternal->CallbackChannelStatus(EChannelState::CLOSING);
  }
  mSessionInternal->CallbackConnectionState(PresentationSessionState::Disconnected);
  mSessionInternal->CallbackChannelStatus(EChannelState::CLOSED);

  if (mIsTearingdown) {
    mSessionInternal = nullptr;
  }
  return NS_OK;
}

END_PRESENTATION_NAMESPACE
