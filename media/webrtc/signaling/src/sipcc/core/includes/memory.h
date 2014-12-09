/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MEMORY_H_
#define _MEMORY_H_

/*
 *  DESCRIPTION
 *     This module contains the functions which implement the dynamic memory
 *      management routines. The following assumptions/rules apply:
 *   1) Packets are allocated a minimum of MIN_BLOCK + BLOCK_OVERHEAD bytes.
 *   2) The size of the heap is set at link time, using the -heap flag
 *      The allocation and sizing of the heap is a cooperative effort
 *      involving the linker, this file, and "sysmem.c".
 *   3) The heap can be reset at any time by calling the function "minit"
 *
 */


/*
 *--------------------DEPRECATED FILE-------------------------------
 *
 * As part of Skittles project this file is deprecated.
 * DO NOT add anything to this file. use cpr_stdlib.h instead
 * If you are doing SYNC merges _DO_ _NOT_ merge anything from parent
 * This file is kept here because it comes from parent/grand parent branches
 * and will not be removed from clearcase till Skittles collapses.
 * [The fAQ on cc tools contains details of why ]
 *
 * If you have questions send email to skittles-dev
 *---------------------------------------------------------------
 */

#endif
