/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/* We need this because Solaris' version of qsort is broken and
 * causes array bounds reads.
 */

#ifndef nsQuickSort_h___
#define nsQuickSort_h___

#include "nscore.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parameters:
 *  1. the array to sort
 *  2. the number of elements in the array
 *  3. the size of each array element
 *  4. comparison function taking two elements and parameter #5 and
 *     returning an integer:
 *      + less than zero if the first element should be before the second
 *      + 0 if the order of the elements does not matter
 *      + greater than zero if the second element should be before the first
 *  5. extra data to pass to comparison function
 */
void NS_QuickSort(void*, unsigned int, unsigned int,
                  int (*)(const void*, const void*, void*),
                  void*);

#ifdef __cplusplus
}
#endif

#endif /* nsQuickSort_h___ */
