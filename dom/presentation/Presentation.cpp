/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Presentation.h"
#include "nsIPresentationDeviceManager.h"
#include "nsIUUIDGenerator.h"
#include "mozilla/dom/PresentationBinding.h"
#include "mozilla/dom/Promise.h"
#include "PresentationService.h"

#define AVAILABLECHANGE_EVENT_NAME NS_LITERAL_STRING("availablechange")
#define SESSIONREADY_EVENT_NAME NS_LITERAL_STRING("sessionready")

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::presentation;

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Presentation", args);
#else
#define LOG(args...)  printf(args);
#endif

// SessionCallback

class SessionCallback : public nsIPresentationRequestCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONREQUESTCALLBACK

  SessionCallback(Presentation* aPresentation,
                  const nsAString& aUrl,
                  const nsAString& aId,
                  const nsAString& aOrigin,
                  Promise* aPromise,
                  nsPIDOMWindow* aWindow);

private:
  virtual ~SessionCallback();

  nsRefPtr<Presentation> mPresentation;
  nsString mId;
  nsRefPtr<Promise> mPromise;
  nsCOMPtr<nsPIDOMWindow> mWindow;
};

NS_IMPL_ISUPPORTS(SessionCallback, nsIPresentationRequestCallback)

SessionCallback::SessionCallback(Presentation* aPresentation,
                                 const nsAString& aUrl,
                                 const nsAString& aId,
                                 const nsAString& aOrigin,
                                 Promise* aPromise,
                                 nsPIDOMWindow* aWindow)
  : mPresentation(aPresentation)
  , mId(aId)
  , mPromise(aPromise)
  , mWindow(aWindow)
{
  LOG("SessionCallback, aId: %s\n", NS_ConvertUTF16toUTF8(mId).get());
}

SessionCallback::~SessionCallback()
{
  mPresentation = nullptr;
  mPromise = nullptr;
  mWindow = nullptr;
}

NS_IMETHODIMP
SessionCallback::NotifySuccess()
{
  LOG("SessionCallback::NotifySuccess\n");
  MOZ_ASSERT(NS_IsMainThread());
  nsRefPtr<PresentationSession> session = PresentationSession::Create(mWindow,
                                                                      mId);
  if (!session) {
    mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return NS_ERROR_DOM_ABORT_ERR;
  }

  // At the sender side, this function must get called after the transport
  // channel is ready. So we simply set the session state as connected.
  session->SetState(true);

  nsAutoString id;
  session->GetId(id);
  LOG("id: %s\n", NS_ConvertUTF16toUTF8(id).get());
  mPromise->MaybeResolve(session);
  mPromise = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
SessionCallback::NotifyError(const nsAString& aError)
{
  LOG("SessionCallback::NotifyError, %s\n", NS_ConvertUTF16toUTF8(aError).get());
  MOZ_ASSERT(NS_IsMainThread());
  mPromise->MaybeRejectBrokenly(aError);
  return NS_OK;
}

// Presentation

NS_IMPL_CYCLE_COLLECTION_CLASS(Presentation)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(Presentation, DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSession)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(Presentation, DOMEventTargetHelper)
  tmp->Shutdown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSession)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(Presentation, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Presentation, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(Presentation)
  NS_INTERFACE_MAP_ENTRY(nsIPresentationListener)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

/* static */ already_AddRefed<Presentation>
Presentation::Create(nsPIDOMWindow* aWindow)
{
  nsRefPtr<Presentation> presentation =
    new Presentation(aWindow);

  return presentation.forget();
}

Presentation::Presentation(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
  , mAvailable(false)
  , mSession(nullptr)
{
  NS_WARN_IF(!Init());
}

Presentation::~Presentation()
{
  Shutdown();
}

bool
Presentation::Init()
{
  nsRefPtr<PresentationService> service = PresentationService::Get();
  if (NS_WARN_IF(!service)) {
    return false;
  }
  service->RegisterListener(this);

  nsCOMPtr<nsIPresentationDeviceManager> deviceManager =
    do_GetService(PRESENTATION_DEVICE_MANAGER_CONTRACTID);
  deviceManager->GetDeviceAvailable(&mAvailable);

  return true;
}

void Presentation::Shutdown()
{
  nsRefPtr<PresentationService> service = PresentationService::Get();
  if (NS_WARN_IF(!service)) {
    return;
  }
  service->UnregisterListener(this);
}

/* virtual */ JSObject*
Presentation::WrapObject(JSContext* aCx)
{
  return PresentationBinding::Wrap(aCx, this);
}

already_AddRefed<Promise>
Presentation::StartSession(const nsAString& aUrl,
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

  nsRefPtr<PresentationService> service = PresentationService::Get();
  if(NS_WARN_IF(!service)) {
    promise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
    return promise.forget();
  }

  // Assign id if it's not specified.
  nsString id;
  if (aId.WasPassed()) {
    id = aId.Value();
  } else {
    nsCOMPtr<nsIUUIDGenerator> uuidgen =
      do_GetService("@mozilla.org/uuid-generator;1");
    if(NS_WARN_IF(!uuidgen)) {
      promise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
      return promise.forget();
    }

    nsID uuid;
    uuidgen->GenerateUUIDInPlace(&uuid);
    char buffer[NSID_LENGTH];
    uuid.ToProvidedString(buffer);
    CopyASCIItoUTF16(buffer, id);
  }

  nsCOMPtr<nsIPresentationRequestCallback> callback =
    new SessionCallback(this, aUrl, id, origin, promise, GetOwner());
  if (NS_FAILED(service->StartSessionInternal(aUrl, id, origin, callback))) {
    promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
  }

  return promise.forget();
}

already_AddRefed<Promise>
Presentation::JoinSession(const nsAString& aUrl,
                                   const nsAString& aId,
                                   ErrorResult& aRv)
{
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

  nsRefPtr<PresentationService> service = PresentationService::Get();
  if(NS_WARN_IF(!service)) {
    promise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
    return promise.forget();
  }

  nsCOMPtr<nsIPresentationRequestCallback> callback =
    new SessionCallback(this, aUrl, aId, origin, promise, GetOwner());
/*  if (NS_FAILED(service->JoinSessionInternal(aUrl, aId, origin, callback))) {
    promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
  }*/

  return promise.forget();
}

already_AddRefed<PresentationSession>
Presentation::GetSession() const
{
  nsRefPtr<PresentationSession> session = mSession;
  return session.forget();
}

bool
Presentation::Available() const
{
  LOG("Presentation::Available %d", mAvailable);
  return mAvailable;
}

NS_IMETHODIMP
Presentation::NotifyAvailableChange(bool aAvailable)
{
  LOG("Presentation::NotifyAvailableChange, %d", aAvailable);
  mAvailable = aAvailable;
  DispatchTrustedEvent(AVAILABLECHANGE_EVENT_NAME);
  return NS_OK;
}

NS_IMETHODIMP
Presentation::NotifySessionReady(const nsAString& aId)
{
  LOG("Presentation::NotifySessionReady");
  mSession = PresentationSession::Create(GetOwner(), aId);
  NS_ENSURE_TRUE(mSession, NS_ERROR_DOM_ABORT_ERR);
  DispatchTrustedEvent(SESSIONREADY_EVENT_NAME);
  return NS_OK;
}
