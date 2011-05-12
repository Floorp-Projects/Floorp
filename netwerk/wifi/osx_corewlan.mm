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
 * The Initial Developer of the Original Code is Mozilla Foundation
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

#include "nsObjCExceptions.h"
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (!UsingSnowLeopard())
    return NS_ERROR_NOT_AVAILABLE;

  accessPoints.Clear();

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  @try {
    NSBundle * bundle = [[[NSBundle alloc] initWithPath:@"/System/Library/Frameworks/CoreWLAN.framework"] autorelease];
    if (!bundle) {
      [pool release];
      return NS_ERROR_NOT_AVAILABLE;
    }

    Class CWI_class = [bundle classNamed:@"CWInterface"];
    if (!CWI_class) {
      [pool release];
      return NS_ERROR_NOT_AVAILABLE;
    }

    id scanResult = [[CWI_class interface] scanForNetworksWithParameters:nil error:nil];
    if (!scanResult) {
      [pool release];
      return NS_ERROR_NOT_AVAILABLE;
    }

    NSArray* scan = [NSMutableArray arrayWithArray:scanResult];
    NSEnumerator *enumerator = [scan objectEnumerator];

    while (id anObject = [enumerator nextObject]) {
      nsWifiAccessPoint* ap = new nsWifiAccessPoint();
      if (!ap) {
        [pool release];
        return NS_ERROR_OUT_OF_MEMORY;
      }

      // [CWInterface bssidData] is deprecated on OS X 10.7 and up.  Which is
      // is a pain, so we'll use it for as long as it's available.
      unsigned char macData[6] = {0};
      if ([anObject respondsToSelector:@selector(bssidData)]) {
        NSData* data = [anObject bssidData];
        if (data) {
          memcpy(macData, [data bytes], 6);
        }
      } else {
        // [CWInterface bssid] returns a string formatted "00:00:00:00:00:00".
        NSString* macString = [anObject bssid];
        if (macString && ([macString length] == 17)) {
          for (NSUInteger i = 0; i < 6; ++i) {
            NSString* part = [macString substringWithRange:NSMakeRange(i * 3, 2)];
            NSScanner* scanner = [NSScanner scannerWithString:part];
            unsigned int data = 0;
            if (![scanner scanHexInt:&data]) {
              data = 0;
            }
            macData[i] = (unsigned char) data;
          }
        }
      }

      // [CWInterface rssiValue] is available on OS X 10.7 and up (and
      // [CWInterface rssi] is deprecated).
      int signal = 0;
      if ([anObject respondsToSelector:@selector(rssiValue)]) {
        signal = (int) ((NSInteger) [anObject rssiValue]);
      } else {
        signal = [[anObject rssi] intValue];
      }

      ap->setMac(macData);
      ap->setSignal(signal);
      ap->setSSID([[anObject ssid] UTF8String], 32);

      accessPoints.AppendObject(ap);
    }
  }
  @catch(NSException *_exn) {
    [pool release];
    return NS_ERROR_NOT_AVAILABLE;
  }

  [pool release];

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NS_ERROR_NOT_AVAILABLE);
}
