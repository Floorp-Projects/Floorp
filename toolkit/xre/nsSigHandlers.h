/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__i386) || defined(__amd64__)

/*
 * x87 FPU Control Word:
 *
 * 0 -> IM  Invalid Operation
 * 1 -> DM  Denormalized Operand
 * 2 -> ZM  Zero Divide
 * 3 -> OM  Overflow
 * 4 -> UM  Underflow
 * 5 -> PM  Precision
 */
#define FPU_EXCEPTION_MASK 0x3f

/*
 * x86 FPU Status Word:
 *
 * 0..5  ->      Exception flags  (see x86 FPU Control Word)
 * 6     -> SF   Stack Fault
 * 7     -> ES   Error Summary Status
 */
#define FPU_STATUS_FLAGS 0xff

/*
 * MXCSR Control and Status Register:
 *
 * 0..5  ->      Exception flags (see x86 FPU Control Word)
 * 6     -> DAZ  Denormals Are Zero
 * 7..12 ->      Exception mask (see x86 FPU Control Word)
 */
#define SSE_STATUS_FLAGS   FPU_EXCEPTION_MASK
#define SSE_EXCEPTION_MASK (FPU_EXCEPTION_MASK << 7)

#endif
