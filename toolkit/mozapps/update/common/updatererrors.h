/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef UPDATEERRORS_H
#define UPDATEERRORS_H

#define OK 0

// Error codes that are no longer used should not be used again unless they
// aren't used in client code (e.g. UpdateService.jsm, updates.js, etc.).

#define MAR_ERROR_EMPTY_ACTION_LIST 1
#define LOADSOURCE_ERROR_WRONG_SIZE 2

// Error codes 3-16 are for general update problems.
#define USAGE_ERROR 3
#define CRC_ERROR 4
#define PARSE_ERROR 5
#define READ_ERROR 6
#define WRITE_ERROR 7
// #define UNEXPECTED_ERROR 8 // Replaced with errors 38-42
#define ELEVATION_CANCELED 9

// Error codes 10-14 are related to memory allocation failures.
// Note: If more memory allocation error codes are added, the implementation of
// isMemoryAllocationErrorCode in UpdateService.jsm should be updated to account
// for them.
#define READ_STRINGS_MEM_ERROR 10
#define ARCHIVE_READER_MEM_ERROR 11
#define BSPATCH_MEM_ERROR 12
#define UPDATER_MEM_ERROR 13
#define UPDATER_QUOTED_PATH_MEM_ERROR 14

#define BAD_ACTION_ERROR 15
#define STRING_CONVERSION_ERROR 16

// Error codes 17-23 are related to security tasks for MAR
// signing and MAR protection.
#define CERT_LOAD_ERROR 17
#define CERT_HANDLING_ERROR 18
#define CERT_VERIFY_ERROR 19
#define ARCHIVE_NOT_OPEN 20
#define COULD_NOT_READ_PRODUCT_INFO_BLOCK_ERROR 21
#define MAR_CHANNEL_MISMATCH_ERROR 22
#define VERSION_DOWNGRADE_ERROR 23

// Error codes 24-33 and 49-58 are for the Windows maintenance service.
// Note: If more maintenance service error codes are added, the implementations
// of IsServiceSpecificErrorCode in updater.cpp and UpdateService.jsm should be
// updated to account for them.
#define SERVICE_UPDATER_COULD_NOT_BE_STARTED 24
#define SERVICE_NOT_ENOUGH_COMMAND_LINE_ARGS 25
#define SERVICE_UPDATER_SIGN_ERROR 26
#define SERVICE_UPDATER_COMPARE_ERROR 27
#define SERVICE_UPDATER_IDENTITY_ERROR 28
#define SERVICE_STILL_APPLYING_ON_SUCCESS 29
#define SERVICE_STILL_APPLYING_ON_FAILURE 30
#define SERVICE_UPDATER_NOT_FIXED_DRIVE 31
#define SERVICE_COULD_NOT_LOCK_UPDATER 32
#define SERVICE_INSTALLDIR_ERROR 33

#define NO_INSTALLDIR_ERROR 34
#define WRITE_ERROR_ACCESS_DENIED 35
// #define WRITE_ERROR_SHARING_VIOLATION 36 // Replaced with errors 46-48
#define WRITE_ERROR_CALLBACK_APP 37
#define UPDATE_SETTINGS_FILE_CHANNEL 38
#define UNEXPECTED_XZ_ERROR 39
#define UNEXPECTED_MAR_ERROR 40
#define UNEXPECTED_BSPATCH_ERROR 41
#define UNEXPECTED_FILE_OPERATION_ERROR 42
#define UNEXPECTED_STAGING_ERROR 43
#define DELETE_ERROR_STAGING_LOCK_FILE 44
#define DELETE_ERROR_EXPECTED_DIR 46
#define DELETE_ERROR_EXPECTED_FILE 47
#define RENAME_ERROR_EXPECTED_FILE 48

// Error codes 24-33 and 49-58 are for the Windows maintenance service.
// Note: If more maintenance service error codes are added, the implementations
// of IsServiceSpecificErrorCode in updater.cpp and UpdateService.jsm should be
// updated to account for them.
#define SERVICE_COULD_NOT_COPY_UPDATER 49
#define SERVICE_STILL_APPLYING_TERMINATED 50
#define SERVICE_STILL_APPLYING_NO_EXIT_CODE 51
#define SERVICE_INVALID_APPLYTO_DIR_STAGED_ERROR 52
#define SERVICE_CALC_REG_PATH_ERROR 53
#define SERVICE_INVALID_APPLYTO_DIR_ERROR 54
#define SERVICE_INVALID_INSTALL_DIR_PATH_ERROR 55
#define SERVICE_INVALID_WORKING_DIR_PATH_ERROR 56
#define SERVICE_INSTALL_DIR_REG_ERROR 57
#define SERVICE_UPDATE_STATUS_UNCHANGED 58

#define WRITE_ERROR_FILE_COPY 61
#define WRITE_ERROR_DELETE_FILE 62
#define WRITE_ERROR_OPEN_PATCH_FILE 63
#define WRITE_ERROR_PATCH_FILE 64
#define WRITE_ERROR_APPLY_DIR_PATH 65
#define WRITE_ERROR_CALLBACK_PATH 66
#define WRITE_ERROR_FILE_ACCESS_DENIED 67
#define WRITE_ERROR_DIR_ACCESS_DENIED 68
#define WRITE_ERROR_DELETE_BACKUP 69
#define WRITE_ERROR_EXTRACT 70
#define REMOVE_FILE_SPEC_ERROR 71
#define INVALID_APPLYTO_DIR_STAGED_ERROR 72
#define LOCK_ERROR_PATCH_FILE 73
#define INVALID_APPLYTO_DIR_ERROR 74
#define INVALID_INSTALL_DIR_PATH_ERROR 75
#define INVALID_WORKING_DIR_PATH_ERROR 76
#define INVALID_CALLBACK_PATH_ERROR 77
#define INVALID_CALLBACK_DIR_ERROR 78
#define UPDATE_STATUS_UNCHANGED 79

// Error codes 80 through 99 are reserved for UpdateService.jsm

// The following error codes are only used by updater.exe
// when a fallback key exists for tests.
#define FALLBACKKEY_UNKNOWN_ERROR 100
#define FALLBACKKEY_REGPATH_ERROR 101
#define FALLBACKKEY_NOKEY_ERROR 102
#define FALLBACKKEY_SERVICE_NO_STOP_ERROR 103
#define FALLBACKKEY_LAUNCH_ERROR 104

#define SILENT_UPDATE_NEEDED_ELEVATION_ERROR 105
#define WRITE_ERROR_BACKGROUND_TASK_SHARING_VIOLATION 106

// Error codes 110 and 111 are reserved for UpdateService.jsm

#endif  // UPDATEERRORS_H
