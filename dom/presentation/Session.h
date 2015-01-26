#ifndef mozilla_dom_presentation_Session_h
#define mozilla_dom_presentation_Session_h

#include "nsIPresentationControlChannel.h"
#include "nsIPresentationSessionTransport.h"
#include "nsRefPtr.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "mozilla/dom/presentation/PresentationService.h"

namespace mozilla {
namespace dom {
namespace presentation {

class PresentationService;

class Session : public nsIPresentationControlChannelListener
              , public nsIPresentationSessionTransportCallback
{
public:
  NS_DECL_ISUPPORTS

  static already_AddRefed<Session>
  CreateRequester(const nsAString& aId,
                  nsIPresentationControlChannel* aControlChannel,
                  PresentationService* aService);

  static already_AddRefed<Session>
  CreateResponder(const nsAString& aId,
                  nsIPresentationControlChannel* aControlChannel,
                  PresentationService* aService);

  void
  Send() {}

  void
  Close() {}

  const nsAString& Id() const
  {
    return mId;
  }

protected:
  explicit Session(const nsAString& aId,
                   nsIPresentationControlChannel* aControlChannel,
                   PresentationService* aService);
  virtual ~Session();

  nsCOMPtr<nsIPresentationControlChannel> mControlChannel;
  nsRefPtr<PresentationService> mService;

private:
  nsString mId;
//  nsCOMPtr<nsIPresentationDevice> mDevice; // joinSession
  nsCOMPtr<nsIPresentationSessionTransport> mTransport;
};

}
}
}

#endif
