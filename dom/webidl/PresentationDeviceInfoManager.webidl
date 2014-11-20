/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary PresentationDeviceInfo {
  DOMString id;
  DOMString name;
  DOMString type;
};

[NavigatorProperty="mozPresentationDeviceInfos",
 JSImplementation="@mozilla.org/presentation-device/deviceInfos;1",
 Pref="dom.presentation.enabled",
 CheckPermissions="settings-read settings-write"]
interface PresentationDeviceInfoManager : EventTarget {
  // notify if any device udpated.
  attribute EventHandler ondevicechange;

  // retrieve all available device infos
  Promise<sequence<PresentationDeviceInfo>> getAll();

  // Force all registered device provider to update device information.
  void forceDiscovery();
};
