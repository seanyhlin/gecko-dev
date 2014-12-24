#include "SessionTransport.h"

using namespace mozilla::dom;
using namespace mozilla::dom::presentation;

NS_IMPL_ISUPPORTS0(SessionTransport)

SessionTransport::~SessionTransport()
{
  mCtrlChannel = nullptr;
}
