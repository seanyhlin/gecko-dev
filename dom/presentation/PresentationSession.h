/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationSession_h
#define mozilla_dom_PresentationSession_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/PresentationSessionBinding.h"
#include "mozilla/ErrorResult.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

#include "PresentationCommon.h"
#include "PresentationSessionInternal.h"

struct JSContext;

BEGIN_PRESENTATION_NAMESPACE

class ISessionInternal;
class NavigatorPresentation;
class PresentationSocketChannel;

class PresentationSession MOZ_FINAL : public DOMEventTargetHelper
                                    , public ISessionCallback
{
  friend class NavigatorPresentation;
  friend class PresentationSocketChannel;
public:
  class ResetResourceRunnalbe MOZ_FINAL : public nsRunnable
  {
  public:
    ResetResourceRunnalbe(PresentationSession* aSession)
      : mSession(aSession)
    {
    }
    virtual ~ResetResourceRunnalbe() {};
    NS_IMETHOD Run() {
      if (mSession) {
        mSession->ResetSessionResource();
      }
      return NS_OK;
    }
  private:
    PresentationSession* mSession;
  };

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PresentationSession, DOMEventTargetHelper)

  PresentationSession(nsPIDOMWindow* aWindow);
  virtual void
  NotifyStateChanged(PresentationSessionState aState) MOZ_OVERRIDE;
  virtual void
  NotifyChannelStatus(EChannelState aChannelState) MOZ_OVERRIDE;
  virtual void
  NotifyMessageRecieved(const nsACString& aData, bool aIsBinary) MOZ_OVERRIDE;

  // WebIDL
  nsPIDOMWindow*
  GetParentObject() const { return GetOwner(); }
  // WebIDL
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;
  // WebIDL
  PresentationSessionState
  State() const { return mSessionState; }
  // WebIDL
  already_AddRefed<PresentationSocketChannel>
  Channel();
  // WebIDL
  void
  GetId(nsString& aId) const;
  // WebIDL
  IMPL_EVENT_HANDLER(statechange);

private:
  virtual ~PresentationSession();

  void
  ConnectISessionInternal();

  void
  CreateSocketChannel();

  void
  InitSessionInfo(NavigatorPresentation* aNP,
                  const nsACString& aOrigin,
                  const nsACString& aUrl,
                  const nsACString& aSessionId,
                  const nsACString& aDeviceId);

  void
  StartSessionConnection();

  void
  CloseSessionConnection();

  void
  PostMessage(const nsACString& aMessage, bool aIsBinary);

  void
  ResetSessionResource();

  nsRefPtr<NavigatorPresentation> mNaviPresentation;
  nsRefPtr<PresentationSocketChannel> mChannel;
  nsAutoPtr<PresentationSessionCallback> mCallback;
  ISessionInternal* mISInternal;
  PresentationSessionState mSessionState;
  nsCString mOrigin;
  nsCString mURL;
  nsCString mSessionId;
  nsCString mDeviceId;
};

END_PRESENTATION_NAMESPACE

#endif // mozilla_dom_PresentationSession_h
