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
 *   Jesper Kristensen <mail@jesperkristensen.dk>
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

#include "nsAccelerometerWin.h"
#include "nsIServiceManager.h"
#include "windows.h"

#ifdef WINCE_WINDOWS_MOBILE

////////////////////////////
// HTC 
////////////////////////////


typedef struct _SENSORDATA
{
    SHORT   TiltX;          // From -1000 to 1000 (about), 0 is flat
    SHORT   TiltY;          // From -1000 to 1000 (about), 0 is flat
    SHORT   Orientation;    // From -1000 to 1000 (about)

    WORD    Unknown1;       // Always zero
    DWORD   AngleY;         // From 0 to 359
    DWORD   AngleX;         // From 0 to 359
    DWORD   Unknown2;       // Bit field?
} SENSORDATA, *PSENSORDATA;

typedef HANDLE (WINAPI * HTCSensorOpen)(DWORD);
typedef void   (WINAPI * HTCSensorClose)(HANDLE);
typedef DWORD  (WINAPI * HTCSensorGetDataOutput)(HANDLE, PSENSORDATA);

HTCSensorOpen           gHTCSensorOpen = nsnull;
HTCSensorClose          gHTCSensorClose = nsnull;
HTCSensorGetDataOutput  gHTCSensorGetDataOutput = nsnull;

class HTCSensor : public Sensor
{
public:
  HTCSensor();
  ~HTCSensor();
  PRBool Startup();
  void Shutdown();
  void GetValues(double *x, double *y, double *z);
private:
  HMODULE mLibrary;
  HANDLE  mHTCHandle;
};

HTCSensor::HTCSensor()
{
}

HTCSensor::~HTCSensor()
{
}

PRBool
HTCSensor::Startup()
{
  HMODULE hSensorLib = LoadLibraryW(L"HTCSensorSDK.dll");
  
  if (!hSensorLib)
    return PR_FALSE;

  gHTCSensorOpen = (HTCSensorOpen) GetProcAddressW(hSensorLib, L"HTCSensorOpen");
  gHTCSensorClose = (HTCSensorClose) GetProcAddressW(hSensorLib, L"HTCSensorClose");
  gHTCSensorGetDataOutput = (HTCSensorGetDataOutput) GetProcAddressW(hSensorLib,
                                                                           L"HTCSensorGetDataOutput");

  if (gHTCSensorOpen != nsnull && gHTCSensorClose != nsnull &&
      gHTCSensorGetDataOutput != nsnull) {
      mHTCHandle = gHTCSensorOpen(1);
      if (mHTCHandle)
       return PR_TRUE;
    }

  FreeLibrary(hSensorLib);
  mLibrary = nsnull;
  gHTCSensorOpen = nsnull;
  gHTCSensorClose = nsnull;
  gHTCSensorGetDataOutput = nsnull;
  return PR_FALSE;
}

void
HTCSensor::Shutdown()
{
  NS_ASSERTION(mHTCHandle, "mHTCHandle should not be null at shutdown!");
  gHTCSensorClose(mHTCHandle);

  NS_ASSERTION(mLibrary, "Shutdown called when mLibrary is null?");
  FreeLibrary(mLibrary);
  mLibrary = nsnull;
  gHTCSensorOpen = nsnull;
  gHTCSensorClose = nsnull;
  gHTCSensorGetDataOutput = nsnull;
}

void
HTCSensor::GetValues(double *x, double *y, double *z)
{
  if (!mHTCHandle)
    return;

  static const double htcScalingFactor = 1.0 / 1000.0 * 9.8;

  SENSORDATA sd;
  gHTCSensorGetDataOutput(mHTCHandle, &sd);

  *x = ((double)sd.TiltX) / 980 ;
  *y = ((double)sd.TiltY) / 980 ;
  *z = ((double)sd.Orientation) / 1000;
}


////////////////////////////
// Samsung
////////////////////////////

/**
* The result of an SMI SDK function call.
*/
typedef UINT SMI_RESULT;

/**
 * Common result codes.
*/
typedef enum
{
  SMI_SUCCESS                     = 0x00000000,
  SMI_ERROR_UNKNOWN               = 0x00000001,
  SMI_ERROR_DEVICE_NOT_FOUND      = 0x00000002,
  SMI_ERROR_DEVICE_DISABLED       = 0x00000003,
  SMI_ERROR_PERMISSION_DENIED     = 0x00000004,
  SMI_ERROR_INVALID_PARAMETER     = 0x00000005,
  SMI_ERROR_CANNOT_ACTIVATE_SERVER= 0x00000006,
  SMI_ACCELEROMETER_RESULT_BASE   = 0x10010000,
  SMI_HAPTICS_RESULT_BASE         = 0x10020000,
  SMI_LED_RESULT_BASE             = 0x10030000
} SmiResultCode;

/**
 *  Accelerometer vector data.
 *
 */
typedef struct
{
  FLOAT x;     /**< X-direction value */
  FLOAT y;     /**< Y-direction value */
  FLOAT z;     /**< Z-direction value */
} SmiAccelerometerVector;

/** 
 *  Specifies the capabilities of the Accelerometer device. 
 */
typedef struct
{
  UINT callbackPeriod;     /**<The unit of the vector sampling time, in milliseconds. */
} SmiAccelerometerCapabilities;

typedef SMI_RESULT (WINAPI * SmiAccelerometerGetVector)(SmiAccelerometerVector *);
typedef void (*SmiAccelerometerHandler)(SmiAccelerometerVector accel);

SmiAccelerometerGetVector         gSmiAccelerometerGetVector = nsnull;

class SMISensor : public Sensor
{
public:
  SMISensor();
  ~SMISensor();
  PRBool Startup();
  void Shutdown();
  void GetValues(double *x, double *y, double *z);
private:
  HMODULE mLibrary;
};

SMISensor::SMISensor()
  :mLibrary(nsnull)
{
}

SMISensor::~SMISensor()
{
}

PRBool
SMISensor::Startup()
{
  HMODULE hSensorLib = LoadLibraryW(L"SamsungMobileSDK_1.dll");

  if (!hSensorLib)
    return PR_FALSE;

  gSmiAccelerometerGetVector = (SmiAccelerometerGetVector)
    GetProcAddressW(hSensorLib, L"SmiAccelerometerGetVector");

  if (gSmiAccelerometerGetVector == nsnull) {
    FreeLibrary(hSensorLib);
    mLibrary = nsnull;
    gSmiAccelerometerGetVector = nsnull;
    return PR_FALSE;
  }

  mLibrary = hSensorLib;
  return PR_TRUE;

}

void
SMISensor::Shutdown()
{
  NS_ASSERTION(mLibrary, "Shutdown called when mLibrary is null?");
  FreeLibrary(mLibrary);
  mLibrary = nsnull;
  gSmiAccelerometerGetVector = nsnull;
}

void
SMISensor::GetValues(double *x, double *y, double *z)
{
  NS_ASSERTION(mLibrary, "mLibrary should not be null when GetValues is called");

  SmiAccelerometerVector vector;
  vector.x = vector.y = vector.z = 0;
  SMI_RESULT result = gSmiAccelerometerGetVector(&vector);

  // don't we have to adjust
  *x = vector.x;
  *y = vector.y;
  *z = vector.z;
}

////////////////////////////
// Toshiba TG01 / T-01A
////////////////////////////

typedef struct _TS_ACCELERATION
{
  SHORT x;
  SHORT y;
  SHORT z;
  FILETIME time;
} TS_ACCELERATION;

typedef DWORD (WINAPI *TSRegisterAcceleration)(HANDLE,DWORD,DWORD*);
typedef DWORD (WINAPI *TSDeregisterAcceleration)(DWORD);
typedef DWORD (WINAPI *TSGetAcceleration)(DWORD,TS_ACCELERATION*);

TSRegisterAcceleration gTSRegisterAcceleration = nsnull;
TSDeregisterAcceleration gTSDeregisterAcceleration = nsnull;
TSGetAcceleration gTSGetAcceleration = nsnull;

class TsSensor : public Sensor
{
public:
  TsSensor();
  ~TsSensor();
  PRBool Startup();
  void Shutdown();
  void GetValues(double *x, double *y, double *z);
private:
  HMODULE mLibrary;
  DWORD mId;
};

TsSensor::TsSensor() : mLibrary(nsnull), mId(0)
{
}

TsSensor::~TsSensor()
{
}

PRBool
TsSensor::Startup()
{
  HMODULE hSensorLib = LoadLibraryW(L"axcon.dll");

  if (!hSensorLib)
    return PR_FALSE;

  gTSRegisterAcceleration = (TSRegisterAcceleration) GetProcAddressW(hSensorLib, L"TSRegisterAcceleration");
  gTSDeregisterAcceleration = (TSDeregisterAcceleration) GetProcAddressW(hSensorLib, L"TSDeregisterAcceleration");
  gTSGetAcceleration = (TSGetAcceleration) GetProcAddressW(hSensorLib, L"TSGetAcceleration");

  if (gTSRegisterAcceleration && gTSDeregisterAcceleration && gTSGetAcceleration) {
    if (!gTSRegisterAcceleration(GetModuleHandle(NULL), 100, &mId)) {
      mLibrary = hSensorLib;
      return PR_TRUE;
    }
  }

  FreeLibrary(hSensorLib);

  mLibrary = nsnull;
  mId = 0;
  gTSRegisterAcceleration = nsnull;
  gTSDeregisterAcceleration = nsnull;
  gTSGetAcceleration = nsnull;

  return PR_FALSE;
}

void
TsSensor::Shutdown()
{
  NS_ASSERTION(mLibrary, "Shutdown called when mLibrary is null?");

  gTSDeregisterAcceleration(mId);
  FreeLibrary(mLibrary);

  mLibrary = nsnull;
  mId = 0;
  gTSRegisterAcceleration = nsnull;
  gTSDeregisterAcceleration = nsnull;
  gTSGetAcceleration = nsnull;
}

void
TsSensor::GetValues(double *x, double *y, double *z)
{
  NS_ASSERTION(mLibrary, "mLibrary should not be null when GetValues is called");

  TS_ACCELERATION data;
  gTSGetAcceleration(mId, &data);

  // Value for TG-01 is landscaped
  *x = ((double)data.y) / 1000;
  *y = ((double)data.x) / 1000;
  *z = ((double)data.z) / 1000;
}

#endif // WINCE_WINDOWS_MOBILE

#if !defined(WINCE) && !defined(WINCE_WINDOWS_MOBILE) // normal windows.

////////////////////////////
// ThinkPad
////////////////////////////

typedef struct {
  int status; // Current internal state
  unsigned short x; // raw value
  unsigned short y; // raw value
  unsigned short xx; // avg. of 40ms
  unsigned short yy; // avg. of 40ms
  char temp; // raw value (could be deg celsius?)
  unsigned short x0; // Used for "auto-center"
  unsigned short y0; // Used for "auto-center"
} ThinkPadAccelerometerData;

typedef void (__stdcall *ShockproofGetAccelerometerData)(ThinkPadAccelerometerData*);

ShockproofGetAccelerometerData gShockproofGetAccelerometerData = nsnull;

class ThinkPadSensor : public Sensor
{
public:
  ThinkPadSensor();
  ~ThinkPadSensor();
  PRBool Startup();
  void Shutdown();
  void GetValues(double *x, double *y, double *z);
private:
  HMODULE mLibrary;
};

ThinkPadSensor::ThinkPadSensor()
{
}

ThinkPadSensor::~ThinkPadSensor()
{
}

PRBool
ThinkPadSensor::Startup()
{
  mLibrary = LoadLibrary("sensor.dll");
  if (!mLibrary)
    return PR_FALSE;

  gShockproofGetAccelerometerData = (ShockproofGetAccelerometerData)
    GetProcAddress(mLibrary, "ShockproofGetAccelerometerData");
  if (!gShockproofGetAccelerometerData) {
    FreeLibrary(mLibrary);
    mLibrary = nsnull;
    return PR_FALSE;
  }
  return PR_TRUE;
}

void
ThinkPadSensor::Shutdown()
{
  NS_ASSERTION(mLibrary, "Shutdown called when mLibrary is null?");
  FreeLibrary(mLibrary);
  mLibrary = nsnull;
  gShockproofGetAccelerometerData = nsnull;
}

void
ThinkPadSensor::GetValues(double *x, double *y, double *z)
{
  ThinkPadAccelerometerData accelData;

  gShockproofGetAccelerometerData(&accelData);

  // accelData.x and accelData.y is the acceleration measured from the accelerometer.
  // x and y is switched from what we use, and the accelerometer does not support z axis.
  // Balance point (526, 528) and 90 degree tilt (144) determined experimentally.
  *x = ((double)(accelData.y - 526)) / 144;
  *y = ((double)(accelData.x - 528)) / 144;
  *z = 1.0;
}

#endif

nsAccelerometerWin::nsAccelerometerWin(){}
nsAccelerometerWin::~nsAccelerometerWin(){}

void
nsAccelerometerWin::UpdateHandler(nsITimer *aTimer, void *aClosure)
{
  nsAccelerometerWin *self = reinterpret_cast<nsAccelerometerWin *>(aClosure);
  if (!self || !self->mSensor) {
    NS_ERROR("no self or sensor");
    return;
  }
  double x, y, z;
  self->mSensor->GetValues(&x, &y, &z);
  self->AccelerationChanged(x, y, z);
}

void nsAccelerometerWin::Startup()
{
  NS_ASSERTION(!mSensor, "mSensor should be null.  Startup called twice?");

  PRBool started = PR_FALSE;

#ifdef WINCE_WINDOWS_MOBILE

  mSensor = new SMISensor();
  if (mSensor)
    started = mSensor->Startup();

  if (!started) {
    mSensor = new HTCSensor();
    if (mSensor)
      started = mSensor->Startup();
  }

  if (!started) {
    mSensor = new TsSensor();
    if (mSensor)
      started = mSensor->Startup();
  }

#endif

#if !defined(WINCE) && !defined(WINCE_WINDOWS_MOBILE) // normal windows.

  mSensor = new ThinkPadSensor();
  if (mSensor)
    started = mSensor->Startup();

#endif
  
  if (!started)
    return;

  mUpdateTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (mUpdateTimer)
    mUpdateTimer->InitWithFuncCallback(UpdateHandler,
                                       this,
                                       mUpdateInterval,
                                       nsITimer::TYPE_REPEATING_SLACK);
}

void nsAccelerometerWin::Shutdown()
{
  if (mUpdateTimer) {
    mUpdateTimer->Cancel();
    mUpdateTimer = nsnull;
  }

  if (mSensor)
    mSensor->Shutdown();
}

