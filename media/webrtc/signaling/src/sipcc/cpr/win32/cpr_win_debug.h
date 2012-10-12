/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_WIN_DEBUG_H_
#define _CPR_WIN_DEBUG_H_

/* Enumerated types to test out CPR functionality */
typedef enum {
   Test1Mutex,
   Test2Mutex,
   Test3Mutex
} mutex_t;

typedef enum {
   Test1MsgQueue,
   Test2MsgQueue,
   Test3MsgQueue
} msgQueue_t;

typedef enum {
   Test1Timer,
   Test2Timer,
   Test3Timer
} timer_t;

/* Test data to pass around the system */
//char testData1[15] = "Hello World";
//char testData2[15] = "allo monde";

#endif
