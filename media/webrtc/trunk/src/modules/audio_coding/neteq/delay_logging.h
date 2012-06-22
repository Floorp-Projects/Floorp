/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Contains definitions for the delay logging functionality. Only used for debugging and
 * tracing purposes.
 */

#ifndef DELAY_LOGGING_H
#define DELAY_LOGGING_H

#define NETEQ_DELAY_LOGGING_VERSION_STRING "2.0"

#define NETEQ_DELAY_LOGGING_SIGNAL_RECIN 1
#define NETEQ_DELAY_LOGGING_SIGNAL_FLUSH 2
#define NETEQ_DELAY_LOGGING_SIGNAL_CLOCK 3
#define NETEQ_DELAY_LOGGING_SIGNAL_EOF 4
#define NETEQ_DELAY_LOGGING_SIGNAL_DECODE 5
#define NETEQ_DELAY_LOGGING_SIGNAL_CHANGE_FS 6
#define NETEQ_DELAY_LOGGING_SIGNAL_MERGE_INFO 7
#define NETEQ_DELAY_LOGGING_SIGNAL_EXPAND_INFO 8
#define NETEQ_DELAY_LOGGING_SIGNAL_ACCELERATE_INFO 9
#define NETEQ_DELAY_LOGGING_SIGNAL_PREEMPTIVE_INFO 10
#define NETEQ_DELAY_LOGGING_SIGNAL_OPTBUF 11
#define NETEQ_DELAY_LOGGING_SIGNAL_DECODE_ONE_DESC 12

#endif
