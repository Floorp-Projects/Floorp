/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>
#import <CoreWLAN/CoreWLAN.h>

#include <dlfcn.h>
#include <unistd.h>

#include <objc/objc.h>
#include <objc/objc-runtime.h>

#include "nsObjCExceptions.h"
#include "nsCOMArray.h"
#include "nsWifiMonitor.h"
#include "nsWifiAccessPoint.h"
#include "MacWifiScanner.h"

namespace mozilla {

nsresult WifiScannerImpl::GetAccessPointsFromWLAN(
    nsTArray<RefPtr<nsIWifiAccessPoint>>& accessPoints) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  accessPoints.Clear();

  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

  @try {
    NSBundle* bundle = [[[NSBundle alloc]
        initWithPath:@"/System/Library/Frameworks/CoreWLAN.framework"]
        autorelease];
    if (!bundle) {
      [pool release];
      return NS_ERROR_NOT_AVAILABLE;
    }

    Class CWI_class = [bundle classNamed:@"CWInterface"];
    if (!CWI_class) {
      [pool release];
      return NS_ERROR_NOT_AVAILABLE;
    }

    id scanResult = [[CWI_class interface] scanForNetworksWithSSID:nil
                                                             error:nil];
    if (!scanResult) {
      [pool release];
      return NS_ERROR_NOT_AVAILABLE;
    }

    NSArray* scan = [NSMutableArray arrayWithArray:scanResult];
    NSEnumerator* enumerator = [scan objectEnumerator];

    while (id anObject = [enumerator nextObject]) {
      auto* ap = new nsWifiAccessPoint();
      if (!ap) {
        [pool release];
        return NS_ERROR_OUT_OF_MEMORY;
      }

      // [CWInterface bssidData] is deprecated on OS X 10.7 and up.  Which is
      // is a pain, so we'll use it for as long as it's available.
      unsigned char macData[6] = {0};
      if ([anObject respondsToSelector:@selector(bssidData)]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-method-access"
        NSData* data = [anObject bssidData];
#pragma clang diagnostic pop
        if (data) {
          memcpy(macData, [data bytes], 6);
        }
      } else {
        // [CWInterface bssid] returns a string formatted "00:00:00:00:00:00".
        NSString* macString = [anObject bssid];
        if (macString && ([macString length] == 17)) {
          for (NSUInteger i = 0; i < 6; ++i) {
            NSString* part =
                [macString substringWithRange:NSMakeRange(i * 3, 2)];
            NSScanner* scanner = [NSScanner scannerWithString:part];
            unsigned int data = 0;
            if (![scanner scanHexInt:&data]) {
              data = 0;
            }
            macData[i] = (unsigned char)data;
          }
        }
      }

      int signal = (int)((NSInteger)[anObject rssiValue]);
      ap->setMac(macData);
      ap->setSignal(signal);
      ap->setSSID([[anObject ssid] UTF8String], 32);

      accessPoints.AppendElement(ap);
    }
  } @catch (NSException*) {
    [pool release];
    return NS_ERROR_NOT_AVAILABLE;
  }

  [pool release];

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_NOT_AVAILABLE);
}

}  // namespace mozilla
