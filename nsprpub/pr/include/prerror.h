/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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

#ifndef prerror_h___
#define prerror_h___

#include "prtypes.h"

PR_BEGIN_EXTERN_C

typedef PRInt32 PRErrorCode;

#define PR_NSPR_ERROR_BASE -2600

#define PR_OUT_OF_MEMORY_ERROR              PR_NSPR_ERROR_BASE + 0
                                    /* Insufficient memory to perform request   */
#define PR_BAD_DESCRIPTOR_ERROR             PR_NSPR_ERROR_BASE + 1
                                    /* the file descriptor used as an argument
                                       in the function is invalid; either it has
                                       been deleted or otherwise corrupted.     */
#define PR_WOULD_BLOCK_ERROR                PR_NSPR_ERROR_BASE + 2
                                    /* The operation would have blocked and that
                                       is in conflict with the semantics that
                                       have been established.                   */
#define PR_ACCESS_FAULT_ERROR               PR_NSPR_ERROR_BASE + 3
#define PR_INVALID_METHOD_ERROR             PR_NSPR_ERROR_BASE + 4
                                    /* The method being called is invalid for
                                       the type of file descriptor used.        */
#define PR_ILLEGAL_ACCESS_ERROR             PR_NSPR_ERROR_BASE + 5
#define PR_UNKNOWN_ERROR                    PR_NSPR_ERROR_BASE + 6
                                    /* Some unknown error has occured */
#define PR_PENDING_INTERRUPT_ERROR          PR_NSPR_ERROR_BASE + 7
                                    /* The operation terminated because another
                                       thread has interrupted it (PR_Interrupt) */
#define PR_NOT_IMPLEMENTED_ERROR            PR_NSPR_ERROR_BASE + 8
                                    /* The function called has not been
                                       implemented.                             */
#define PR_IO_ERROR                         PR_NSPR_ERROR_BASE + 9
#define PR_IO_TIMEOUT_ERROR                 PR_NSPR_ERROR_BASE + 10
                                    /* The I/O operation has not completed in
                                       the time specified for the function.     */
#define PR_IO_PENDING_ERROR                 PR_NSPR_ERROR_BASE + 11
                                    /* An I/O operation has been attempted on
                                       a file descriptor that is currently
                                       busy with another operation.             */
#define PR_DIRECTORY_OPEN_ERROR             PR_NSPR_ERROR_BASE + 12
                                    /* The directory could not be opened.       */
#define PR_INVALID_ARGUMENT_ERROR           PR_NSPR_ERROR_BASE + 13
                                    /* One or more of the arguments to the 
                                       function is invalid.                     */
#define PR_ADDRESS_NOT_AVAILABLE_ERROR      PR_NSPR_ERROR_BASE + 14
                                    /* The network address (PRNetAddr) is not
                                       available (probably in use).             */
#define PR_ADDRESS_NOT_SUPPORTED_ERROR      PR_NSPR_ERROR_BASE + 15
#define PR_IS_CONNECTED_ERROR               PR_NSPR_ERROR_BASE + 16
                                    /* An attempt to connect on an already
                                       connected network file descriptor.       */ 
#define PR_BAD_ADDRESS_ERROR                PR_NSPR_ERROR_BASE + 17
                                     /* The network address specified is invalid
                                       (as reported by the network).            */
#define PR_ADDRESS_IN_USE_ERROR             PR_NSPR_ERROR_BASE + 18
#define PR_CONNECT_REFUSED_ERROR            PR_NSPR_ERROR_BASE + 19
                                    /* The peer has refused to allow the connec-
                                       tion to be established.                  */
#define PR_NETWORK_UNREACHABLE_ERROR        PR_NSPR_ERROR_BASE + 20
                                    /* The network address specifies a host
                                       that is unreachable (perhaps temporary). */
#define PR_CONNECT_TIMEOUT_ERROR            PR_NSPR_ERROR_BASE + 21
                                    /* The connection attempt did not complete
                                       in a reasonable period of time.          */
#define PR_NOT_CONNECTED_ERROR              PR_NSPR_ERROR_BASE + 22
                                    /* The call attempted to use connected
                                       sematics on a network file descriptor
                                       that was not connected.                  */
#define PR_LOAD_LIBRARY_ERROR               PR_NSPR_ERROR_BASE + 23
                                    /* Some sort of failure attempting to load
                                       a dynamic library.                       */
#define PR_UNLOAD_LIBRARY_ERROR             PR_NSPR_ERROR_BASE + 24
                                    /* Some sort of failure attempting to unload
                                       a dynamic library.                       */
#define PR_FIND_SYMBOL_ERROR                PR_NSPR_ERROR_BASE + 25
                                    /* Dynamic library symbol could not be found
                                       in any of the available libraries.       */
#define PR_INSUFFICIENT_RESOURCES_ERROR     PR_NSPR_ERROR_BASE + 26
                                    /* There are insufficient system resources
                                       to process the request.                  */
#define PR_DIRECTORY_LOOKUP_ERROR           PR_NSPR_ERROR_BASE + 27
#define PR_TPD_RANGE_ERROR                  PR_NSPR_ERROR_BASE + 28
                                    /* Attempt to access a TPD key that is beyond
                                       any key that has been allocated to the
                                       process.                                 */

#define PR_PROC_DESC_TABLE_FULL_ERROR       PR_NSPR_ERROR_BASE + 29
#define PR_SYS_DESC_TABLE_FULL_ERROR        PR_NSPR_ERROR_BASE + 30
#define PR_NOT_SOCKET_ERROR                 PR_NSPR_ERROR_BASE + 31
#define PR_NOT_TCP_SOCKET_ERROR             PR_NSPR_ERROR_BASE + 32
#define PR_SOCKET_ADDRESS_IS_BOUND_ERROR    PR_NSPR_ERROR_BASE + 33
#define PR_NO_ACCESS_RIGHTS_ERROR           PR_NSPR_ERROR_BASE + 34
#define PR_OPERATION_NOT_SUPPORTED_ERROR    PR_NSPR_ERROR_BASE + 35
#define PR_PROTOCOL_NOT_SUPPORTED_ERROR     PR_NSPR_ERROR_BASE + 36
#define PR_REMOTE_FILE_ERROR                PR_NSPR_ERROR_BASE + 37
#define PR_BUFFER_OVERFLOW_ERROR            PR_NSPR_ERROR_BASE + 38
#define PR_CONNECT_RESET_ERROR              PR_NSPR_ERROR_BASE + 39
#define PR_RANGE_ERROR                      PR_NSPR_ERROR_BASE + 40

#define PR_DEADLOCK_ERROR                   PR_NSPR_ERROR_BASE + 41
#define PR_FILE_IS_LOCKED_ERROR             PR_NSPR_ERROR_BASE + 42
#define PR_FILE_TOO_BIG_ERROR               PR_NSPR_ERROR_BASE + 43
#define PR_NO_DEVICE_SPACE_ERROR            PR_NSPR_ERROR_BASE + 44
#define PR_PIPE_ERROR                       PR_NSPR_ERROR_BASE + 45
#define PR_NO_SEEK_DEVICE_ERROR             PR_NSPR_ERROR_BASE + 46
#define PR_IS_DIRECTORY_ERROR               PR_NSPR_ERROR_BASE + 47
#define PR_LOOP_ERROR                       PR_NSPR_ERROR_BASE + 48
#define PR_NAME_TOO_LONG_ERROR              PR_NSPR_ERROR_BASE + 49
#define PR_FILE_NOT_FOUND_ERROR             PR_NSPR_ERROR_BASE + 50
#define PR_NOT_DIRECTORY_ERROR              PR_NSPR_ERROR_BASE + 51
#define PR_READ_ONLY_FILESYSTEM_ERROR       PR_NSPR_ERROR_BASE + 52
#define PR_DIRECTORY_NOT_EMPTY_ERROR        PR_NSPR_ERROR_BASE + 53
#define PR_FILESYSTEM_MOUNTED_ERROR         PR_NSPR_ERROR_BASE + 54
#define PR_NOT_SAME_DEVICE_ERROR            PR_NSPR_ERROR_BASE + 55
#define PR_DIRECTORY_CORRUPTED_ERROR        PR_NSPR_ERROR_BASE + 56
#define PR_FILE_EXISTS_ERROR                PR_NSPR_ERROR_BASE + 57
#define PR_MAX_DIRECTORY_ENTRIES_ERROR      PR_NSPR_ERROR_BASE + 58
#define PR_INVALID_DEVICE_STATE_ERROR       PR_NSPR_ERROR_BASE + 59
#define PR_DEVICE_IS_LOCKED_ERROR           PR_NSPR_ERROR_BASE + 60
#define PR_NO_MORE_FILES_ERROR              PR_NSPR_ERROR_BASE + 61
#define PR_END_OF_FILE_ERROR                PR_NSPR_ERROR_BASE + 62
#define PR_FILE_SEEK_ERROR                  PR_NSPR_ERROR_BASE + 63
#define PR_FILE_IS_BUSY_ERROR               PR_NSPR_ERROR_BASE + 64
	
#define PR_IN_PROGRESS_ERROR                PR_NSPR_ERROR_BASE + 66
#define PR_ALREADY_INITIATED_ERROR          PR_NSPR_ERROR_BASE + 67
#define PR_GROUP_EMPTY_ERROR                PR_NSPR_ERROR_BASE + 68
#define PR_INVALID_STATE_ERROR              PR_NSPR_ERROR_BASE + 69
#define PR_MAX_ERROR                        PR_NSPR_ERROR_BASE + 70
                                    /* Place holder for the end of the list     */

/*
** Set error will preserve an error condition within a thread context.
** The values stored are the NSPR (platform independent) translation of
** the error. Also, if available, the platform specific oserror is stored.
** If there is no appropriate OS error number, a zero my be supplied.
*/
PR_EXTERN(void) PR_SetError(PRErrorCode errorCode, PRInt32 oserr);

/*
** The text value specified may be NULL. If it is not NULL and the text length
** is zero, the string is assumed to be a null terminated C string. Otherwise
** the text is assumed to be the length specified and possibly include NULL
** characters (e.g., a multi-national string).
**
** The text will be copied into to thread structure and remain there
** until the next call to PR_SetError.
*/
PR_EXTERN(void) PR_SetErrorText(
    PRIntn textLength, const char *text);

/*
** Return the current threads last set error code.
*/
PR_EXTERN(PRErrorCode) PR_GetError(void);

/*
** Return the current threads last set os error code. This is used for
** machine specific code that desires the underlying os error.
*/
PR_EXTERN(PRInt32) PR_GetOSError(void);

/*
** Get the length of the error text. If a zero is returned, then there
** is no text. Otherwise, the value returned is sufficient to contain
** the error text currently available.
*/
PR_EXTERN(PRInt32) PR_GetErrorTextLength(void);

/*
** Copy the current threads current error text. Then actual number of bytes
** copied is returned as the result. If the result is zero, the 'text' area
** is unaffected.
*/
PR_EXTERN(PRInt32) PR_GetErrorText(char *text);


PR_END_EXTERN_C

#endif /* prerror_h___ */
