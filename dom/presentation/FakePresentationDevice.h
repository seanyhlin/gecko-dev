#ifndef mozilla_dom_fakepresentationmanager_h__
#define mozilla_dom_fakepresentationmanager_h__

#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"

#include "nsIPresentationDevice.h"
#include "nsIPresentationControlChannel.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {
namespace presentation {

class MockPresentationDevice : public nsIPresentationDevice
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONDEVICE
  MockPresentationDevice()
    : mId(NS_LITERAL_CSTRING("testID"))
    , mName(NS_LITERAL_CSTRING("testName"))
    , mType(NS_LITERAL_CSTRING("testType"))
  {}
private:
  virtual ~MockPresentationDevice();
  nsCString mId;
  nsCString mName;
  nsCString mType;
};

}
}
}

#endif
