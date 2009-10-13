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
 * Doug Turner <dougt@dougt.org>
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#include "nsAccelerometerX.h"
#include "nsIServiceManager.h"
#include "stdlib.h"

#include <sys/sysctl.h>
#include <sys/resource.h>
#include <sys/vm.h>

#define MODEL_NAME_LENGTH 64
static char gModelName[MODEL_NAME_LENGTH];

nsAccelerometerX::nsAccelerometerX()
{
}

nsAccelerometerX::~nsAccelerometerX()
{
}

// Data format returned from IOConnectMethodStructureIStructureO.
// I am not sure what the other bits in this structure are,
// or if there are any, but this has to be 40 bytes long or
// the call to read fails.
//
// Since we make the SmsData struct larger than any members we plan to access we
// keep track of the the size of the part of the struct we plan to access for
// use in bounds checking.
#define SMSDATA_PADDING_SIZE 34
typedef struct
{
  PRInt16 x;
  PRInt16 y;
  PRInt16 z;
  PRInt8  unknown[SMSDATA_PADDING_SIZE];
} SmsData;
#define SMSDATA_USED_SIZE (sizeof(SmsData) - SMSDATA_PADDING_SIZE)

void
nsAccelerometerX::UpdateHandler(nsITimer *aTimer, void *aClosure)
{
  nsAccelerometerX *self = reinterpret_cast<nsAccelerometerX *>(aClosure);
  if (!self) {
    NS_ERROR("no self");
    return;
  }

  size_t bufferLen = sizeof(SmsData);

  void * input = malloc(bufferLen);
  void * output = malloc(bufferLen);

  if (!input || !output)
    return;
    
  memset(input, 0, bufferLen);
  memset(output, 0, bufferLen);

  size_t structureOutputSize = bufferLen;
#if (MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4)
  kern_return_t result = ::IOConnectMethodStructureIStructureO(self->mSmsConnection,
                                                               5, /* Magic number for SMCMotionSensor */
                                                               bufferLen,
                                                               (IOByteCount*)&structureOutputSize,
                                                               input,
                                                               output);
#else
  kern_return_t result = ::IOConnectCallStructMethod((mach_port_t)self->mSmsConnection,
                                                     5, /* Magic number for SMCMotionSensor */
                                                     input,
                                                     bufferLen,
                                                     output,
                                                     &structureOutputSize);
#endif

  if ((result != kIOReturnSuccess) || (structureOutputSize < SMSDATA_USED_SIZE)) {
    free(input);
    free(output);
    return;
  }

  SmsData *data = (SmsData*) output;
  
  float xf, yf, zf;

  // we want to normalize the return result from the chip to
  // something between -1 and 1 where 0 is the balance point.

  const int normalizeFactor = 250.5;
  
  if (!strcmp(gModelName, "MacBookPro5,1")) {
    xf = ((float)data->x) / normalizeFactor;
    yf = (((float)data->y) / normalizeFactor) * -1;
    zf = ((float)data->z) / normalizeFactor;
  }
  else if (!strcmp(gModelName, "MacBookPro5,3")) {
    xf = ((float)data->y) / normalizeFactor;
    yf = (((float)data->x) / normalizeFactor) * -1;
    zf = (((float)data->z) / normalizeFactor) * -1;
  }
  else
  {
    xf = (((float)data->x) / normalizeFactor) * -1;
    yf = ((float)data->y) / normalizeFactor;
    zf = ((float)data->z) / normalizeFactor;
  }

  free(input);
  free(output);

  self->AccelerationChanged( xf, yf, zf );
}

void nsAccelerometerX::Startup()
{
  // we can fail, and that just means the caller will not see any changes.

  mach_port_t port;
  kern_return_t result = ::IOMasterPort(MACH_PORT_NULL, &port);
  if (result != kIOReturnSuccess)
    return;
  
  CFMutableDictionaryRef  dict = ::IOServiceMatching("SMCMotionSensor");
  if (!dict)
    return;
  
  io_iterator_t iter;
  result = ::IOServiceGetMatchingServices(port, dict, &iter);
  if (result != kIOReturnSuccess)
    return;
  
  io_object_t device = ::IOIteratorNext(iter);

  ::IOObjectRelease(iter);
  
  if (!device)
    return;
  
  result = ::IOServiceOpen(device, mach_task_self(), 0, &mSmsConnection);
  ::IOObjectRelease(device);
  
  if (result != kIOReturnSuccess)
    return;
  
  mach_port_deallocate(mach_task_self(), port);
  
  /* get the version of the hardware we are running on. */
  int mib[2];
  size_t len = MODEL_NAME_LENGTH;
  mib[0] = CTL_HW;
  mib[1] = HW_MODEL;
  sysctl(mib, 2, gModelName, &len, NULL, 0);

  mUpdateTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (mUpdateTimer)
    mUpdateTimer->InitWithFuncCallback(UpdateHandler,
                                       this,
                                       mUpdateInterval,
                                       nsITimer::TYPE_REPEATING_SLACK);
}

void nsAccelerometerX::Shutdown()
{
  if (mSmsConnection)
    ::IOServiceClose(mSmsConnection);
  
  if (mUpdateTimer) {
    mUpdateTimer->Cancel();
    mUpdateTimer = nsnull;
  }
}

