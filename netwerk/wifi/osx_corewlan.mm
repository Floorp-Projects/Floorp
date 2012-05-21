/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
