/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Errors_h__
#define Errors_h__

#define OK 0

// Old unused error codes:
// #define MEM_ERROR 1  // Replaced with errors 10-16 (inclusive)
// #define IO_ERROR 2  // Use READ_ERROR or WRITE_ERROR instead

// Error codes 3-16 are for general update problems.
#define USAGE_ERROR 3
#define CRC_ERROR 4
#define PARSE_ERROR 5
#define READ_ERROR 6
#define WRITE_ERROR 7
// #define UNEXPECTED_ERROR 8 // Replaced with errors 38-42
#define ELEVATION_CANCELED 9
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

// Error codes 24-34 are related to the maintenance service
// and so are Windows only
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
#define SERVICE_COULD_NOT_COPY_UPDATER 49

#define NO_INSTALLDIR_ERROR 34
#define WRITE_ERROR_ACCESS_DENIED 35
// #define WRITE_ERROR_SHARING_VIOLATION 36 // Replaced with errors 46-48
#define WRITE_ERROR_CALLBACK_APP 37
#define INVALID_UPDATER_STATUS_CODE 38
#define UNEXPECTED_BZIP_ERROR 39
#define UNEXPECTED_MAR_ERROR 40
#define UNEXPECTED_BSPATCH_ERROR 41
#define UNEXPECTED_FILE_OPERATION_ERROR 42
#define FILESYSTEM_MOUNT_READWRITE_ERROR 43
#define FOTA_GENERAL_ERROR 44
#define FOTA_UNKNOWN_ERROR 45
#define WRITE_ERROR_SHARING_VIOLATION_SIGNALED 46
#define WRITE_ERROR_SHARING_VIOLATION_NOPROCESSFORPID 47
#define WRITE_ERROR_SHARING_VIOLATION_NOPID 48

// The following error codes are only used by updater.exe
// when a fallback key exists and XPCShell tests are being run.
#define FALLBACKKEY_UNKNOWN_ERROR 100
#define FALLBACKKEY_REGPATH_ERROR 101
#define FALLBACKKEY_NOKEY_ERROR 102
#define FALLBACKKEY_SERVICE_NO_STOP_ERROR 103
#define FALLBACKKEY_LAUNCH_ERROR 104

#endif  // Errors_h__
