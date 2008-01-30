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
 *   Daniel Veditz <dveditz@netscape.com>
 *   Douglas Turner <dougt@netscape.com>
 *   Jens Bannmann <jens.b@web.de>
 *   Dave Townsend <dtownsend@oxymoronical.com>
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

#ifndef __NS_INSTALL_H__
#define __NS_INSTALL_H__

class nsInstall
{
    public:

        enum
        {
            BAD_PACKAGE_NAME            = -200,
            UNEXPECTED_ERROR            = -201,
            ACCESS_DENIED               = -202,
            EXECUTION_ERROR             = -203,
            NO_INSTALL_SCRIPT           = -204,
            NO_CERTIFICATE              = -205,
            NO_MATCHING_CERTIFICATE     = -206,
            CANT_READ_ARCHIVE           = -207,
            INVALID_ARGUMENTS           = -208,
            ILLEGAL_RELATIVE_PATH       = -209,
            USER_CANCELLED              = -210,
            INSTALL_NOT_STARTED         = -211,
            SILENT_MODE_DENIED          = -212,
            NO_SUCH_COMPONENT           = -213,
            DOES_NOT_EXIST              = -214,
            READ_ONLY                   = -215,
            IS_DIRECTORY                = -216,
            NETWORK_FILE_IS_IN_USE      = -217,
            APPLE_SINGLE_ERR            = -218,
            INVALID_PATH_ERR            = -219,
            PATCH_BAD_DIFF              = -220,
            PATCH_BAD_CHECKSUM_TARGET   = -221,
            PATCH_BAD_CHECKSUM_RESULT   = -222,
            UNINSTALL_FAILED            = -223,
            PACKAGE_FOLDER_NOT_SET      = -224,
            EXTRACTION_FAILED           = -225,
            FILENAME_ALREADY_USED       = -226,
            INSTALL_CANCELLED           = -227,
            DOWNLOAD_ERROR              = -228,
            SCRIPT_ERROR                = -229,

            ALREADY_EXISTS              = -230,
            IS_FILE                     = -231,
            SOURCE_DOES_NOT_EXIST       = -232,
            SOURCE_IS_DIRECTORY         = -233,
            SOURCE_IS_FILE              = -234,
            INSUFFICIENT_DISK_SPACE     = -235,
            FILENAME_TOO_LONG           = -236,

            UNABLE_TO_LOCATE_LIB_FUNCTION = -237,
            UNABLE_TO_LOAD_LIBRARY        = -238,

            CHROME_REGISTRY_ERROR       = -239,

            MALFORMED_INSTALL           = -240,

            KEY_ACCESS_DENIED           = -241,
            KEY_DOES_NOT_EXIST          = -242,
            VALUE_DOES_NOT_EXIST        = -243,

            UNSUPPORTED_TYPE            = -244,

            INVALID_SIGNATURE           = -260,
            INVALID_HASH                = -261,
            INVALID_HASH_TYPE           = -262,

            OUT_OF_MEMORY               = -299,

            GESTALT_UNKNOWN_ERR         = -5550,
            GESTALT_INVALID_ARGUMENT    = -5551,

            SUCCESS                     = 0,
            REBOOT_NEEDED               = 999
        };
};

#endif
