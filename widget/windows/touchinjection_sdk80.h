/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef touchinjection_sdk80_h
#define touchinjection_sdk80_h

#include <windows.h>

// Note, this isn't inclusive of all touch injection header info.
// You may need to add more to expand on current apis.

#ifndef TOUCH_FEEDBACK_DEFAULT

#  define TOUCH_FEEDBACK_DEFAULT 0x1
#  define TOUCH_FEEDBACK_INDIRECT 0x2
#  define TOUCH_FEEDBACK_NONE 0x3

enum POINTER_FEEDBACK_MODE {
  POINTER_FEEDBACK_DEFAULT =
      1,  // The injected pointer input feedback may get suppressed by the
          // end-user settings in the Pen and Touch control panel.
  POINTER_FEEDBACK_INDIRECT =
      2,  // The injected pointer input feedback overrides the end-user settings
          // in the Pen and Touch control panel.
  POINTER_FEEDBACK_NONE = 3,  // No touch visualizations.
};

enum {
  PT_POINTER = 0x00000001,   // Generic pointer
  PT_TOUCH = 0x00000002,     // Touch
  PT_PEN = 0x00000003,       // Pen
  PT_MOUSE = 0x00000004,     // Mouse
  PT_TOUCHPAD = 0x00000005,  // Touch pad
};

using POINTER_INPUT_TYPE = DWORD;
using POINTER_FLAGS = UINT32;

enum POINTER_BUTTON_CHANGE_TYPE {
  POINTER_CHANGE_NONE,
  POINTER_CHANGE_FIRSTBUTTON_DOWN,
  POINTER_CHANGE_FIRSTBUTTON_UP,
  POINTER_CHANGE_SECONDBUTTON_DOWN,
  POINTER_CHANGE_SECONDBUTTON_UP,
  POINTER_CHANGE_THIRDBUTTON_DOWN,
  POINTER_CHANGE_THIRDBUTTON_UP,
  POINTER_CHANGE_FOURTHBUTTON_DOWN,
  POINTER_CHANGE_FOURTHBUTTON_UP,
  POINTER_CHANGE_FIFTHBUTTON_DOWN,
  POINTER_CHANGE_FIFTHBUTTON_UP,
};

struct POINTER_INFO {
  POINTER_INPUT_TYPE pointerType;
  UINT32 pointerId;
  UINT32 frameId;
  POINTER_FLAGS pointerFlags;
  HANDLE sourceDevice;
  HWND hwndTarget;
  POINT ptPixelLocation;
  POINT ptHimetricLocation;
  POINT ptPixelLocationRaw;
  POINT ptHimetricLocationRaw;
  DWORD dwTime;
  UINT32 historyCount;
  INT32 InputData;
  DWORD dwKeyStates;
  UINT64 PerformanceCount;
  POINTER_BUTTON_CHANGE_TYPE ButtonChangeType;
};

using TOUCH_FLAGS = UINT32;
using TOUCH_MASK = UINT32;

struct POINTER_TOUCH_INFO {
  POINTER_INFO pointerInfo;
  TOUCH_FLAGS touchFlags;
  TOUCH_MASK touchMask;
  RECT rcContact;
  RECT rcContactRaw;
  UINT32 orientation;
  UINT32 pressure;
};

#  define PEN_FLAG_NONE 0x00000000      // Default
#  define PEN_FLAG_BARREL 0x00000001    // The barrel button is pressed
#  define PEN_FLAG_INVERTED 0x00000002  // The pen is inverted
#  define PEN_FLAG_ERASER 0x00000004    // The eraser button is pressed

#  define PEN_MASK_NONE \
    0x00000000  // Default - none of the optional fields are valid
#  define PEN_MASK_PRESSURE 0x00000001  // The pressure field is valid
#  define PEN_MASK_ROTATION 0x00000002  // The rotation field is valid
#  define PEN_MASK_TILT_X 0x00000004    // The tiltX field is valid
#  define PEN_MASK_TILT_Y 0x00000008    // The tiltY field is valid

using PEN_FLAGS = UINT32;
using PEN_MASK = UINT32;

struct POINTER_PEN_INFO {
  POINTER_INFO pointerInfo;
  PEN_FLAGS penFlags;
  PEN_MASK penMask;
  UINT32 pressure;
  UINT32 rotation;
  INT32 tiltX;
  INT32 tiltY;
};

struct POINTER_TYPE_INFO {
  POINTER_INPUT_TYPE type;
  union {
    POINTER_TOUCH_INFO touchInfo;
    POINTER_PEN_INFO penInfo;
  };
};

#  define TOUCH_FLAG_NONE 0x00000000  // Default

#  define TOUCH_MASK_NONE \
    0x00000000  // Default - none of the optional fields are valid
#  define TOUCH_MASK_CONTACTAREA 0x00000001  // The rcContact field is valid
#  define TOUCH_MASK_ORIENTATION 0x00000002  // The orientation field is valid
#  define TOUCH_MASK_PRESSURE 0x00000004     // The pressure field is valid

#  define POINTER_FLAG_NONE 0x00000000          // Default
#  define POINTER_FLAG_NEW 0x00000001           // New pointer
#  define POINTER_FLAG_INRANGE 0x00000002       // Pointer has not departed
#  define POINTER_FLAG_INCONTACT 0x00000004     // Pointer is in contact
#  define POINTER_FLAG_FIRSTBUTTON 0x00000010   // Primary action
#  define POINTER_FLAG_SECONDBUTTON 0x00000020  // Secondary action
#  define POINTER_FLAG_THIRDBUTTON 0x00000040   // Third button
#  define POINTER_FLAG_FOURTHBUTTON 0x00000080  // Fourth button
#  define POINTER_FLAG_FIFTHBUTTON 0x00000100   // Fifth button
#  define POINTER_FLAG_PRIMARY 0x00002000       // Pointer is primary
#  define POINTER_FLAG_CONFIDENCE \
    0x00004000  // Pointer is considered unlikely to be accidental
#  define POINTER_FLAG_CANCELED \
    0x00008000  // Pointer is departing in an abnormal manner
#  define POINTER_FLAG_DOWN \
    0x00010000  // Pointer transitioned to down state (made contact)
#  define POINTER_FLAG_UPDATE 0x00020000  // Pointer update
#  define POINTER_FLAG_UP \
    0x00040000  // Pointer transitioned from down state (broke contact)
#  define POINTER_FLAG_WHEEL 0x00080000           // Vertical wheel
#  define POINTER_FLAG_HWHEEL 0x00100000          // Horizontal wheel
#  define POINTER_FLAG_CAPTURECHANGED 0x00200000  // Lost capture

#endif  // TOUCH_FEEDBACK_DEFAULT

#define TOUCH_FLAGS_CONTACTUPDATE \
  (POINTER_FLAG_UPDATE | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT)
#define TOUCH_FLAGS_CONTACTDOWN \
  (POINTER_FLAG_DOWN | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT)

using InitializeTouchInjectionPtr = BOOL(WINAPI*)(UINT32, DWORD);
using InjectTouchInputPtr = BOOL(WINAPI*)(UINT32, const POINTER_TOUCH_INFO*);

#if !defined(NTDDI_WIN10_RS5) || (NTDDI_VERSION < NTDDI_WIN10_RS5)
#  define HSYNTHETICPOINTERDEVICE intptr_t
#endif  // NTDDI_VERSION < NTDDI_WIN10_RS5

using CreateSyntheticPointerDevicePtr = HSYNTHETICPOINTERDEVICE(WINAPI*)(
    POINTER_INPUT_TYPE, ULONG, POINTER_FEEDBACK_MODE);
using DestroySyntheticPointerDevicePtr = void(WINAPI*)(HSYNTHETICPOINTERDEVICE);
using InjectSyntheticPointerInputPtr = BOOL(WINAPI*)(HSYNTHETICPOINTERDEVICE,
                                                     const POINTER_TYPE_INFO*,
                                                     UINT32);

#endif  // touchinjection_sdk80_h
