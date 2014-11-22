#ifndef PRESENTATIONCOMMON_H_
#define PRESENTATIONCOMMON_H_

#include "mozilla/dom/PresentationSessionBinding.h"
#include "mozilla/Observer.h"

#undef PRESENT_LOG
#if defined(ANDROID)
#include <android/log.h>
#define PRESENT_LOG(args...) \
  __android_log_print(ANDROID_LOG_INFO, \
                      "Presentation", \
                      ## args)
#else
#define PRESENT_LOG(args...) printf(args)
#endif


#define BEGIN_PRESENTATION_NAMESPACE \
  namespace mozilla { namespace dom {
#define END_PRESENTATION_NAMESPACE \
  } /* namespace dom */ } /* namespace mozilla */

BEGIN_PRESENTATION_NAMESPACE

enum EChannelState{
  CONNECTING = 0,
  OPEN       = 1,
  CLOSING    = 2,
  CLOSED     = 3
};

enum EPresentationEvent
{
  DeviceAvailableChanged,
  DeviceSelected,
  SessionStateChanged
};

struct DeviceInformation {
  nsCString mOrigin;
  nsCString mURL;
  nsCString mSessionId;
  nsCString mId;
  nsCString mName;
  nsCString mType;
};

struct PresentationEvent
{
  EPresentationEvent eventType;
  union {
    bool available;
    PresentationSessionState psState;
  };
  DeviceInformation deviceInfo;
};

typedef mozilla::Observer<PresentationEvent>     PresentationEventObserver;
typedef mozilla::ObserverList<PresentationEvent> PresentationEventObserverList;

END_PRESENTATION_NAMESPACE

#endif
