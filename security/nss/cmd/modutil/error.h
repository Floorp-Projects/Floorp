/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MODUTIL_ERROR_H
#define MODUTIL_ERROR_H

/*
 * The values of these enumerated constants are immutable and must not be
 * changed.
 */
typedef enum {
    NO_ERR = 0,
    INVALID_USAGE_ERR,
    UNEXPECTED_ARG_ERR,
    UNKNOWN_OPTION_ERR,
    MULTIPLE_COMMAND_ERR,
    OPTION_NEEDS_ARG_ERR,
    DUPLICATE_OPTION_ERR,
    MISSING_PARAM_ERR,
    INVALID_FIPS_ARG,
    NO_COMMAND_ERR,
    NO_DBDIR_ERR,
    FIPS_SWITCH_FAILED_ERR,
    FIPS_ALREADY_ON_ERR,
    FIPS_ALREADY_OFF_ERR,
    FILE_ALREADY_EXISTS_ERR,
    FILE_DOESNT_EXIST_ERR,
    FILE_NOT_READABLE_ERR,
    FILE_NOT_WRITEABLE_ERR,
    DIR_DOESNT_EXIST_ERR,
    DIR_NOT_READABLE_ERR,
    DIR_NOT_WRITEABLE_ERR,
    INVALID_CONSTANT_ERR,
    ADD_MODULE_FAILED_ERR,
    UNUSED_ERR, /* reserved for future use */
    OUT_OF_MEM_ERR,
    DELETE_INTERNAL_ERR,
    DELETE_FAILED_ERR,
    NO_LIST_LOCK_ERR,
    NO_MODULE_LIST_ERR,
    NO_SUCH_MODULE_ERR,
    MOD_INFO_ERR,
    SLOT_INFO_ERR,
    TOKEN_INFO_ERR,
    NO_SUCH_TOKEN_ERR,
    CHANGEPW_FAILED_ERR,
    BAD_PW_ERR,
    DB_ACCESS_ERR,
    AUTHENTICATION_FAILED_ERR,
    NO_SUCH_SLOT_ERR,
    ENABLE_FAILED_ERR,
    UPDATE_MOD_FAILED_ERR,
    DEFAULT_FAILED_ERR,
    UNDEFAULT_FAILED_ERR,
    STDIN_READ_ERR,
    UNSPECIFIED_ERR,
    NOCERTDB_MISUSE_ERR,
    NSS_INITIALIZE_FAILED_ERR,
    INITPW_FAILED_ERR,

    LAST_ERR /* must be last */
} Error;
#define SUCCESS NO_ERR

/* !!! Should move this into its own .c and un-static it. */
static char *errStrings[] = {
    "Operation completed successfully.\n",
    "ERROR: Invalid command line.\n",
    "ERROR: Not expecting argument \"%s\".\n",
    "ERROR: Unknown option: %s.\n",
    "ERROR: %s: multiple commands are not allowed on the command line.\n",
    "ERROR: %s: option needs an argument.\n",
    "ERROR: %s: option cannot be given more than once.\n",
    "ERROR: Command \"%s\" requires parameter \"%s\".\n",
    "ERROR: Argument to -fips must be \"true\" or \"false\".\n",
    "ERROR: No command was specified.\n",
    "ERROR: Cannot determine database directory: use the -dbdir option.\n",
    "ERROR: Unable to switch FIPS modes.\n",
    "FIPS mode already enabled.\n",
    "FIPS mode already disabled.\n",
    "ERROR: File \"%s\" already exists.\n",
    "ERROR: File \"%s\" does not exist.\n",
    "ERROR: File \"%s\" is not readable.\n",
    "ERROR: File \"%s\" is not writeable.\n",
    "ERROR: Directory \"%s\" does not exist.\n",
    "ERROR: Directory \"%s\" is not readable.\n",
    "ERROR: Directory \"%s\" is not writeable.\n",
    "\"%s\" is not a recognized value.\n",
    "ERROR: Failed to add module \"%s\". Probable cause : \"%s\".\n",
    "Unused error string",
    "ERROR: Out of memory.\n",
    "ERROR: Cannot delete internal module.\n",
    "ERROR: Failed to delete module \"%s\".\n",
    "ERROR: Unable to obtain lock on module list.\n",
    "ERROR: Unable to obtain module list.\n",
    "ERROR: Module \"%s\" not found in database.\n",
    "ERROR: Unable to get information about module \"%s\".\n",
    "ERROR: Unable to get information about slot \"%s\".\n",
    "ERROR: Unable to get information about token \"%s\".\n",
    "ERROR: Token \"%s\" not found.\n",
    "ERROR: Unable to change password on token \"%s\".\n",
    "ERROR: Incorrect password.\n",
    "ERROR: Unable to access database \"%s\".\n",
    "ERROR: Unable to authenticate to token \"%s\".\n",
    "ERROR: Slot \"%s\" not found.\n",
    "ERROR: Failed to %s slot \"%s\".\n",
    "ERROR: Failed to update module \"%s\".\n",
    "ERROR: Failed to change defaults.\n",
    "ERROR: Failed to change default.\n",
    "ERROR: Unable to read from standard input.\n",
    "ERROR: Unknown error occurred.\n",
    "ERROR: -nocertdb option can only be used with the -jar command.\n",
    "ERROR: NSS_Initialize() failed.\n",
    "ERROR: Unable to set initial password on the database.\n"
};

typedef enum {
    FIPS_ENABLED_MSG = 0,
    FIPS_DISABLED_MSG,
    USING_DBDIR_MSG,
    CREATING_DB_MSG,
    ADD_MODULE_SUCCESS_MSG,
    DELETE_SUCCESS_MSG,
    CHANGEPW_SUCCESS_MSG,
    BAD_PW_MSG,
    PW_MATCH_MSG,
    DONE_MSG,
    ENABLE_SUCCESS_MSG,
    DEFAULT_SUCCESS_MSG,
    UNDEFAULT_SUCCESS_MSG,
    BROWSER_RUNNING_MSG,
    ABORTING_MSG,
    P11_KIT_ENABLED_MSG,

    LAST_MSG /* must be last */
} Message;

/* defined in modutil.c */
extern char *msgStrings[];

#endif /* MODUTIL_ERROR_H */
