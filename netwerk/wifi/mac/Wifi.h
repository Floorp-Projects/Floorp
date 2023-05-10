// Copyright 2008, Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. Neither the name of Google Inc. nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// The contents of this file are taken from Apple80211.h from the iStumbler
// project (http://www.istumbler.net). This project is released under the BSD
// license with the following restrictions.
//
// Copyright (c) 02006, Alf Watt (alf@istumbler.net). All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of iStumbler nor the names of its contributors may be
//   used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// This is the reverse engineered header for the Apple80211 private framework.
// The framework can be found at
// /System/Library/PrivateFrameworks/Apple80211.framework.

#ifndef GEARS_GEOLOCATION_OSX_WIFI_H__
#define GEARS_GEOLOCATION_OSX_WIFI_H__

#include <CoreFoundation/CoreFoundation.h>

extern "C" {

typedef SInt32 WIErr;

// A WirelessContext should be created using WirelessAttach
// before any other Wireless functions are called. WirelessDetach
// is used to dispose of a WirelessContext.
typedef struct __WirelessContext* WirelessContextPtr;

// WirelessAttach
//
// This should be called before all other wireless functions.
typedef WIErr (*WirelessAttachFunction)(WirelessContextPtr* outContext,
                                        const UInt32);

// WirelessDetach
//
// This should be called after all other wireless functions.
typedef WIErr (*WirelessDetachFunction)(WirelessContextPtr inContext);

typedef UInt16 WINetworkInfoFlags;

struct WirelessNetworkInfo {
  UInt16 channel;            // Channel for the network.
  SInt16 noise;              // Noise for the network. 0 for Adhoc.
  SInt16 signal;             // Signal strength of the network. 0 for Adhoc.
  UInt8 macAddress[6];       // MAC address of the wireless access point.
  UInt16 beaconInterval;     // Beacon interval in milliseconds
  WINetworkInfoFlags flags;  // Flags for the network
  UInt16 nameLen;
  SInt8 name[32];
};

// WirelessScanSplit
//
// WirelessScanSplit scans for available wireless networks. It will allocate 2
// CFArrays to store a list of managed and adhoc networks. The arrays hold
// CFData objects which contain WirelessNetworkInfo structures.
//
// Note: An adhoc network created on the computer the scan is running on will
// not be found. WirelessGetInfo can be used to find info about a local adhoc
// network.
//
// If stripDups != 0 only one bases tation for each SSID will be returned.
typedef WIErr (*WirelessScanSplitFunction)(WirelessContextPtr inContext,
                                           CFArrayRef* apList,
                                           CFArrayRef* adhocList,
                                           const UInt32 stripDups);

}  // extern "C"

#endif  // GEARS_GEOLOCATION_OSX_WIFI_H__
