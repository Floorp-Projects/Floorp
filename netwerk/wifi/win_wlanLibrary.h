/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

// Moz headers (alphabetical)

// System headers (alphabetical)
#include <windows.h>   // HINSTANCE, HANDLE
#include <wlanapi.h>   // Wlan* functions


class WinWLANLibrary {
 public:
  static WinWLANLibrary* Load();
  ~WinWLANLibrary();

  HANDLE GetWLANHandle() const;
  decltype(::WlanEnumInterfaces)* GetWlanEnumInterfacesPtr() const;
  decltype(::WlanGetNetworkBssList)* GetWlanGetNetworkBssListPtr() const;
  decltype(::WlanFreeMemory)* GetWlanFreeMemoryPtr() const;
  decltype(::WlanCloseHandle)* GetWlanCloseHandlePtr() const;
  decltype(::WlanOpenHandle)* GetWlanOpenHandlePtr() const;
  decltype(::WlanRegisterNotification)* GetWlanRegisterNotificationPtr() const;
  decltype(::WlanScan)* GetWlanScanPtr() const;

 private:
  WinWLANLibrary();
  bool Initialize();

  HMODULE mWlanLibrary;
  HANDLE mWlanHandle;
  decltype(::WlanEnumInterfaces)* mWlanEnumInterfacesPtr;
  decltype(::WlanGetNetworkBssList)* mWlanGetNetworkBssListPtr;
  decltype(::WlanFreeMemory)* mWlanFreeMemoryPtr;
  decltype(::WlanCloseHandle)* mWlanCloseHandlePtr;
  decltype(::WlanOpenHandle)* mWlanOpenHandlePtr;
  decltype(::WlanRegisterNotification)* mWlanRegisterNotificationPtr;
  decltype(::WlanScan)* mWlanScanPtr;
};

class ScopedWLANObject {
public:
 ScopedWLANObject(WinWLANLibrary* library, void* object)
   : mLibrary(library)
   , mObject(object)
  {
  }

  ~ScopedWLANObject()
  {
    (*(mLibrary->GetWlanFreeMemoryPtr()))(mObject);
  }

 private:
  WinWLANLibrary *mLibrary;
  void *mObject;
};
