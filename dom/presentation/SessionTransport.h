#include "nsIPresentationControlChannel.h"
#include "nsIPresentationDevice.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsString.h"

namespace mozilla {
namespace dom {
namespace presentation {

class SessionTransport MOZ_FINAL: public nsISupports
{
public:
  NS_DECL_ISUPPORTS

  SessionTransport(const nsAString& aId,
                   nsIPresentationControlChannel* aChannel,
                   nsIPresentationDevice* aDevice)
    : mId(aId)
    , mCtrlChannel(aChannel)
    , mDevice(aDevice)
  {
  }

  ~SessionTransport()
  {
    mCtrlChannel = nullptr;
    mDevice = nullptr;
  }

  nsString mId;
  nsCOMPtr<nsIPresentationControlChannel> mCtrlChannel;
  nsCOMPtr<nsIPresentationDevice> mDevice;
};

} // namespace presentation
} // namespace dom
} // namespace mozilla
