/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdio.h>

#ifdef DEBUG
#define PRIO_DEBUG(msg)                                                        \
  do {                                                                         \
    fprintf(stderr, "Error: %s\n", msg);                                       \
  } while (false);
#else
#define PRIO_DEBUG(msg) ;
#endif

#endif /* __DEBUG_H__ */
