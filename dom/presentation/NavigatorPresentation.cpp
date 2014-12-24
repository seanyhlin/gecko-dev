/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NavigatorPresentation.h"
#include "nsIPresentationDeviceManager.h"
#include "mozilla/dom/NavigatorPresentationBinding.h"
#include "mozilla/dom/Promise.h"
#include "PresentationService.h"

#define AVAILABLECHANGE_EVENT_NAME NS_LITERAL_STRING("availablechange")
#define SESSIONREADY_EVENT_NAME NS_LITERAL_STRING("sessionready")

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "GonkDBus", args);
#else
#define LOG(args...) printf(args);
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::presentation;

// StartSessionCallback

NS_IMPL_ISUPPORTS(StartSessionCallback, nsIPresentationServiceCallback)

StartSessionCallback::StartSessionCallback(NavigatorPresentation* aNavigatorPresentation,
                                           const nsAString& aUrl,
                                           const nsAString& aId,
                                           const nsAString& aOrigin,
                                           Promise* aPromise,
                                           nsPIDOMWindow* aWindow)
  : mNavigatorPresentation(aNavigatorPresentation)
  , mId(aId)
  , mPromise(aPromise)
  , mWindow(aWindow)
{
  LOG("StartSessionCallback, aId: %s\n", NS_ConvertUTF16toUTF8(mId).get());
}

StartSessionCallback::~StartSessionCallback()
{
  mNavigatorPresentation = nullptr;
  mPromise = nullptr;
  mWindow = nullptr;
}

NS_IMETHODIMP
StartSessionCallback::NotifySuccess()
{
  LOG("StartSessionCallback::NotifySuccess");
  MOZ_ASSERT(NS_IsMainThread());
  nsRefPtr<PresentationSession> session = new PresentationSession(mWindow,
                                                                  mId);
  nsAutoString id;
  session->GetId(id);
  LOG("id: %s\n", NS_ConvertUTF16toUTF8(id).get());
  mPromise->MaybeResolve(session);
  mPromise = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
StartSessionCallback::NotifyError(const nsAString& aError)
{
  MOZ_ASSERT(NS_IsMainThread());
  mPromise->MaybeRejectBrokenly(aError);
  return NS_OK;
}

// NavigatorPresentation

NS_IMPL_CYCLE_COLLECTION_INHERITED(NavigatorPresentation, DOMEventTargetHelper, mSession)

NS_IMPL_ADDREF_INHERITED(NavigatorPresentation, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(NavigatorPresentation, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(NavigatorPresentation)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

/* static */ already_AddRefed<NavigatorPresentation>
NavigatorPresentation::Create(nsPIDOMWindow* aWindow)
{
  nsRefPtr<NavigatorPresentation> presentation =
    new NavigatorPresentation(aWindow);

  return presentation.forget();
}

NavigatorPresentation::NavigatorPresentation(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
  , mAvailable(false)
  , mSession(nullptr)
{
  NS_WARN_IF(!Init());
}

/* virtual */ NavigatorPresentation::~NavigatorPresentation()
{
  PresentationService* service = PresentationService::Get();
  if (!NS_WARN_IF(service)) {
    return;
  }
  service->UnregisterListener(this);
}

bool
NavigatorPresentation::Init()
{
  PresentationService* service = PresentationService::Get();
  if (!service) {
    return false;
  }
  service->RegisterListener(this);

  nsCOMPtr<nsIPresentationDeviceManager> deviceManager =
    do_GetService(PRESENTATION_DEVICE_MANAGER_CONTRACTID);
  deviceManager->GetDeviceAvailable(&mAvailable);

  return true;
}

/* virtual */ JSObject*
NavigatorPresentation::WrapObject(JSContext* aCx)
{
  return NavigatorPresentationBinding::Wrap(aCx, this);
}

already_AddRefed<Promise>
NavigatorPresentation::StartSession(const nsAString& aUrl,
                                    const Optional<nsAString>& aId,
                                    ErrorResult& aRv)
{
  LOG("StartSession\n");
  // Get origin.
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsIPrincipal* principal = global->PrincipalOrNull();
  if (NS_WARN_IF(!principal)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsAutoCString cstrOrigin;
  principal->GetOrigin(getter_Copies(cstrOrigin));
  NS_ConvertUTF8toUTF16 origin(cstrOrigin);

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  PresentationService* service = PresentationService::Get();
  if(NS_WARN_IF(!service)) {
    promise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
    return promise.forget();
  }

  // Assign id if it's not specified.
  nsString id;
  if (aId.WasPassed()) {
    id = aId.Value();
  } else {
    service->GenerateUniqueId(id);
  }

  nsCOMPtr<nsIPresentationServiceCallback> callback =
    new StartSessionCallback(this, aUrl, id, origin, promise, GetOwner());
  if (NS_FAILED(service->StartSessionInternal(aUrl, id, origin, callback))) {
    promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
  }

  return promise.forget();
}

already_AddRefed<Promise>
NavigatorPresentation::JoinSession(const nsAString& aUrl,
                                   const nsAString& aId,
                                   ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsIPrincipal* principal = global->PrincipalOrNull();
  if (NS_WARN_IF(!principal)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsAutoCString origin;
  principal->GetOrigin(getter_Copies(origin));

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  PresentationService* service = PresentationService::Get();
  if(NS_WARN_IF(!service)) {
    promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return promise.forget();
  }
  service->JoinSessionInternal(aUrl, aId, NS_ConvertUTF8toUTF16(origin));

  return promise.forget();
}

already_AddRefed<PresentationSession>
NavigatorPresentation::GetSession() const
{
  nsRefPtr<PresentationSession> session = mSession;
  return session.forget();
}

bool
NavigatorPresentation::Available() const
{
  LOG("NavigatorPresentation::Available %d\n", mAvailable);
  return mAvailable;
}

NS_IMETHODIMP
NavigatorPresentation::NotifyAvailableChange(bool aAvailable)
{
  LOG("NavigatorPresentation::NotifyAvailableChange, %d\n", aAvailable);
  mAvailable = aAvailable;
  DispatchTrustedEvent(AVAILABLECHANGE_EVENT_NAME);
  return NS_OK;
}

NS_IMETHODIMP
NavigatorPresentation::NotifySessionReady(const nsAString& aId)
{
  LOG("NavigatorPresentation::NotifySessionReady\n");
  mSession = new PresentationSession(GetOwner(), aId);
  DispatchTrustedEvent(SESSIONREADY_EVENT_NAME);
  return NS_OK;
}
