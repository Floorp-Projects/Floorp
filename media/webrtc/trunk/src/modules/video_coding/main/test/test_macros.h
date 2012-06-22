/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VCM_TEST_MACROS_H
#define VCM_TEST_MACROS_H

#include <cstdio>
#include <cstdlib>

extern int vcmMacrosTests;
extern int vcmMacrosErrors;

#define PRINT_ERR_MSG(msg)                              \
    do {                                                \
        fprintf(stderr, "Error at line %i of %s\n%s",   \
            __LINE__, __FILE__, msg);                   \
    } while(0)

#define TEST(expr)                                              \
    do {                                                        \
        vcmMacrosTests++;                                       \
        if (!(expr)) {                                          \
            PRINT_ERR_MSG("Assertion failed: " #expr "\n\n");   \
            vcmMacrosErrors++;                                  \
        }                                                       \
    } while(0)

#define TEST_EXIT_ON_FAIL(expr)                                             \
    do {                                                                    \
        vcmMacrosTests++;                                                   \
        if (!(expr)) {                                                      \
            PRINT_ERR_MSG("Assertion failed: " #expr "\nExiting...\n\n");   \
            vcmMacrosErrors++;                                              \
            exit(EXIT_FAILURE);                                             \
        }                                                                   \
    } while(0)

#endif
