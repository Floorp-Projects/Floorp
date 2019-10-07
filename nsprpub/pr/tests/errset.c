/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***********************************************************************
**
** Name: errset.c
**
** Description: errset.c exercises the functions in prerror.c.
** This code is a unit test of the prerror.c capability.
**
** Note: There's some fluff in here. The guts of the test
** were plagerized from another test. So, sue me.
**
**
*/
#include "prerror.h"
#include "plgetopt.h"
#include "prlog.h"

#include <stdio.h>
#include <string.h>

static int _debug_on = 0;

struct errinfo {
    PRErrorCode errcode;
    char        *errname;
};

struct errinfo errcodes[] = {
    {PR_OUT_OF_MEMORY_ERROR,            "PR_OUT_OF_MEMORY_ERROR"},
    {PR_UNKNOWN_ERROR, "An intentionally long error message text intended to force a delete of the current errorString buffer and get another one."},
    {PR_BAD_DESCRIPTOR_ERROR,           "PR_BAD_DESCRIPTOR_ERROR"},
    {PR_WOULD_BLOCK_ERROR,              "PR_WOULD_BLOCK_ERROR"},
    {PR_ACCESS_FAULT_ERROR,             "PR_ACCESS_FAULT_ERROR"},
    {PR_INVALID_METHOD_ERROR,           "PR_INVALID_METHOD_ERROR"},
    {PR_ILLEGAL_ACCESS_ERROR,           "PR_ILLEGAL_ACCESS_ERROR"},
    {PR_UNKNOWN_ERROR,                  "PR_UNKNOWN_ERROR"},
    {PR_PENDING_INTERRUPT_ERROR,        "PR_PENDING_INTERRUPT_ERROR"},
    {PR_NOT_IMPLEMENTED_ERROR,          "PR_NOT_IMPLEMENTED_ERROR"},
    {PR_IO_ERROR,                       "PR_IO_ERROR"},
    {PR_IO_TIMEOUT_ERROR,               "PR_IO_TIMEOUT_ERROR"},
    {PR_IO_PENDING_ERROR,               "PR_IO_PENDING_ERROR"},
    {PR_DIRECTORY_OPEN_ERROR,           "PR_DIRECTORY_OPEN_ERROR"},
    {PR_INVALID_ARGUMENT_ERROR,         "PR_INVALID_ARGUMENT_ERROR"},
    {PR_ADDRESS_NOT_AVAILABLE_ERROR,    "PR_ADDRESS_NOT_AVAILABLE_ERROR"},
    {PR_ADDRESS_NOT_SUPPORTED_ERROR,    "PR_ADDRESS_NOT_SUPPORTED_ERROR"},
    {PR_IS_CONNECTED_ERROR,             "PR_IS_CONNECTED_ERROR"},
    {PR_BAD_ADDRESS_ERROR,              "PR_BAD_ADDRESS_ERROR"},
    {PR_ADDRESS_IN_USE_ERROR,           "PR_ADDRESS_IN_USE_ERROR"},
    {PR_CONNECT_REFUSED_ERROR,          "PR_CONNECT_REFUSED_ERROR"},
    {PR_NETWORK_UNREACHABLE_ERROR,      "PR_NETWORK_UNREACHABLE_ERROR"},
    {PR_CONNECT_TIMEOUT_ERROR,          "PR_CONNECT_TIMEOUT_ERROR"},
    {PR_NOT_CONNECTED_ERROR,            "PR_NOT_CONNECTED_ERROR"},
    {PR_LOAD_LIBRARY_ERROR,             "PR_LOAD_LIBRARY_ERROR"},
    {PR_UNLOAD_LIBRARY_ERROR,           "PR_UNLOAD_LIBRARY_ERROR"},
    {PR_FIND_SYMBOL_ERROR,              "PR_FIND_SYMBOL_ERROR"},
    {PR_INSUFFICIENT_RESOURCES_ERROR,   "PR_INSUFFICIENT_RESOURCES_ERROR"},
    {PR_DIRECTORY_LOOKUP_ERROR,         "PR_DIRECTORY_LOOKUP_ERROR"},
    {PR_TPD_RANGE_ERROR,                "PR_TPD_RANGE_ERROR"},
    {PR_PROC_DESC_TABLE_FULL_ERROR,     "PR_PROC_DESC_TABLE_FULL_ERROR"},
    {PR_SYS_DESC_TABLE_FULL_ERROR,      "PR_SYS_DESC_TABLE_FULL_ERROR"},
    {PR_NOT_SOCKET_ERROR,               "PR_NOT_SOCKET_ERROR"},
    {PR_NOT_TCP_SOCKET_ERROR,           "PR_NOT_TCP_SOCKET_ERROR"},
    {PR_SOCKET_ADDRESS_IS_BOUND_ERROR,  "PR_SOCKET_ADDRESS_IS_BOUND_ERROR"},
    {PR_NO_ACCESS_RIGHTS_ERROR,         "PR_NO_ACCESS_RIGHTS_ERROR"},
    {PR_OPERATION_NOT_SUPPORTED_ERROR,  "PR_OPERATION_NOT_SUPPORTED_ERROR"},
    {PR_PROTOCOL_NOT_SUPPORTED_ERROR,   "PR_PROTOCOL_NOT_SUPPORTED_ERROR"},
    {PR_REMOTE_FILE_ERROR,              "PR_REMOTE_FILE_ERROR"},
    {PR_BUFFER_OVERFLOW_ERROR,          "PR_BUFFER_OVERFLOW_ERROR"},
    {PR_CONNECT_RESET_ERROR,            "PR_CONNECT_RESET_ERROR"},
    {PR_RANGE_ERROR,                    "PR_RANGE_ERROR"},
    {PR_DEADLOCK_ERROR,                 "PR_DEADLOCK_ERROR"},
    {PR_FILE_IS_LOCKED_ERROR,           "PR_FILE_IS_LOCKED_ERROR"},
    {PR_FILE_TOO_BIG_ERROR,             "PR_FILE_TOO_BIG_ERROR"},
    {PR_NO_DEVICE_SPACE_ERROR,          "PR_NO_DEVICE_SPACE_ERROR"},
    {PR_PIPE_ERROR,                     "PR_PIPE_ERROR"},
    {PR_NO_SEEK_DEVICE_ERROR,           "PR_NO_SEEK_DEVICE_ERROR"},
    {PR_IS_DIRECTORY_ERROR,             "PR_IS_DIRECTORY_ERROR"},
    {PR_LOOP_ERROR,                     "PR_LOOP_ERROR"},
    {PR_NAME_TOO_LONG_ERROR,            "PR_NAME_TOO_LONG_ERROR"},
    {PR_FILE_NOT_FOUND_ERROR,           "PR_FILE_NOT_FOUND_ERROR"},
    {PR_NOT_DIRECTORY_ERROR,            "PR_NOT_DIRECTORY_ERROR"},
    {PR_READ_ONLY_FILESYSTEM_ERROR,     "PR_READ_ONLY_FILESYSTEM_ERROR"},
    {PR_DIRECTORY_NOT_EMPTY_ERROR,      "PR_DIRECTORY_NOT_EMPTY_ERROR"},
    {PR_FILESYSTEM_MOUNTED_ERROR,       "PR_FILESYSTEM_MOUNTED_ERROR"},
    {PR_NOT_SAME_DEVICE_ERROR,          "PR_NOT_SAME_DEVICE_ERROR"},
    {PR_DIRECTORY_CORRUPTED_ERROR,      "PR_DIRECTORY_CORRUPTED_ERROR"},
    {PR_FILE_EXISTS_ERROR,              "PR_FILE_EXISTS_ERROR"},
    {PR_MAX_DIRECTORY_ENTRIES_ERROR,    "PR_MAX_DIRECTORY_ENTRIES_ERROR"},
    {PR_INVALID_DEVICE_STATE_ERROR,     "PR_INVALID_DEVICE_STATE_ERROR"},
    {PR_DEVICE_IS_LOCKED_ERROR,         "PR_DEVICE_IS_LOCKED_ERROR"},
    {PR_NO_MORE_FILES_ERROR,            "PR_NO_MORE_FILES_ERROR"},
    {PR_END_OF_FILE_ERROR,              "PR_END_OF_FILE_ERROR"},
    {PR_FILE_SEEK_ERROR,                "PR_FILE_SEEK_ERROR"},
    {PR_FILE_IS_BUSY_ERROR,             "PR_FILE_IS_BUSY_ERROR"},
    {PR_IN_PROGRESS_ERROR,              "PR_IN_PROGRESS_ERROR"},
    {PR_ALREADY_INITIATED_ERROR,        "PR_ALREADY_INITIATED_ERROR"},
    {PR_GROUP_EMPTY_ERROR,              "PR_GROUP_EMPTY_ERROR"},
    {PR_INVALID_STATE_ERROR,            "PR_INVALID_STATE_ERROR"},
    {PR_NETWORK_DOWN_ERROR,             "PR_NETWORK_DOWN_ERROR"},
    {PR_SOCKET_SHUTDOWN_ERROR,          "PR_SOCKET_SHUTDOWN_ERROR"},
    {PR_CONNECT_ABORTED_ERROR,          "PR_CONNECT_ABORTED_ERROR"},
    {PR_HOST_UNREACHABLE_ERROR,         "PR_HOST_UNREACHABLE_ERROR"}
};

int main(int argc, char **argv)
{

    int count, errnum;

    /*
     * -d           debug mode
     */

    PLOptStatus os;
    PLOptState *opt = PL_CreateOptState(argc, argv, "d");
    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) {
            continue;
        }
        switch (opt->option)
        {
            case 'd':  /* debug mode */
                _debug_on = 1;
                break;
            default:
                break;
        }
    }
    PL_DestroyOptState(opt);

    count = sizeof(errcodes)/sizeof(errcodes[0]);
    printf("\nNumber of error codes = %d\n\n",count);
    for (errnum = 0; errnum < count; errnum++) {
        PRInt32 len1, len2, err;
        char    msg[256];

        PR_SetError( errnum, -5 );
        err = PR_GetError();
        PR_ASSERT( err == errnum );
        err = PR_GetOSError();
        PR_ASSERT( err == -5 );
        PR_SetErrorText( strlen(errcodes[errnum].errname), errcodes[errnum].errname );
        len1 = PR_GetErrorTextLength();
        len2 = PR_GetErrorText( msg );
        PR_ASSERT( len1 == len2 );
        printf("%5.5d -- %s\n", errnum, msg );
    }

    return 0;
}
