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
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Ventnor <m.ventnor@gmail.com>
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

#include <unistd.h>
#include "nsAccelerometerUnix.h"
#include "nsIServiceManager.h"

typedef struct {
  const char* mPosition;
  const char* mCalibrate;
  nsAccelerometerUnixDriver mToken;
} Accelerometer;

static const Accelerometer gAccelerometers[] = {
  // MacBook
  {"/sys/devices/platform/applesmc.768/position",
   "/sys/devices/platform/applesmc.768/calibrate",
   eAppleSensor},
  // Thinkpad
  {"/sys/devices/platform/hdaps/position",
   "/sys/devices/platform/hdaps/calibrate",
   eIBMSensor},
  // Maemo Fremantle
  {"/sys/class/i2c-adapter/i2c-3/3-001d/coord",
   NULL,
   eMaemoSensor},
};

nsAccelerometerUnix::nsAccelerometerUnix() :
  mPositionFile(NULL),
  mCalibrateFile(NULL),
  mType(eNoSensor)
{
}

nsAccelerometerUnix::~nsAccelerometerUnix()
{
}

void
nsAccelerometerUnix::UpdateHandler(nsITimer *aTimer, void *aClosure)
{
  nsAccelerometerUnix *self = reinterpret_cast<nsAccelerometerUnix *>(aClosure);
  if (!self) {
    NS_ERROR("no self");
    return;
  }

  float xf, yf, zf;

  switch (self->mType) {
    case eAppleSensor:
    {
      int x, y, z, calibrate_x, calibrate_y;
      fflush(self->mCalibrateFile);
      rewind(self->mCalibrateFile);

      fflush(self->mPositionFile);
      rewind(self->mPositionFile);

      if (fscanf(self->mCalibrateFile, "(%d, %d)", &calibrate_x, &calibrate_y) <= 0)
        return;

      if (fscanf(self->mPositionFile, "(%d, %d, %d)", &x, &y, &z) <= 0)
        return;

      // In applesmc:
      // - the x calibration value is negated
      // - a negative z actually means a correct-way-up computer
      // - dividing by 255 gives us the intended value of -1 to 1
      xf = ((float)(x + calibrate_x)) / 255.0;
      yf = ((float)(y - calibrate_y)) / 255.0;
      zf = ((float)z) / -255.0;
      break;
    }
    case eIBMSensor:
    {
      int x, y, calibrate_x, calibrate_y;
      fflush(self->mCalibrateFile);
      rewind(self->mCalibrateFile);

      fflush(self->mPositionFile);
      rewind(self->mPositionFile);

      if (fscanf(self->mCalibrateFile, "(%d, %d)", &calibrate_x, &calibrate_y) <= 0)
        return;

      if (fscanf(self->mPositionFile, "(%d, %d)", &x, &y) <= 0)
        return;

      xf = ((float)(x - calibrate_x)) / 180.0;
      yf = ((float)(y - calibrate_y)) / 180.0;
      zf = 1.0f;
      break;
    }
    case eMaemoSensor:
    {
      int x, y, z;
      fflush(self->mPositionFile);
      rewind(self->mPositionFile);

      if (fscanf(self->mPositionFile, "%d %d %d", &x, &y, &z) <= 0)
        return;

      xf = ((float)x) / -1000.0;
      yf = ((float)y) / -1000.0;
      zf = ((float)z) / -1000.0;
      break;
    }

    case eNoSensor:
    default:
      return;
  }

  self->AccelerationChanged( xf, yf, zf );
}

void nsAccelerometerUnix::Startup()
{
  // Accelerometers in Linux are used by reading a file (yay UNIX!), which is
  // in a slightly different location depending on the driver.
  for (unsigned int i = 0; i < NS_ARRAY_LENGTH(gAccelerometers); i++) {
    if (!(mPositionFile = fopen(gAccelerometers[i].mPosition, "r")))
      continue;

    mType = gAccelerometers[i].mToken;
    if (gAccelerometers[i].mCalibrate) {
      mCalibrateFile = fopen(gAccelerometers[i].mCalibrate, "r");
      if (!mCalibrateFile) {
        fclose(mPositionFile);
        mPositionFile = nsnull;
        return;
      }
    }

    break;
  }

  if (mType == eNoSensor)
    return;
  
  mUpdateTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (mUpdateTimer)
    mUpdateTimer->InitWithFuncCallback(UpdateHandler,
                                       this,
                                       mUpdateInterval,
                                       nsITimer::TYPE_REPEATING_SLACK);
}

void nsAccelerometerUnix::Shutdown()
{
  if (mPositionFile) {
    fclose(mPositionFile);
    mPositionFile = nsnull;
  }

  // Fun fact: writing to the calibrate file causes the
  // driver to re-calibrate the accelerometer
  if (mCalibrateFile) {
    fclose(mCalibrateFile);
    mCalibrateFile = nsnull;
  }

  mType = eNoSensor;

  if (mUpdateTimer) {
    mUpdateTimer->Cancel();
    mUpdateTimer = nsnull;
  }
}

