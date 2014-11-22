#ifndef mozilla_dom_presentation_util_h
#define mozilla_dom_presentation_util_h

#include "mozilla/Preferences.h"
#include "mozilla/dom/PresentationManager.h"

#include "PresentationCommon.h"

BEGIN_PRESENTATION_NAMESPACE

static IPresentationManager*
GetIPresentationManager()
{
  IPresentationManager* IPMgr =
      static_cast<IPresentationManager*>(PresentationManager::GetInstance());
  MOZ_ASSERT(IPMgr, "IPresentationManager should exist !!!");
  return IPMgr;
}

static IManageSessionInternal*
GetIManageSessionInternal()
{
  IManageSessionInternal* IMSInternal =
      static_cast<IManageSessionInternal*>(PresentationManager::GetInstance());
  MOZ_ASSERT(IMSInternal, "IManageSessionInternal should exist !!!");
  return IMSInternal;
}

END_PRESENTATION_NAMESPACE

#endif