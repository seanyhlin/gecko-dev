/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AsyncEventDispatcher.h"

#include "mozilla/dom/AvailableChangeEvent.h"
#include "mozilla/dom/AvailableChangeEventBinding.h"
#include "mozilla/dom/NavigatorPresentation.h"
#include "mozilla/dom/NavigatorPresentationBinding.h"
#include "mozilla/dom/NavigatorPresentationChild.h"
#include "mozilla/dom/PresentationUtil.h"
#include "mozilla/Services.h"

#include "nsIDOMCustomEvent.h"
#include "nsIPrincipal.h"

using namespace mozilla;

BEGIN_PRESENTATION_NAMESPACE

NS_IMPL_ADDREF_INHERITED(NavigatorPresentation, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(NavigatorPresentation, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(NavigatorPresentation)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(NavigatorPresentation)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(NavigatorPresentation,
                                                  DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(NavigatorPresentation,
                                                DOMEventTargetHelper)
  tmp->Shutdown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSession)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

#define PRESENTATION_REQUEST_PARAMS "presentation-request-params"

NavigatorPresentation::NavigatorPresentation(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
  , mPresentationManager(nullptr)
  , mAvailable(false)
  , mIsPresenter(false)
{
}

NavigatorPresentation::~NavigatorPresentation()
{
  Shutdown();
}

JSObject*
NavigatorPresentation::WrapObject(JSContext* aCx)
{
  return NavigatorPresentationBinding::Wrap(aCx, this);
}

void
NavigatorPresentation::Init()
{
  CreateIPresentationManager();
  MOZ_ASSERT(mPresentationManager, " mPresentationManager should exist !");
  AddEventListener(NS_LITERAL_STRING(PRESENTATION_REQUEST_PARAMS),
                                     this, true, true, 2);
  mPresentationManager->AddObserver(this);
}

void
NavigatorPresentation::Shutdown()
{
  RemoveEventListener(NS_LITERAL_STRING(PRESENTATION_REQUEST_PARAMS), this, false);
  if (mPresentationManager) {
    mPresentationManager->RemoveObserver(this);
    if (XRE_GetProcessType() != GeckoProcessType_Default) {
      delete mPresentationManager;
    }
    mPresentationManager = nullptr;
  }
  if (mSession) {
    mSession->CloseSessionConnection();
    mSession = nullptr;
  }
  mSessionStore.Clear();
  mPromiseStore.Clear();
}

void
NavigatorPresentation::Notify(const PresentationEvent& aEvent)
{
  switch (aEvent.eventType) {
    case DeviceAvailableChanged:
      NotifyAvailableChange(aEvent.available);
      break;
    case DeviceSelected: {
      NotifySessionInitInfo(aEvent.deviceInfo.mOrigin,
                            aEvent.deviceInfo.mURL,
                            aEvent.deviceInfo.mSessionId,
                            aEvent.deviceInfo.mId);
      break;
    }
    default:
      break;
  }
}

void
NavigatorPresentation::NotifyAvailableChange(bool aAvailable)
{
  MOZ_ASSERT(NS_IsMainThread(), "Should in main thread");
  mAvailable = aAvailable;
  AvailableChangeEventInit init;
  init.mAvailable = aAvailable;
  nsRefPtr<AvailableChangeEvent> event =
      AvailableChangeEvent::Constructor(this,
                                        NS_LITERAL_STRING("availablechange"),
                                        init);
  DispatchTrustedEvent(event);
}

void
NavigatorPresentation::NotifySessionInitInfo(const nsACString& aOrigin,
                                             const nsACString& aURL,
                                             const nsACString& aSessionId,
                                             const nsACString& aDeviceId)
{
  // Get notified after device selected / canceled from UI prompt
  nsAutoCString sessionKey;
  sessionKey = aURL + aSessionId;
  MOZ_ASSERT(!mSessionStore.Contains(sessionKey), "Session should not in store !!");

  if (aDeviceId.IsEmpty()) {
    // User cancel selecting or Can not find a session to join for now.
    // TODO : Maybe to reject the promise.
    return;
  }

  nsRefPtr<PresentationSession> session = new PresentationSession(GetOwner());
  MOZ_ASSERT(session, "Session can't be created correctly !");
  mSessionStore.Put(sessionKey, session);
  session->InitSessionInfo(this,
                           aOrigin,
                           aURL,
                           aSessionId,
                           aDeviceId);

  session->StartSessionConnection();
  session = nullptr;
}

nsresult
NavigatorPresentation::AnswerPromise(const nsACString& aOrigin,
                                     const nsACString& aURL,
                                     const nsACString& aSessionId)
{
  nsAutoCString promiseKey;
  promiseKey = aOrigin + aURL;

  if (mPromiseStore.Contains(promiseKey)) {
    nsRefPtr<Promise> promise;
    mPromiseStore.Remove(promiseKey, getter_AddRefs(promise));

    nsAutoCString sessionKey;
    sessionKey = aURL + aSessionId;
    PresentationSession* session = mSessionStore.GetWeak(sessionKey);
    if (session) {
      nsRefPtr<PresentationSession> refSession = session;
      promise->MaybeResolve(refSession);
      session = nullptr;
    } else {
      promise->MaybeReject(NS_ERROR_UNEXPECTED);
      return NS_ERROR_UNEXPECTED;
    }
  }
  return NS_OK;
}

void
NavigatorPresentation::CreateIPresentationManager()
{
  if (!mPresentationManager)
  {
    if (XRE_GetProcessType() != GeckoProcessType_Default) {
      mPresentationManager =
          static_cast<IPresentationManager*>(new NavigatorPresentationChild());
    }
    else {
      mPresentationManager = GetIPresentationManager();
    }
  }
  if (!mPresentationManager) {
    NS_WARNING("Fail to create mPresentationManager !");
  }
}

already_AddRefed<Promise>
NavigatorPresentation::StartSession(const nsAString& aUrl,
                                    const nsAString& aSessionId,
                                    ErrorResult& aRv)
{
  MOZ_ASSERT(mPresentationManager, " mPresentationManager should exist !");
  MOZ_ASSERT(GetOwner());
  if (mIsPresenter) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsAutoCString url = NS_ConvertUTF16toUTF8(aUrl);
  nsAutoCString sessionId = NS_ConvertUTF16toUTF8(aSessionId);

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  nsCOMPtr<nsIPrincipal> principal = global->PrincipalOrNull();
  nsAutoCString pOrigin;
  if (principal) {
    principal->GetOrigin(getter_Copies(pOrigin));
  }

  nsAutoCString promiseKey;
  promiseKey = pOrigin + url;
  if (!mPromiseStore.Contains(promiseKey)) {
    nsRefPtr<Promise> promise = Promise::Create(global, aRv);
    NS_ENSURE_TRUE(!aRv.Failed(), nullptr);
    mPromiseStore.Put(promiseKey, promise);
    mPresentationManager->StartPresentationSession(pOrigin,
                                                   url,
                                                   sessionId);
  }
  Promise* p = mPromiseStore.GetWeak(promiseKey);
  nsRefPtr<Promise> refPromise = p;
  return refPromise.forget();
}

already_AddRefed<Promise>
NavigatorPresentation::JoinSession(const nsAString& aUrl,
                                   const nsAString& aSessionId,
                                   ErrorResult& aRv)
{
  MOZ_ASSERT(mPresentationManager,
             " mPresentationManager should exist !");
  MOZ_ASSERT(GetOwner());
  if (mIsPresenter) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  nsCOMPtr<nsIPrincipal> principal = global->PrincipalOrNull();
  nsAutoCString pOrigin;
  if (principal) {
    principal->GetOrigin(getter_Copies(pOrigin));
  }

  nsAutoCString url = NS_ConvertUTF16toUTF8(aUrl);
  nsAutoCString sessionId = NS_ConvertUTF16toUTF8(aSessionId);

  nsAutoCString promiseKey;
  nsAutoCString sessionKey;
  promiseKey = pOrigin + url;
  sessionKey = url + sessionId;

  if (mPromiseStore.Contains(promiseKey)) {
    if (mSessionStore.Contains(sessionKey)) {
      // A promise is generated before by StartSession/JoinSession, and
      // already done device selection (waiting connection status), so hold !
    } else {
      // A promise is generated before by StartSession/JoinSession, and not
      // done device selection. Nothing to do.
    }
  } else {
    nsRefPtr<Promise> promise = Promise::Create(global, aRv);
    NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

    mPromiseStore.Put(promiseKey, promise);
    if (!mSessionStore.Contains(sessionKey)) {
      mPresentationManager->JoinPresentationSession(pOrigin,
                                                    url,
                                                    sessionId);
    } else {
      PresentationSession* session = mSessionStore.GetWeak(sessionKey);
      nsRefPtr<PresentationSession> refSession = session;
      promise->MaybeResolve(refSession);
      session = nullptr;
    }
  }

  Promise* p = mPromiseStore.GetWeak(promiseKey);
  nsRefPtr<Promise> refPromise = p;
  return refPromise.forget();
}

already_AddRefed<PresentationSession>
NavigatorPresentation::GetSession()
{
  if (!mSession || mSession->State() != PresentationSessionState::Connected) {
    return nullptr;
  }
  nsRefPtr<PresentationSession> session = mSession;
  return session.forget();
}

void
NavigatorPresentation::CreateSessionOnPresenter(const nsACString& aURL,
                                                const nsACString& aSessionId,
                                                const nsACString& aDeviceId)
{
  mIsPresenter = true;
  if (!mSession) {
    // 1st screen, nullptr,
    // 2nd screen, return mSession
    nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
    if (!global) {
      return;
    }
    nsCOMPtr<nsIPrincipal> principal = global->PrincipalOrNull();
    nsAutoCString pOrigin;
    if (principal) {
      principal->GetOrigin(getter_Copies(pOrigin));
    }

    nsAutoCString sessionKey;
    sessionKey = aURL + aSessionId;
    PresentationSession* session = mSessionStore.GetWeak(sessionKey);
    if (session) {
      mSession = session;
    } else {
      mSession = new PresentationSession(GetOwner());
      mSessionStore.Put(sessionKey, session);
    }
    mSession->InitSessionInfo(this,
                              pOrigin,
                              aURL,
                              aSessionId,
                              aDeviceId);
  }
}

NS_IMETHODIMP
NavigatorPresentation::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString type;
  aEvent->GetType(type);
  if (type.EqualsLiteral(PRESENTATION_REQUEST_PARAMS)) {
    nsCOMPtr<nsIDOMCustomEvent> customEvent = do_QueryInterface(aEvent);
    nsCOMPtr<nsIVariant> variant;
    customEvent->GetDetail(getter_AddRefs(variant));

    uint16_t dataType;
    nsresult rv = variant->GetDataType(&dataType);
    if (dataType == nsIDataType::VTYPE_ARRAY) {
      uint16_t eltType;
      nsIID eltIID;
      uint32_t arrayLen;
      void *array;
      rv = variant->GetAsArray(&eltType, &eltIID, &arrayLen, &array);
      NS_ENSURE_SUCCESS(rv, rv);
      char16_t **elms = reinterpret_cast<char16_t **>(array);
      NS_ENSURE_TRUE(arrayLen == 3, NS_ERROR_UNEXPECTED);

      nsAutoCString url = NS_ConvertUTF16toUTF8(nsDependentString(elms[0]));
      nsAutoCString sessionId = NS_ConvertUTF16toUTF8(nsDependentString(elms[1]));
      nsAutoCString deviceId = NS_ConvertUTF16toUTF8(nsDependentString(elms[2]));

      CreateSessionOnPresenter(url, sessionId, deviceId);
    }
  }
  return NS_OK;
}

END_PRESENTATION_NAMESPACE
