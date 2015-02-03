/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_presentation_PresentationService_h
#define mozilla_dom_presentation_PresentationService_h

#include "nsCOMPtr.h"
#include "nsClassHashtable.h"
#include "nsIObserver.h"
#include "nsIPresentationRequestCallback.h"
#include "nsIPresentationDevice.h"
#include "nsIPresentationDevicePrompt.h" // nsIPresentationDeviceRequest
#include "nsTObserverArray.h"
#include "mozilla/dom/presentation/Session.h"
#include "PresentationSessionTransport.h"

class nsIInputStream;

namespace mozilla {
namespace dom {
namespace presentation {

class Session;

class PresentationService : public nsIObserver
{
  struct SessionInfo;
  friend class PresentationSessionTransport;
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  /**
   * Static function to return the PresentationService singleton instance. If
   * it's in the child process, an object of PresentationIPCService is returned.
   * Must called from main thread.
   */
  static already_AddRefed<PresentationService>
  Get();

  /**
   * Start a new presentation session and display a prompt box which asks users
   * to select a device.
   *
   * @param aUrl: The content to be presented.
   * @param aId: An ID to identify the presentation instance.
   * @param aOrigin: The origin which initiate the request.
   * @param aCallback: Invoke the callback when the operation is completed.
   *                   NotifySuccess() is called with a device id if a session is
   *                   established successfully with the selected device.
   *                   Otherwise, NotifyError() is called with a error message.
   */
  virtual nsresult
  StartSessionInternal(const nsAString& aUrl,
                       const nsAString& aId,
                       const nsAString& aOrigin,
                       nsIPresentationRequestCallback* aCallback);

  /**
   * Join a existing presentation session.
   *
   * @param aUrl: The content to be presented.
   * @param aId: An ID to identify the presentation instance.
   * @param aOrigin: The origin which initiate the request.
   * @param aCallback: Invoke the callback when the operation is completed.
   *                   NotifySuccess() is called with a device id if one
   *                   presentation session exists that is presenting the same
   *                   url with the same presentation ID. Otherwise,
   *                   NotifyError() is called with a error message.
   */
  virtual nsresult
  JoinSessionInternal(const nsAString& aUrl,
                      const nsAString& aId,
                      const nsAString& aOrigin);

  virtual nsresult
  SendMessageInternal(const nsAString& aSessionId,
                      nsIInputStream* aStream);

  virtual nsresult
  CloseSessionInternal(const nsAString& aSessionId);

  /**
   * Register a message handler. Must be called from the main thread.
   */
  virtual void
  RegisterListener(nsIPresentationListener* aListener);

  /**
   * Unregister a message handler. Must be called from the main thread.
   */
  virtual void
  UnregisterListener(nsIPresentationListener* aListener);

  /**
   * Register a session listener. Must be called from the main thread.
   */
  virtual void
  RegisterSessionListener(const nsAString& aSessionId,
                          nsIPresentationSessionListener* aListener);

  /**
   * Unregister a session listener. Must be called from the main thread.
   */
  virtual void
  UnregisterSessionListener(const nsAString& aSessionId,
                            nsIPresentationSessionListener* aListener);

  /**
   * Notify registered listeners of the change of device availablility.
   */
  void
  NotifyAvailableListeners(bool aAvailable);

  /**
   * Notify registered listeners of the change of session object.
   */
  void
  NotifySessionReady(const nsAString& aId);

  nsresult
  OnSessionComplete(Session* aSession);

  nsresult
  OnSessionClose(Session* aSession, nsresult aReason);

  nsresult
  OnSessionMessage(Session* aSession, const nsACString& aMessage);

protected:
  PresentationService();

  virtual
  ~PresentationService();

  nsTObserverArray<nsCOMPtr<nsIPresentationListener> > mListeners;

private:
  /**
   * Create an instance of PresentationService if it's in the parent process.
   * Otherwise, create an instance of PresentationIPCService if it's in the
   * child process.
   */
  static already_AddRefed<PresentationService>
  Create();

  bool
  Init();

  nsresult
  HandleShutdown();

  nsresult
  HandleDeviceChange();

  nsresult
  HandleSessionRequest(nsISupports* aSubject);

  bool
  FindAppOnDevice(const nsAString& aUrl);

  nsresult
  ReplyRequestWithError(const nsAString& aId, const nsAString& aError);

  bool mAvailable;
  bool mPendingSessionReady;
  nsString mPendingSessionId;

  class PresentationDeviceRequest MOZ_FINAL : public nsIPresentationDeviceRequest
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPRESENTATIONDEVICEREQUEST

    PresentationDeviceRequest(const nsAString& aUrl,
                              const nsAString& aId,
                              const nsAString& aOrigin);

  private:
    virtual ~PresentationDeviceRequest();

    nsString mRequestUrl;
    nsString mId;
    nsString mOrigin;
  };

  struct SessionInfo
  {
    SessionInfo(nsIPresentationRequestCallback* aCallback)
      : isRequester(true)
      , isSessionComplete(false)
      , callback(aCallback)
    { }

    SessionInfo(Session* aSession,
                nsIPresentationDevice* aDevice)
      : isRequester(false)
      , isSessionComplete(false)
      , session(aSession)
      , device(aDevice)
    { }

    bool isRequester;
    bool isSessionComplete;
    nsRefPtr<Session> session;
    nsCOMPtr<nsIPresentationDevice> device;
    nsCOMPtr<nsIPresentationSessionListener> listener;
    nsCOMPtr<nsIPresentationRequestCallback> callback;
  };

  nsClassHashtable<nsStringHashKey, SessionInfo> mSessionInfo;
};

} // namespace presentation
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_presentation_PresentationService_h
