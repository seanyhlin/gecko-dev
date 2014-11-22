/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/PresentationSession.h"
#include "mozilla/dom/PresentationSessionBinding.h"
#include "mozilla/dom/PresentationSessionChild.h"
#include "mozilla/dom/NavigatorPresentation.h"
#include "mozilla/dom/NavigatorPresentationChild.h"

#include "nsIDOMMessageEvent.h"
#include "PresentationManager.h"
#include "PresentationSessionInternal.h"
#include "PresentationSocketChannel.h"

BEGIN_PRESENTATION_NAMESPACE

NS_IMPL_ADDREF_INHERITED(PresentationSession, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(PresentationSession, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PresentationSession)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(PresentationSession)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PresentationSession,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNaviPresentation)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChannel)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PresentationSession,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNaviPresentation)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChannel)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

PresentationSession::PresentationSession(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
  , mNaviPresentation(nullptr)
  , mISInternal(nullptr)
  , mSessionState(PresentationSessionState::Disconnected)
{
}

PresentationSession::~PresentationSession()
{
  mCallback = nullptr;
  mISInternal = nullptr;
  mNaviPresentation = nullptr;
  mChannel = nullptr;
}

JSObject*
PresentationSession::WrapObject(JSContext* aCx)
{
  return PresentationSessionBinding::Wrap(aCx, this);
}

void
PresentationSession::ConnectISessionInternal()
{
  if (!mISInternal)
  {
    if (XRE_GetProcessType() != GeckoProcessType_Default) {
      mISInternal = static_cast<ISessionInternal*>
        (new PresentationSessionChild(mURL,
                                      mSessionId,
                                      mDeviceId));
    }
    else {
      mISInternal = static_cast<ISessionInternal*>
        (GetIManageSessionInternal()->GetPSInternal(mURL,
                                                    mSessionId,
                                                    mDeviceId));
    }
    mCallback = new PresentationSessionCallback(this);
    MOZ_ASSERT(mISInternal, "A ISessionInternal should be created !");
    mISInternal->SetCallback(mCallback);
    return;
  }
}

void
PresentationSession::CreateSocketChannel()
{
  if (!mChannel) {
    mChannel = new PresentationSocketChannel(GetOwner(), this);
    MOZ_ASSERT(mChannel, " Create PresentationSocketChannel failed! ");
  }
}

void
PresentationSession::InitSessionInfo(NavigatorPresentation* aNP,
                                     const nsACString& aOrigin,
                                     const nsACString& aUrl,
                                     const nsACString& aSessionId,
                                     const nsACString& aDeviceId)
{
  mNaviPresentation = aNP;
  mOrigin = aOrigin;
  mURL = aUrl;
  mSessionId = aSessionId;
  mDeviceId = aDeviceId;
  ConnectISessionInternal();
  mSessionState = mISInternal->State();
  CreateSocketChannel();
}

void
PresentationSession::StartSessionConnection()
{
  MOZ_ASSERT(mISInternal, "should get a ISessionInternal !");
  bool ret = mISInternal->StartConnection();
  if (!ret) {
    NS_WARNING("StartSessionConnection failed !");
  }
}

void
PresentationSession::CloseSessionConnection()
{
  MOZ_ASSERT(mISInternal, "should get a ISessionInternal !");
  bool ret = mISInternal->CloseConnection();
  if (!ret) {
    NS_WARNING("StartSessionConnection failed !");
    return;
  }
  mSessionState = PresentationSessionState::Disconnected;
}

void
PresentationSession::ResetSessionResource()
{
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    delete mISInternal;
  }
  mISInternal = nullptr;
  mURL = NS_LITERAL_CSTRING("");
  mSessionId = NS_LITERAL_CSTRING("");
  mDeviceId = NS_LITERAL_CSTRING("");
}

void
PresentationSession::NotifyStateChanged(PresentationSessionState aState)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  MOZ_ASSERT(mNaviPresentation, " mNaviPresentation should exist !!");
  MOZ_ASSERT(mChannel, " mChannel should exist !!");

  mSessionState = aState;
  // Resolving promise first if there is one, then notify statechange
  // for presentation session.
  nsresult rv = mNaviPresentation->AnswerPromise(mOrigin,
                                                 mURL,
                                                 mSessionId);
  if (NS_FAILED(rv)) {
    NS_WARNING(" AnswerPromise goes unexpected way !! Please check.");
  }
  nsRefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(this, NS_LITERAL_STRING("statechange"), false);
  asyncDispatcher->PostDOMEvent();
}

void
PresentationSession::NotifyChannelStatus(EChannelState aChannelState)
{
  MOZ_ASSERT(mChannel, " mChannel should exist !!");
  // TODO : Need to figure out the difference between Channel Status &
  //        Session State.
  switch (aChannelState) {
    case EChannelState::CONNECTING:
      break;
    case EChannelState::OPEN:
      mChannel->NotifyOpen();
      break;
    case EChannelState::CLOSING:
      mChannel->NotifyError();
      break;
    case EChannelState::CLOSED:
      mChannel->NotifyClose();
      ResetResourceRunnalbe* runnable = new ResetResourceRunnalbe(this);
      NS_DispatchToMainThread(runnable);
      break;
  }
}

void
PresentationSession::NotifyMessageRecieved(const nsACString& aData,
                                           bool aIsBinary)
{
  MOZ_ASSERT(mChannel, " mChannel should exist !!");
  mChannel->NotifyMessage(aData, aIsBinary);
}

void
PresentationSession::GetId(nsString& aId) const
{
  aId = NS_ConvertUTF8toUTF16(mSessionId);
}

void
PresentationSession::PostMessage(const nsACString& aMessage, bool aIsBinary)
{
  MOZ_ASSERT(mISInternal, "should get a ISessionInternal !");
  bool ret = mISInternal->PostMessageInfo(aMessage, aIsBinary);
  if (!ret) {
    NS_WARNING("PostMessageInfo failed !");
  }
}

already_AddRefed<PresentationSocketChannel>
PresentationSession::Channel()
{
  CreateSocketChannel();
  nsRefPtr<PresentationSocketChannel> channel = mChannel;
  return channel.forget();
}

END_PRESENTATION_NAMESPACE
