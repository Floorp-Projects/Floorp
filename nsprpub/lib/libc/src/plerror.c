/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
** File:plerror.c
** Description: Simple routine to print translate the calling thread's
**  error numbers and print them to "syserr".
*/

#include "plerror.h"

#include "prprf.h"
#include "prerror.h"

PR_IMPLEMENT(void) PL_FPrintError(PRFileDesc *fd, const char *msg)
{
static const char *tags[] =
{
    "PR_OUT_OF_MEMORY_ERROR",
    "PR_BAD_DESCRIPTOR_ERROR", 
    "PR_WOULD_BLOCK_ERROR",
    "PR_ACCESS_FAULT_ERROR", 
    "PR_INVALID_METHOD_ERROR", 
    "PR_ILLEGAL_ACCESS_ERROR", 
    "PR_UNKNOWN_ERROR",
    "PR_PENDING_INTERRUPT_ERROR",
    "PR_NOT_IMPLEMENTED_ERROR",
    "PR_IO_ERROR", 
    "PR_IO_TIMEOUT_ERROR", 
    "PR_IO_PENDING_ERROR", 
    "PR_DIRECTORY_OPEN_ERROR", 
    "PR_INVALID_ARGUMENT_ERROR", 
    "PR_ADDRESS_NOT_AVAILABLE_ERROR",
    "PR_ADDRESS_NOT_SUPPORTED_ERROR",
    "PR_IS_CONNECTED_ERROR", 
    "PR_BAD_ADDRESS_ERROR",
    "PR_ADDRESS_IN_USE_ERROR", 
    "PR_CONNECT_REFUSED_ERROR",
    "PR_NETWORK_UNREACHABLE_ERROR",
    "PR_CONNECT_TIMEOUT_ERROR",
    "PR_NOT_CONNECTED_ERROR",
    "PR_LOAD_LIBRARY_ERROR", 
    "PR_UNLOAD_LIBRARY_ERROR", 
    "PR_FIND_SYMBOL_ERROR",
    "PR_INSUFFICIENT_RESOURCES_ERROR", 
    "PR_DIRECTORY_LOOKUP_ERROR", 
    "PR_TPD_RANGE_ERROR",
    "PR_PROC_DESC_TABLE_FULL_ERROR", 
    "PR_SYS_DESC_TABLE_FULL_ERROR",
    "PR_NOT_SOCKET_ERROR", 
    "PR_NOT_TCP_SOCKET_ERROR", 
    "PR_SOCKET_ADDRESS_IS_BOUND_ERROR",
    "PR_NO_ACCESS_RIGHTS_ERROR", 
    "PR_OPERATION_NOT_SUPPORTED_ERROR",
    "PR_PROTOCOL_NOT_SUPPORTED_ERROR", 
    "PR_REMOTE_FILE_ERROR",
    "PR_BUFFER_OVERFLOW_ERROR",
    "PR_CONNECT_RESET_ERROR",
    "PR_RANGE_ERROR",
    "PR_DEADLOCK_ERROR", 
    "PR_FILE_IS_LOCKED_ERROR", 
    "PR_FILE_TOO_BIG_ERROR", 
    "PR_NO_DEVICE_SPACE_ERROR",
    "PR_PIPE_ERROR", 
    "PR_NO_SEEK_DEVICE_ERROR", 
    "PR_IS_DIRECTORY_ERROR", 
    "PR_LOOP_ERROR", 
    "PR_NAME_TOO_LONG_ERROR",
    "PR_FILE_NOT_FOUND_ERROR", 
    "PR_NOT_DIRECTORY_ERROR",
    "PR_READ_ONLY_FILESYSTEM_ERROR", 
    "PR_DIRECTORY_NOT_EMPTY_ERROR",
    "PR_FILESYSTEM_MOUNTED_ERROR", 
    "PR_NOT_SAME_DEVICE_ERROR",
    "PR_DIRECTORY_CORRUPTED_ERROR",
    "PR_FILE_EXISTS_ERROR",
    "PR_MAX_DIRECTORY_ENTRIES_ERROR",
    "PR_INVALID_DEVICE_STATE_ERROR", 
    "PR_DEVICE_IS_LOCKED_ERROR", 
    "PR_NO_MORE_FILES_ERROR",
    "PR_END_OF_FILE_ERROR",
    "PR_FILE_SEEK_ERROR",
    "PR_FILE_IS_BUSY_ERROR", 
    "<unused error code>",
    "PR_IN_PROGRESS_ERROR",
    "PR_ALREADY_INITIATED_ERROR",
    "PR_GROUP_EMPTY_ERROR",
    "PR_INVALID_STATE_ERROR",
    "PR_NETWORK_DOWN_ERROR",
    "PR_SOCKET_SHUTDOWN_ERROR",
    "PR_CONNECT_ABORTED_ERROR",
    "PR_HOST_UNREACHABLE_ERROR",
    "PR_MAX_ERROR"
};

PRErrorCode error = PR_GetError();
PRInt32 oserror = PR_GetOSError();
PRIntn thoseIKnowAbout = sizeof(tags) / sizeof(char*);
PRIntn lastError = PR_NSPR_ERROR_BASE + thoseIKnowAbout;

	if (NULL != msg) PR_fprintf(fd, "%s: ", msg);
    if ((error < PR_NSPR_ERROR_BASE) || (error > lastError))
        PR_fprintf(
			fd, " (%d)OUT OF RANGE, oserror = %d\n", error, oserror);
    else
        PR_fprintf(
            fd, "%s(%d), oserror = %d\n",
            tags[error - PR_NSPR_ERROR_BASE], error, oserror);
}  /* PL_FPrintError */

PR_IMPLEMENT(void) PL_PrintError(const char *msg)
{
	static PRFileDesc *fd = NULL;
	if (NULL == fd) fd = PR_GetSpecialFD(PR_StandardError);
	PL_FPrintError(fd, msg);
}  /* PL_PrintError */

#if defined(WIN16)
/*
** libmain() is a required function for win16
**
*/
int CALLBACK LibMain( HINSTANCE hInst, WORD wDataSeg, 
  WORD cbHeapSize, LPSTR lpszCmdLine )
{
return TRUE;
}
#endif /* WIN16 */





/* plerror.c */
