/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NavigatorPresentation.h"
#include "mozilla/dom/NavigatorPresentationBinding.h"
#include "mozilla/dom/Promise.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(NavigatorPresentation)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(NavigatorPresentation, DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(NavigatorPresentation, DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(NavigatorPresentation, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(NavigatorPresentation, DOMEventTargetHelper)

// QueryInterface implementation for NavigatorPresentation
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
{
}

/* virtual */ NavigatorPresentation::~NavigatorPresentation()
{
}

/* virtual */ JSObject*
NavigatorPresentation::WrapObject(JSContext* aCx)
{
  return NavigatorPresentationBinding::Wrap(aCx, this);
}

already_AddRefed<Promise>
NavigatorPresentation::StartSession(const nsAString& aUrl,
                                    const nsAString& aSessionId,
                                    ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // TODO Resolve/reject the promise.

  return promise.forget();
}

already_AddRefed<Promise>
NavigatorPresentation::JoinSession(const nsAString& aUrl,
                                   const nsAString& aSessionId,
                                   ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // TODO Resolve/reject the promise.

  return promise.forget();
}

already_AddRefed<PresentationSession>
NavigatorPresentation::GetSession() const
{
  // TODO Return an object of PresentationSession when the session is connected.
  // Otherwise, return nullptr.
  return nullptr;
}

bool
NavigatorPresentation::Available() const
{
  return mAvailable;
}

} // namespace dom
} // namespace mozilla
