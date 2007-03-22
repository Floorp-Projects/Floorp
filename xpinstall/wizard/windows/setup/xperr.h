/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sean Su <ssu@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef _XPERR_H_
#define _XPERR_H_

char *XpErrorList[] = {"0"   ,  "OK",
                       "-200",  "BAD_PACKAGE_NAME",
                       "-201",  "UNEXPECTED_ERROR",
                       "-202",  "ACCESS_DENIED",
                       "-203",  "EXECUTION_ERROR",
                       "-204",  "NO_INSTALL_SCRIPT",
                       "-205",  "NO_CERTIFICATE",
                       "-206",  "NO_MATCHING_CERTIFICATE",
                       "-207",  "CANT_READ_ARCHIVE",
                       "-208",  "INVALID_ARGUMENTS",
                       "-209",  "ILLEGAL_RELATIVE_PATH",
                       "-210",  "USER_CANCELLED",
                       "-211",  "INSTALL_NOT_STARTED",
                       "-212",  "SILENT_MODE_DENIED",
                       "-213",  "NO_SUCH_COMPONENT",
                       "-214",  "DOES_NOT_EXIST",
                       "-215",  "READ_ONLY",
                       "-216",  "IS_DIRECTORY",
                       "-217",  "NETWORK_FILE_IS_IN_USE",
                       "-218",  "APPLE_SINGLE_ERR",
                       "-219",  "INVALID_PATH_ERR",
                       "-220",  "PATCH_BAD_DIFF",
                       "-221",  "PATCH_BAD_CHECKSUM_TARGET",
                       "-222",  "PATCH_BAD_CHECKSUM_RESULT",
                       "-223",  "UNINSTALL_FAILED",
                       "-224",  "PACKAGE_FOLDER_NOT_SET",
                       "-225",  "EXTRACTION_FAILED",
                       "-226",  "FILENAME_ALREADY_USED",
                       "-227",  "ABORT_INSTALL",
                       "-228",  "DOWNLOAD_ERROR",
                       "-229",  "SCRIPT_ERROR",
                       "-230",  "ALREADY_EXISTS",
                       "-231",  "IS_FILE",
                       "-232",  "SOURCE_DOES_NOT_EXIST",
                       "-233",  "SOURCE_IS_DIRECTORY",
                       "-234",  "SOURCE_IS_FILE",
                       "-235",  "INSUFFICIENT_DISK_SPACE",
                       "-236",  "FILENAME_TOO_LONG",
                       "-237",  "UNABLE_TO_LOCATE_LIB_FUNCTION",
                       "-238",  "UNABLE_TO_LOAD_LIBRARY",
                       "-239",  "CHROME_REGISTRY_ERROR",
                       "-240",  "MALFORMED_INSTALL",
                       "-241",  "KEY_ACCESS_DENIED",
                       "-242",  "KEY_DOES_NOT_EXIST",
                       "-243",  "VALUE_DOES_NOT_EXIST",
                       "-299",  "OUT_OF_MEMORY",
                       "-322",  "INIT_STUB_ERROR",
                       "-5550", "GESTALT_UNKNOWN_ERR",
                       "-5551", "GESTALT_INVALID_ARGUMENT",
                       ""};

#endif /* _XPERR_H_ */

