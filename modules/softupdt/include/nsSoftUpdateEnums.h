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

#ifndef nsSoftUpdateEnums_h__
#define nsSoftUpdateEnums_h__


typedef enum nsSoftUpdateError {

  /* Errors -200 to -300 */
  nsSoftUpdateError_BAD_PACKAGE_NAME = -200,
  nsSoftUpdateError_UNEXPECTED_ERROR = -201,
  nsSoftUpdateError_ACCESS_DENIED = -202,
  nsSoftUpdateError_TOO_MANY_CERTIFICATES = -203,    /* Installer file must have 1 certificate */
  nsSoftUpdateError_NO_INSTALLER_CERTIFICATE = -204, /* Installer file must have a certificate */
  nsSoftUpdateError_NO_CERTIFICATE =  -205,          /* Extracted file is not signed */
  nsSoftUpdateError_NO_MATCHING_CERTIFICATE = -206,  /* Extracted file does not match installer certificate */
  nsSoftUpdateError_UNKNOWN_JAR_FILE = -207,         /* JAR file has not been opened */
  nsSoftUpdateError_INVALID_ARGUMENTS = -208,        /* Bad arguments to a function */
  nsSoftUpdateError_ILLEGAL_RELATIVE_PATH = -209,    /* Illegal relative path */
  nsSoftUpdateError_USER_CANCELLED = -210,           /* User cancelled */
  nsSoftUpdateError_INSTALL_NOT_STARTED = -211,
  nsSoftUpdateError_SILENT_MODE_DENIED = -212,
  nsSoftUpdateError_NO_SUCH_COMPONENT = -213,        /* no such component in the registry. */
  nsSoftUpdateError_FILE_DOES_NOT_EXIST = -214,      /* File cannot be deleted as it does not exist */
  nsSoftUpdateError_FILE_READ_ONLY = -215,	       /* File cannot be deleted as it is read only. */
  nsSoftUpdateError_FILE_IS_DIRECTORY = -216,	       /* File cannot be deleted as it is a directory */
  nsSoftUpdateError_NETWORK_FILE_IS_IN_USE = -217,	/* File on the network is in-use */
  nsSoftUpdateError_APPLE_SINGLE_ERR = -218,         /* error in AppleSingle unpacking */
  
  nsSoftUpdateError_INVALID_PATH_ERR = -219,  /* GetFolder() did not like the folderID */
  nsSoftUpdateError_PATCH_BAD_DIFF = -220,   /* error in GDIFF patch */
  nsSoftUpdateError_PATCH_BAD_CHECKSUM_TARGET = -221,  /* source file doesn't checksum  */
  nsSoftUpdateError_PATCH_BAD_CHECKSUM_RESULT = -222,  /* final patched file fails checksum  */
  nsSoftUpdateError_UNINSTALL_FAILED = -223,  /* error while uninstalling a package  */
  
  nsSoftUpdateError_GESTALT_UNKNOWN_ERR = -5550,         
  nsSoftUpdateError_GESTALT_INVALID_ARGUMENT = -5551,
} nsSoftUpdateError;

#define nsSoftwareUpdate_SUCCESS 0
#define nsSoftwareUpdate_REBOOT_NEEDED 999

typedef enum nsInstallType {
  nsInstallType_LIMITED_INSTALL= 0,
  nsInstallType_FULL_INSTALL = 1,
  nsInstallType_NO_STATUS_DLG = 2,
  nsInstallType_NO_FINALIZE_DLG = 4
} nsInstallType;

typedef enum nsVersionEnum {
  nsVersionEnum_MAJOR_DIFF       =  4,
  nsVersionEnum_MAJOR_DIFF_MINUS = -4,
  nsVersionEnum_MINOR_DIFF       =  3,
  nsVersionEnum_MINOR_DIFF_MINUS = -3,
  nsVersionEnum_REL_DIFF         =  2,
  nsVersionEnum_REL_DIFF_MINUS   = -2,
  nsVersionEnum_BLD_DIFF         =  1,
  nsVersionEnum_BLD_DIFF_MINUS   = -1,
  nsVersionEnum_EQUAL      =  0
} nsVersionEnum;

typedef enum nsInstallDeleteEnum {
  DELETE_FILE = 1,
  DELETE_COMPONENT=2
} nsInstallDeleteEnum;

/**
 * diff levels are borrowed from the VersionInfo class
 */
typedef enum nsTriggerDiffLevelEnum {
  MAJOR_DIFF =  4,
  MINOR_DIFF =  3,
  REL_DIFF   =  2,
  BLD_DIFF   =  1,
  EQUAL      =  0
} nsTriggerDiffLevelEnum;

typedef enum nsWinRegEnum {
  NS_WIN_REG_CREATE          = 1,
  NS_WIN_REG_DELETE          = 2,
  NS_WIN_REG_DELETE_VAL      = 3,
  NS_WIN_REG_SET_VAL_STRING  = 4,
  NS_WIN_REG_SET_VAL         = 5

} nsWinRegEnum;


typedef enum nsWinRegValueEnum {
  NS_WIN_REG_SZ                          = 1,
  NS_WIN_REG_EXPAND_SZ                   = 2,
  NS_WIN_REG_BINARY                      = 3,
  NS_WIN_REG_DWORD                       = 4,
  NS_WIN_REG_DWORD_LITTLE_ENDIAN         = 4,
  NS_WIN_REG_DWORD_BIG_ENDIAN            = 5,
  NS_WIN_REG_LINK                        = 6,
  NS_WIN_REG_MULTI_SZ                    = 7,
  NS_WIN_REG_RESOURCE_LIST               = 8,
  NS_WIN_REG_FULL_RESOURCE_DESCRIPTOR    = 9,
  NS_WIN_REG_RESOURCE_REQUIREMENTS_LIST  = 10
} nsWinRegValueEnum;

typedef enum nsRegistryErrorsEnum {
  REGERR_SECURITY      = 99,
} nsRegistryErrorsEnum;

#endif /* nsSoftUpdateEnums_h__ */
