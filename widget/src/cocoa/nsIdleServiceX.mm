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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Shawn Wilsher <me@shawnwilsher.com>
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsIdleServiceX.h"
#include "nsObjCExceptions.h"
#include "nsIServiceManager.h"
#import <Foundation/Foundation.h>

NS_IMPL_ISUPPORTS1(nsIdleServiceX, nsIIdleService)

NS_IMETHODIMP
nsIdleServiceX::GetIdleTime(PRUint32 *aTimeDiff)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  kern_return_t rval;
  mach_port_t masterPort;

  rval = IOMasterPort(kIOMasterPortDefault, &masterPort);
  if (rval != KERN_SUCCESS)
    return NS_ERROR_FAILURE;

  io_iterator_t hidItr;
  rval = IOServiceGetMatchingServices(masterPort,
                                      IOServiceMatching("IOHIDSystem"),
                                      &hidItr);

  if (rval != KERN_SUCCESS)
    return NS_ERROR_FAILURE;
  NS_ASSERTION(hidItr, "Our iterator is null, but it ought not to be!");

  io_registry_entry_t entry = IOIteratorNext(hidItr);
  NS_ASSERTION(entry, "Our IO Registry Entry is null, but it shouldn't be!");

  IOObjectRelease(hidItr);

  NSMutableDictionary *hidProps;
  rval = IORegistryEntryCreateCFProperties(entry,
                                           (CFMutableDictionaryRef*)&hidProps,
                                           kCFAllocatorDefault, 0);
  if (rval != KERN_SUCCESS)
    return NS_ERROR_FAILURE;
  NS_ASSERTION(hidProps, "HIDProperties is null, but no error was returned.");
  [hidProps autorelease];

  id idleObj = [hidProps objectForKey:@"HIDIdleTime"];
  NS_ASSERTION([idleObj isKindOfClass: [NSData class]] ||
               [idleObj isKindOfClass: [NSNumber class]],
               "What we got for the idle object is not what we expect!");

  uint64_t time;
  if ([idleObj isKindOfClass: [NSData class]])
    [idleObj getBytes: &time];
  else
    time = [idleObj unsignedLongLongValue];

  IOObjectRelease(entry);

  // convert to ms from ns
  time /= 1000000;
  if (time > PR_UINT32_MAX) // Overflow will occur
    return NS_ERROR_CANNOT_CONVERT_DATA;

  *aTimeDiff = static_cast<PRUint32>(time);

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}
