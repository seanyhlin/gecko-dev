#include "nsIPresentationControlChannel.h"
#include "nsIPresentationDevice.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#ifndef mozilla_dom_presentation_PresentationSessionTransport_h
#define mozilla_dom_presentation_PresentationSessionTransport_h

namespace mozilla {
namespace dom {
namespace presentation {

class PresentationSessionTransport MOZ_FINAL: public nsISupports
{
public:
  NS_DECL_ISUPPORTS

  PresentationSessionTransport(const nsAString& aId,
                   nsIPresentationControlChannel* aChannel,
                   nsIPresentationDevice* aDevice)
    : mId(aId)
    , mCtrlChannel(aChannel)
    , mDevice(aDevice)
  {
  }

  ~PresentationSessionTransport()
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

#endif
