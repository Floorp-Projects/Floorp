/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsSoftUpdateEnums_h__
#define nsSoftUpdateEnums_h__


  /* SoftwareUpdate Errors -200 to -300 */
#define SUERR_BAD_PACKAGE_NAME          -200
#define SUERR_UNEXPECTED_ERROR          -201
#define SUERR_ACCESS_DENIED             -202
#define SUERR_TOO_MANY_CERTIFICATES     -203 /* Installer file must have 1 certificate */
#define SUERR_NO_INSTALLER_CERTIFICATE  -204 /* Installer file must have a certificate */
#define SUERR_NO_CERTIFICATE            -205 /* Extracted file is not signed */
#define SUERR_NO_MATCHING_CERTIFICATE   -206 /* Extracted file does not match installer certificate */
#define SUERR_UNKNOWN_JAR_FILE          -207 /* JAR file has not been opened */
#define SUERR_INVALID_ARGUMENTS         -208 /* Bad arguments to a function */
#define SUERR_ILLEGAL_RELATIVE_PATH     -209 /* Illegal relative path */
#define SUERR_USER_CANCELLED            -210 /* User cancelled */
#define SUERR_INSTALL_NOT_STARTED       -211
#define SUERR_SILENT_MODE_DENIED        -212
#define SUERR_NO_SUCH_COMPONENT         -213 /* no such component in the registry. */
#define SUERR_FILE_DOES_NOT_EXIST       -214 /* File cannot be deleted as it does not exist */
#define SUERR_FILE_READ_ONLY            -215 /* File cannot be deleted as it is read only. */
#define SUERR_FILE_IS_DIRECTORY         -216 /* File cannot be deleted as it is a directory */
#define SUERR_NETWORK_FILE_IS_IN_USE    -217 /* File on the network is in-use */
#define SUERR_APPLE_SINGLE_ERR          -218 /* error in AppleSingle unpacking */
#define SUERR_INVALID_PATH_ERR          -219 /* GetFolder() did not like the folderID */
#define SUERR_PATCH_BAD_DIFF            -220 /* error in GDIFF patch */
#define SUERR_PATCH_BAD_CHECKSUM_TARGET -221 /* source file doesn't checksum  */
#define SUERR_PATCH_BAD_CHECKSUM_RESULT -222 /* final patched file fails checksum  */
#define SUERR_UNINSTALL_FAILED          -223 /* error while uninstalling a package  */
#define SUERR_GESTALT_UNKNOWN_ERR       -5550         
#define SUERR_GESTALT_INVALID_ARGUMENT  -5551

#define SU_SUCCESS 0
#define SU_REBOOT_NEEDED 999

/* install types */
#define SU_LIMITED_INSTALL  0
#define SU_FULL_INSTALL     1
#define SU_NO_STATUS_DLG    2
#define SU_NO_FINALIZE_DLG  4


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
