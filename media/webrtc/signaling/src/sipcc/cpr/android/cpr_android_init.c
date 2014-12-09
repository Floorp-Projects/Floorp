/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  @section intro_sec Introduction
 *  CPR is an OS-abstraction layer which provides the functionality set of an OS
 *  while hiding all the operational details from the applications. CPR also
 *  provides additional functionality when certain operating-systems prove
 *  deficient providing a consistent set of functionality for the applications
 *  to use. The pSipCC library uses the CPR routines to achieve portablity. It
 *  is the responsibility of any vendor requiring to port pSipCC into their
 *  platforms to have the correct implementation of the CPR layer.
 *
 *  The CPR delivery includes two "archives/binary" for memory and string
 *  related operations. These binaries contain functions that the pSIPCC uses
 *  for it's functionality. The header files that contain the external functions
 *  (APIs) from these binaries are also distributed.
 *
 *  @section sub_sec Functionality
 *  CPR consists of a number of functional subsystems to achieve OS abstraction. The main
 *  functionality provided by CPR to the pSIPCC library are
 *  @li Memory Management - Provided as a binary that needs to be linked into CPR
 *  @li Timers
 *  @li Inter Process Communication - Sockets, Message Queues, Mutex, Semaphores
 *  @li String Handling - Provided as a binary that needs to be linked into CPR
 *  @li Thread Abstraction
 *  @li Other Platform/OS Abstractions
 *  @li Debug/Logging Abstraction
 *
 *  @section file_list EXTERNAL APIS
 *   The External APIs that need to be exposed by CPR to the pSIPCC application are
 *   defined in the following header files. The documentation within
 *   each header file lists the functions/macros that @b NEED to be defined for
 *   pSIPCC to work correctly. Example functions (and an implementation for
 *   Linux) is available for most functions defined in these headers. Look under
 *   the "Modules" tab to find the various subsystems and associated APIs and
 *   helper functions.
 *   @li cpr_debug.h
 *   @li cpr_errno.h
 *   @li cpr_in.h
 *   @li cpr_locks.h
 *   @li cpr_rand.h
 *   @li cpr_socket.h
 *   @li cpr_stdio.h
 *   @li cpr_threads.h
 *   @li cpr_timers.h
 *   @li cpr.h
 *   @li cpr_ipc.h
 *   @li cpr_stddef.h
 *   @li cpr_time.h
 *   @li cpr_types.h
 *
 *   The function prototypes in these header files are implemented in the
 *   binaries related to memory and string functionality. The prototypes are
 *   given so that vendors can use these functions for implementation of other
 *   CPR parts.
 *   @li cpr_memory.h
 *   @li cpr_stdlib.h
 *   @li cpr_string.h
 *   @li cpr_strings.h
 *
 *  @section standard_headers Standard Header Files
 *  The pSIPCC component expects some standard header files and associated
 *  functionality to be present in the native operating system of the vendors
 *  porting it. These header files contain some basic functionality that pSIPCC
 *  uses for proper operation. A list of the standard headers (from a standard
 *  Linux distribution) are -
 *   @li <assert.h>
 *   @li <errno.h>
 *   @li <arpa/inet.h>
 *   @li <netinet/in.h>
 *   @li <netinet/tcp.h>
 *   @li <pthread.h>
 *   @li <sys/socket.h>
 *   @li <sys/select.h>
 *   @li <sys/un.h>
 *   @li <unistd.h>
 *   @li <stdarg.h>
 *   @li <stdio.h>
 *   @li <stdlib.h>
 *   @li <string.h>
 *   @li <ctype.h>
 *   @li <strings.h>
 *   @li <time.h>
 *
 *
 *  @section Comp Compiling CPR
 *  A sample makefile is also provided with the Linux distribution. Running
 *  "make" generates a CPR executable called "cprtest". This sample makefile can
 *  be modified to generate a shared library if required. Modify the CC, STDINC,
 *  STDLIB paths to point to the correct compiler and header files in the
 *  environment where this is being built.
 *  @note The README file shows the exact commands required to build/test the CPR
 *  functionality.
 *
 *  @file cpr_linux_init.c
 *  @brief This file contains CPR initialization routines
 *
 *  DESCRIPTION
 *     Initialization routine for the Cisco Portable Runtime layer
 *     running in the Linux operating System.
 */

/**
  * @addtogroup Initialization The initialization module
  * @ingroup CPR
  * @brief The intialization module consists of APIs used to initialize or destroy the CPR layer by pSipCC
  *
  * @{
  */
#include "cpr.h"
#include "cpr_stdlib.h"
#include "cpr_stdio.h"
#include "cpr_timers.h"
#include "cpr_android_timers.h"
#include "plat_api.h"
#include <errno.h>
#ifndef ANDROID
#include <sys/msg.h>
#include <sys/ipc.h>
#endif
#include "plat_debug.h"

/**
  * Boolean to check that cprPreInit been called
  */
static boolean pre_init_called = FALSE;

/**
 * cprInfo prints out informational messages that are
 * not necessarily errors, but maybe of interest to
 * someone debugging a problem. Examples of this are
 * requesting a msg from a msg queue that is empty,
 * stopping a timer that is not running, etc...
 */
int32_t cprInfo = TRUE;


/**
 * cprPreInit
 *
 * @brief The cprPreInit function IS called from pSIPCC @b before any components are initialized.
 *
 * This function @b SHOULD initialize those portions of the CPR that
 * are needed before applications can start using it. The memory subsystem
 * (sandbox) is initialized from this routine.
 *
 *
 * @return CPR_SUCCESS or CPR_FAILURE
 * @note pSIPCC will NOT continue and stop initialization if the return value is CPR_FAILURE.
 */
cprRC_t
cprPreInit (void)
{
    static const char fname[] = "cprPreInit";
    int32_t returnCode;

    /*
     * Make function reentreant
     */
    if (pre_init_called == TRUE) {
        return CPR_SUCCESS;
    }
    pre_init_called = TRUE;
    (void)fname;
    (void)returnCode;
    return CPR_SUCCESS;
}


/**
 * cprPostInit
 *
 * @brief The cprPostInit function IS called from pSIPCC @b after all the components are initialized.
 *
 * This function @b SHOULD complete any CPR activities before the phone is
 * operational. In other words a call to this function will be the last line of
 * the phone initializtion routine from pSIPCC. This function implementation
 * ties in a couple of debug commands to get more information on the CPR from
 * the shell.
 *
 *
 * @return CPR_SUCCESS or CPR_FAILURE
 * @note pSIPCC will NOT continue and stop initialization if the return value is CPR_FAILURE.
 */
cprRC_t
cprPostInit (void)
{
/*    if (cpr_memory_mgmt_post_init() not_eq TRUE) {
        return CPR_FAILURE;
    }
*/
    return CPR_SUCCESS;
}

