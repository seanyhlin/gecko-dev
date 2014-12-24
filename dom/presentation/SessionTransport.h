#include "nsIPresentationControlChannel.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {
namespace presentation {

class SessionTransport : public nsISupports
{
public:
  NS_DECL_ISUPPORTS

  SessionTransport(nsIPresentationControlChannel* aChannel)
    : mCtrlChannel(aChannel)
  {
  }

  ~SessionTransport();
private:
  nsCOMPtr<nsIPresentationControlChannel> mCtrlChannel;
};

} // namespace presentation
} // namespace dom
} // namespace mozilla
