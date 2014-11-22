#ifndef mozilla_dom_PresentationDeviceRequest_h__
#define mozilla_dom_PresentationDeviceRequest_h__

#include "nsIPresentationDevicePrompt.h"
#include "PresentationCommon.h"

BEGIN_PRESENTATION_NAMESPACE

class ICallbackDeviceSelection;

class nsPresentationDeviceRequest : public nsIPresentationDeviceRequest
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONDEVICEREQUEST

  nsPresentationDeviceRequest();
  void SetupRequest(ICallbackDeviceSelection* aICallback,
                    const nsACString& aOrigin,
                    const nsACString& aRequestURL,
                    const nsACString& aLastSessionId);

private:
  virtual ~nsPresentationDeviceRequest();
  ICallbackDeviceSelection* mICallback;
  nsCString mOrigin;
  nsCString mRequestURL;
  nsCString mLastSessionId;
};

END_PRESENTATION_NAMESPACE
#endif
