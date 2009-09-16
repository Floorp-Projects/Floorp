/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Geolocation.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Doug Turner <dougt@meer.net>  (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#import <Cocoa/Cocoa.h>

#include <mach-o/dyld.h>
#include <dlfcn.h>
#include <unistd.h>

#include <objc/objc.h>
#include <objc/objc-runtime.h>

#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsWifiMonitor.h"
#include "nsWifiAccessPoint.h"

BOOL UsingSnowLeopard() {
  static PRInt32 gOSXVersion = 0x0;
  if (gOSXVersion == 0x0) {
    Gestalt(gestaltSystemVersion, (SInt32*)&gOSXVersion);
  }
  return (gOSXVersion >= 0x00001060);
}

nsresult
GetAccessPointsFromWLAN(nsCOMArray<nsWifiAccessPoint> &accessPoints)
{
  if (!UsingSnowLeopard())
    return NS_ERROR_NOT_AVAILABLE;

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  void *corewlan_library = dlopen("/System/Library/Frameworks/CoreWLAN.framework/CoreWLAN",
                                  RTLD_LAZY);
  if (!corewlan_library)
    return NS_ERROR_NOT_AVAILABLE;
  
  accessPoints.Clear();
  
  id anObject;
  NSError *err = nil;
  NSDictionary *params = nil;

  // We do this the hard way because we want to be able to run on pre-10.6.  When we drop
  // this requirement, this objective-c magic goes away.

  // dynamically call: [CWInterface interface];
  Class CWI_class = objc_getClass("CWInterface");
  if (!CWI_class) {
    dlclose(corewlan_library);
    return NS_ERROR_NOT_AVAILABLE;
  }

  SEL interfaceSel = sel_registerName("interface");
  id interface = objc_msgSend(CWI_class, interfaceSel);

  // call [interface scanForNetworksWithParameters:params err:&err]
  SEL scanSel = sel_registerName("scanForNetworksWithParameters:error:");
  id scanResult = objc_msgSend(interface, scanSel, params, err);

  NSArray* scan = [NSMutableArray arrayWithArray:scanResult];
  NSEnumerator *enumerator = [scan objectEnumerator];
  
  while (anObject = [enumerator nextObject]) {
 
    nsWifiAccessPoint* ap = new nsWifiAccessPoint();
    if (!ap)
      continue;
 

    NSData* data = [anObject bssidData];
    ap->setMac((unsigned char*)[data bytes]);
    ap->setSignal([[anObject rssi] intValue]);
    ap->setSSID([[anObject ssid] UTF8String], 32);
    
    accessPoints.AppendObject(ap);
  }

  dlclose(corewlan_library);
  return NS_OK;
}
