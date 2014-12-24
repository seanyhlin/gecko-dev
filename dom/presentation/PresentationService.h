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
#include "nsIPresentationServiceCallback.h"
#include "nsIPresentationDevice.h"
#include "nsIPresentationDevicePrompt.h" // nsIPresentationDeviceRequest
//#include "nsString.h"
#include "nsTObserverArray.h"

#include "SessionTransport.h"

// HACK
#include "nsITimer.h"

namespace mozilla {
namespace dom {
namespace presentation {

class PresentationService : public nsIObserver
{
  struct SessionInfo;
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  /**
   * Static function to return the PresentationService singleton instance. If
   * it's in the child process, an object of PresentationIPCService is returned.
   * Must called from main thread.
   */
  static PresentationService*
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
                       nsIPresentationServiceCallback* aCallback);

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

  void
  GenerateUniqueId(nsAString& aId);

  bool
  AddSessionInfo(const nsAString& aKey, SessionInfo* aInfo);

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

  /**
   * Transport channel has been established.
   */
  void
  OnSessionComplete(SessionTransport* aTransport);

  /**
   * Something is wrong.
   */
  void
  OnSessionFailure();

  /**
   * Sender should continue to establish transport channel.
   */
  void
  OnReceiverReady();

protected:
  PresentationService();
  virtual ~PresentationService();
  
  nsTObserverArray<nsCOMPtr<nsIPresentationListener> > mListeners;

private:
  /**
   * Create an instance of PresentationService if it's in the parent process.
   * Otherwise, create an instance of PresentationIPCService if it's in the
   * child process.
   */
  static PresentationService*
  Create();

  bool
  Init();
  
  nsresult
  HandleShutdown();
  
  bool
  FindAppOnDevice(const nsAString& aUrl);

  bool mAvailable;

  class PresentationDeviceRequest MOZ_FINAL : public nsIPresentationDeviceRequest
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPRESENTATIONDEVICEREQUEST

    PresentationDeviceRequest(const nsAString& aUrl,
                              const nsAString& aId,
                              const nsAString& aOrigin,
                              nsIPresentationServiceCallback* aCallback);

  private:
    virtual ~PresentationDeviceRequest();

    nsString mRequestUrl;
    nsString mId;
    nsString mOrigin;
    nsCOMPtr<nsIPresentationServiceCallback> mCallback;
  };

  struct SessionInfo
  {
    SessionInfo(nsIPresentationDevice* aDevice, SessionTransport* aTransport)
      : device(aDevice)
      , transport(aTransport)
    {
    }

    nsCOMPtr<nsIPresentationDevice> device;
    nsRefPtr<SessionTransport> transport;
  };
  
  nsClassHashtable<nsStringHashKey, SessionInfo> mSessionInfo;

  SessionInfo*
  GetSessionInfo(const nsAString& aUrl,
                 const nsAString& aId);

  uint32_t mLastUniqueId;

  class SessionRequester
  {
  public:
    SessionRequester(const nsAString& aId,
                     nsIPresentationDevice* aDevice,
                     nsIPresentationControlChannel* aCtrlChannel,
                     PresentationService* aService)
      : mId(aId)
      , mDevice(aDevice)
      , mChannel(aCtrlChannel)
      , mService(aService)
    { 
    }

    ~SessionRequester()
    {
      mService = nullptr;
      mDevice = nullptr;
      mChannel = nullptr;
    }

    void
    Fail(const nsresult& aRv)
    {
      // On Receiver side, PresentationService::OnSessionFailure() should be called.
    }

    void
    Connect()
    {
    }

  private:
    nsString mId;
    nsCOMPtr<nsIPresentationDevice> mDevice;
    nsCOMPtr<nsIPresentationControlChannel> mChannel;
    nsRefPtr<PresentationService> mService;
  };

  SessionRequester* mRequester;

  class SessionResponder
  {
  public:
    SessionResponder(const nsAString& aId,
                     nsIPresentationDevice* aDevice,
                     nsIPresentationControlChannel* aCtrlChannel,
                     PresentationService* aService)
      : mId(aId)
      , mDevice(aDevice)
      , mChannel(aCtrlChannel)
      , mService(aService)
    {
    }

    ~SessionResponder()
    {
      mDevice = nullptr;
      mChannel = nullptr;
      mService = nullptr;
    }

    void
    ReceiverReady()
    {
      // On Sender side, PresentationService::OnReceiverReady() should be called.
    }

    void
    Fail(const nsresult& aRv)
    {
      // On Sender side, PresentationService::OnSessionFailure() should be called.
    }

  private:
    nsString mId;
    nsCOMPtr<nsIPresentationDevice> mDevice;
    nsCOMPtr<nsIPresentationControlChannel> mChannel;
    nsRefPtr<PresentationService> mService;
  };
  
  SessionResponder* mResponder;

  // HACK
  nsCOMPtr<nsITimer> mTimer;
};

} // namespace presentation
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_presentation_PresentationService_h
