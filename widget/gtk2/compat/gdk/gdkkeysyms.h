/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GDKKEYSYMS_WRAPPER_H
#define GDKKEYSYMS_WRAPPER_H

#include_next <gdk/gdkkeysyms.h>

#ifndef GDK_ISO_Level5_Shift
#define GDK_ISO_Level5_Shift    0xFE11
#endif

#ifndef GDK_ISO_Level5_Latch
#define GDK_ISO_Level5_Latch    0xFE12
#endif

#ifndef GDK_ISO_Level5_Lock
#define GDK_ISO_Level5_Lock     0xFE13
#endif

#ifndef GDK_dead_greek
#define GDK_dead_greek          0xFE8C
#endif

#ifndef GDK_ch
#define GDK_ch                  0xFEA0
#endif

#ifndef GDK_Ch
#define GDK_Ch                  0xFEA1
#endif

#ifndef GDK_CH
#define GDK_CH                  0xFEA2
#endif

#ifndef GDK_c_h
#define GDK_c_h                 0xFEA3
#endif

#ifndef GDK_C_h
#define GDK_C_h                 0xFEA4
#endif

#ifndef GDK_C_H
#define GDK_C_H                 0xFEA5
#endif

#ifndef GDK_TouchpadToggle
#define GDK_TouchpadToggle      0x1008FFA9
#endif

#ifndef GDK_TouchpadOn
#define GDK_TouchpadOn          0x1008FFB0
#endif

#ifndef GDK_TouchpadOff
#define GDK_TouchpadOff         0x1008ffb1
#endif

#ifndef GDK_LogWindowTree
#define GDK_LogWindowTree       0x1008FE24
#endif

#ifndef GDK_LogGrabInfo
#define GDK_LogGrabInfo         0x1008FE25
#endif

#ifndef GDK_Sleep
#define GDK_Sleep               0x1008FF2F
#endif

#endif /* GDKKEYSYMS_WRAPPER_H */
