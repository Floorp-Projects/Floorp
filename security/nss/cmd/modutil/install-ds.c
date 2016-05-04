/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "install-ds.h"
#include <prmem.h>
#include <plstr.h>
#include <prprf.h>
#include <string.h>

#define PORT_Strcasecmp PL_strcasecmp

#define MODULE_FILE_STRING "ModuleFile"
#define MODULE_NAME_STRING "ModuleName"
#define MECH_FLAGS_STRING "DefaultMechanismFlags"
#define CIPHER_FLAGS_STRING "DefaultCipherFlags"
#define FILES_STRING "Files"
#define FORWARD_COMPATIBLE_STRING "ForwardCompatible"
#define PLATFORMS_STRING "Platforms"
#define RELATIVE_DIR_STRING "RelativePath"
#define ABSOLUTE_DIR_STRING "AbsolutePath"
#define FILE_PERMISSIONS_STRING "FilePermissions"
#define EQUIVALENT_PLATFORM_STRING "EquivalentPlatform"
#define EXECUTABLE_STRING "Executable"

#define DEFAULT_PERMISSIONS 0777

#define PLATFORM_SEPARATOR_CHAR ':'

/* Error codes */
enum {
    BOGUS_RELATIVE_DIR = 0,
    BOGUS_ABSOLUTE_DIR,
    BOGUS_FILE_PERMISSIONS,
    NO_RELATIVE_DIR,
    NO_ABSOLUTE_DIR,
    EMPTY_PLATFORM_STRING,
    BOGUS_PLATFORM_STRING,
    REPEAT_MODULE_FILE,
    REPEAT_MODULE_NAME,
    BOGUS_MODULE_FILE,
    BOGUS_MODULE_NAME,
    REPEAT_MECH,
    BOGUS_MECH_FLAGS,
    REPEAT_CIPHER,
    BOGUS_CIPHER_FLAGS,
    REPEAT_FILES,
    REPEAT_EQUIV,
    BOGUS_EQUIV,
    EQUIV_TOO_MUCH_INFO,
    NO_FILES,
    NO_MODULE_FILE,
    NO_MODULE_NAME,
    NO_PLATFORMS,
    EQUIV_LOOP,
    UNKNOWN_MODULE_FILE
};

/* Indexed by the above error codes */
static const char* errString[] = {
    "%s: Invalid relative directory",
    "%s: Invalid absolute directory",
    "%s: Invalid file permissions",
    "%s: No relative directory specified",
    "%s: No absolute directory specified",
    "Empty string given for platform name",
    "%s: invalid platform string",
    "More than one ModuleFile entry given for platform %s",
    "More than one ModuleName entry given for platform %s",
    "Invalid ModuleFile specification for platform %s",
    "Invalid ModuleName specification for platform %s",
    "More than one DefaultMechanismFlags entry given for platform %s",
    "Invalid DefaultMechanismFlags specification for platform %s",
    "More than one DefaultCipherFlags entry given for platform %s",
    "Invalid DefaultCipherFlags entry given for platform %s",
    "More than one Files entry given for platform %s",
    "More than one EquivalentPlatform entry given for platform %s",
    "Invalid EquivalentPlatform specification for platform %s",
    "Module %s uses an EquivalentPlatform but also specifies its own"
    " information",
    "No Files specification in module %s",
    "No ModuleFile specification in module %s",
    "No ModuleName specification in module %s",
    "No Platforms specification in installer script",
    "Platform %s has an equivalency loop",
    "Module file \"%s\" in platform \"%s\" does not exist"
};

static char* PR_Strdup(const char* str);

#define PAD(x)                  \
    {                           \
        int i;                  \
        for (i = 0; i < x; i++) \
            printf(" ");        \
    }
#define PADINC 4

Pk11Install_File*
Pk11Install_File_new()
{
    Pk11Install_File* new_this;
    new_this = (Pk11Install_File*)PR_Malloc(sizeof(Pk11Install_File));
    Pk11Install_File_init(new_this);
    return new_this;
}

void
Pk11Install_File_init(Pk11Install_File* _this)
{
    _this->jarPath = NULL;
    _this->relativePath = NULL;
    _this->absolutePath = NULL;
    _this->executable = PR_FALSE;
    _this->permissions = 0;
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:  ~Pk11Install_File
// Class:   Pk11Install_File
// Notes:   Destructor.
*/
void
Pk11Install_File_delete(Pk11Install_File* _this)
{
    Pk11Install_File_Cleanup(_this);
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:  Cleanup
// Class:   Pk11Install_File
*/
void
Pk11Install_File_Cleanup(Pk11Install_File* _this)
{
    if (_this->jarPath) {
        PR_Free(_this->jarPath);
        _this->jarPath = NULL;
    }
    if (_this->relativePath) {
        PR_Free(_this->relativePath);
        _this->relativePath = NULL;
    }
    if (_this->absolutePath) {
        PR_Free(_this->absolutePath);
        _this->absolutePath = NULL;
    }

    _this->permissions = 0;
    _this->executable = PR_FALSE;
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:  Generate
// Class:   Pk11Install_File
// Notes:   Creates a file data structure from a syntax tree.
// Returns: NULL for success, otherwise an error message.
*/
char*
Pk11Install_File_Generate(Pk11Install_File* _this,
                          const Pk11Install_Pair* pair)
{
    Pk11Install_ListIter* iter;
    Pk11Install_Value* val;
    Pk11Install_Pair* subpair;
    Pk11Install_ListIter* subiter;
    Pk11Install_Value* subval;
    char* errStr;
    char* endp;
    PRBool gotPerms;

    iter = NULL;
    subiter = NULL;
    errStr = NULL;
    gotPerms = PR_FALSE;

    /* Clear out old values */
    Pk11Install_File_Cleanup(_this);

    _this->jarPath = PR_Strdup(pair->key);

    /* Go through all the pairs under this file heading */
    iter = Pk11Install_ListIter_new(pair->list);
    for (; (val = iter->current); Pk11Install_ListIter_nextItem(iter)) {
        if (val->type == PAIR_VALUE) {
            subpair = val->pair;

            /* Relative directory */
            if (!PORT_Strcasecmp(subpair->key, RELATIVE_DIR_STRING)) {
                subiter = Pk11Install_ListIter_new(subpair->list);
                subval = subiter->current;
                if (!subval || (subval->type != STRING_VALUE)) {
                    errStr = PR_smprintf(errString[BOGUS_RELATIVE_DIR],
                                         _this->jarPath);
                    goto loser;
                }
                _this->relativePath = PR_Strdup(subval->string);
                Pk11Install_ListIter_delete(&subiter);

                /* Absolute directory */
            } else if (!PORT_Strcasecmp(subpair->key, ABSOLUTE_DIR_STRING)) {
                subiter = Pk11Install_ListIter_new(subpair->list);
                subval = subiter->current;
                if (!subval || (subval->type != STRING_VALUE)) {
                    errStr = PR_smprintf(errString[BOGUS_ABSOLUTE_DIR],
                                         _this->jarPath);
                    goto loser;
                }
                _this->absolutePath = PR_Strdup(subval->string);
                Pk11Install_ListIter_delete(&subiter);

                /* file permissions */
            } else if (!PORT_Strcasecmp(subpair->key,
                                        FILE_PERMISSIONS_STRING)) {
                subiter = Pk11Install_ListIter_new(subpair->list);
                subval = subiter->current;
                if (!subval || (subval->type != STRING_VALUE) ||
                    !subval->string || !subval->string[0]) {
                    errStr = PR_smprintf(errString[BOGUS_FILE_PERMISSIONS],
                                         _this->jarPath);
                    goto loser;
                }
                _this->permissions = (int)strtol(subval->string, &endp, 8);
                if (*endp != '\0') {
                    errStr = PR_smprintf(errString[BOGUS_FILE_PERMISSIONS],
                                         _this->jarPath);
                    goto loser;
                }
                gotPerms = PR_TRUE;
                Pk11Install_ListIter_delete(&subiter);
            }
        } else {
            if (!PORT_Strcasecmp(val->string, EXECUTABLE_STRING)) {
                _this->executable = PR_TRUE;
            }
        }
    }

    /* Default permission value */
    if (!gotPerms) {
        _this->permissions = DEFAULT_PERMISSIONS;
    }

    /* Make sure we got all the information */
    if (!_this->relativePath && !_this->absolutePath) {
        errStr = PR_smprintf(errString[NO_ABSOLUTE_DIR], _this->jarPath);
        goto loser;
    }
#if 0
    if(!_this->relativePath ) {
        errStr = PR_smprintf(errString[NO_RELATIVE_DIR], _this->jarPath);
        goto loser;
    }
    if(!_this->absolutePath) {
        errStr = PR_smprintf(errString[NO_ABSOLUTE_DIR], _this->jarPath);
        goto loser;
    }
#endif

loser:
    if (iter) {
        Pk11Install_ListIter_delete(&iter);
    }
    if (subiter) {
        Pk11Install_ListIter_delete(&subiter);
    }
    return errStr;
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:  Print
// Class:   Pk11Install_File
*/
void
Pk11Install_File_Print(Pk11Install_File* _this, int pad)
{
    PAD(pad);
    printf("jarPath: %s\n",
           _this->jarPath ? _this->jarPath : "<NULL>");
    PAD(pad);
    printf("relativePath: %s\n",
           _this->relativePath ? _this->relativePath : "<NULL>");
    PAD(pad);
    printf("absolutePath: %s\n",
           _this->absolutePath ? _this->absolutePath : "<NULL>");
    PAD(pad);
    printf("permissions: %o\n", _this->permissions);
}

Pk11Install_PlatformName*
Pk11Install_PlatformName_new()
{
    Pk11Install_PlatformName* new_this;
    new_this = (Pk11Install_PlatformName*)
        PR_Malloc(sizeof(Pk11Install_PlatformName));
    Pk11Install_PlatformName_init(new_this);
    return new_this;
}

void
Pk11Install_PlatformName_init(Pk11Install_PlatformName* _this)
{
    _this->OS = NULL;
    _this->verString = NULL;
    _this->numDigits = 0;
    _this->arch = NULL;
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:  ~Pk11Install_PlatformName
// Class:   Pk11Install_PlatformName
*/
void
Pk11Install_PlatformName_delete(Pk11Install_PlatformName* _this)
{
    Pk11Install_PlatformName_Cleanup(_this);
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:  Cleanup
// Class:   Pk11Install_PlatformName
*/
void
Pk11Install_PlatformName_Cleanup(Pk11Install_PlatformName* _this)
{
    if (_this->OS) {
        PR_Free(_this->OS);
        _this->OS = NULL;
    }
    if (_this->verString) {
        int i;
        for (i = 0; i < _this->numDigits; i++) {
            PR_Free(_this->verString[i]);
        }
        PR_Free(_this->verString);
        _this->verString = NULL;
    }
    if (_this->arch) {
        PR_Free(_this->arch);
        _this->arch = NULL;
    }
    _this->numDigits = 0;
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:  Generate
// Class:   Pk11Install_PlatformName
// Notes:   Extracts the information from a platform string.
*/
char*
Pk11Install_PlatformName_Generate(Pk11Install_PlatformName* _this,
                                  const char* str)
{
    char* errStr;
    char* copy;
    char *end, *start;   /* start and end of a section (OS, version, arch)*/
    char *pend, *pstart; /* start and end of one portion of version*/
    char* endp;          /* used by strtol*/
    int periods, i;

    errStr = NULL;
    copy = NULL;

    if (!str) {
        errStr = PR_smprintf(errString[EMPTY_PLATFORM_STRING]);
        goto loser;
    }
    copy = PR_Strdup(str);

    /*
    // Get the OS
    */
    end = strchr(copy, PLATFORM_SEPARATOR_CHAR);
    if (!end || end == copy) {
        errStr = PR_smprintf(errString[BOGUS_PLATFORM_STRING], str);
        goto loser;
    }
    *end = '\0';

    _this->OS = PR_Strdup(copy);

    /*
    // Get the digits of the version of form: x.x.x (arbitrary number of digits)
    */

    start = end + 1;
    end = strchr(start, PLATFORM_SEPARATOR_CHAR);
    if (!end) {
        errStr = PR_smprintf(errString[BOGUS_PLATFORM_STRING], str);
        goto loser;
    }
    *end = '\0';

    if (end != start) {
        /* Find out how many periods*/
        periods = 0;
        pstart = start;
        while ((pend = strchr(pstart, '.'))) {
            periods++;
            pstart = pend + 1;
        }
        _this->numDigits = 1 + periods;
        _this->verString = (char**)PR_Malloc(sizeof(char*) * _this->numDigits);

        pstart = start;
        i = 0;
        /* Get the digits before each period*/
        while ((pend = strchr(pstart, '.'))) {
            if (pend == pstart) {
                errStr = PR_smprintf(errString[BOGUS_PLATFORM_STRING], str);
                goto loser;
            }
            *pend = '\0';
            _this->verString[i] = PR_Strdup(pstart);
            endp = pend;
            if (endp == pstart || (*endp != '\0')) {
                errStr = PR_smprintf(errString[BOGUS_PLATFORM_STRING], str);
                goto loser;
            }
            pstart = pend + 1;
            i++;
        }
        /* Last digit comes after the last period*/
        if (*pstart == '\0') {
            errStr = PR_smprintf(errString[BOGUS_PLATFORM_STRING], str);
            goto loser;
        }
        _this->verString[i] = PR_Strdup(pstart);
        /*
        if(endp==pstart || (*endp != '\0')) {
            errStr = PR_smprintf(errString[BOGUS_PLATFORM_STRING], str);
            goto loser;
        }
        */
    } else {
        _this->verString = NULL;
        _this->numDigits = 0;
    }

    /*
    // Get the architecture
    */
    start = end + 1;
    if (strchr(start, PLATFORM_SEPARATOR_CHAR)) {
        errStr = PR_smprintf(errString[BOGUS_PLATFORM_STRING], str);
        goto loser;
    }
    _this->arch = PR_Strdup(start);

    if (copy) {
        PR_Free(copy);
    }
    return NULL;
loser:
    if (_this->OS) {
        PR_Free(_this->OS);
        _this->OS = NULL;
    }
    if (_this->verString) {
        for (i = 0; i < _this->numDigits; i++) {
            PR_Free(_this->verString[i]);
        }
        PR_Free(_this->verString);
        _this->verString = NULL;
    }
    _this->numDigits = 0;
    if (_this->arch) {
        PR_Free(_this->arch);
        _this->arch = NULL;
    }
    if (copy) {
        PR_Free(copy);
    }

    return errStr;
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:  operator ==
// Class:   Pk11Install_PlatformName
// Returns: PR_TRUE if the platform have the same OS, arch, and version
*/
PRBool
Pk11Install_PlatformName_equal(Pk11Install_PlatformName* _this,
                               Pk11Install_PlatformName* cmp)
{
    int i;

    if (!_this->OS || !_this->arch || !cmp->OS || !cmp->arch) {
        return PR_FALSE;
    }

    if (PORT_Strcasecmp(_this->OS, cmp->OS) ||
        PORT_Strcasecmp(_this->arch, cmp->arch) ||
        _this->numDigits != cmp->numDigits) {
        return PR_FALSE;
    }

    for (i = 0; i < _this->numDigits; i++) {
        if (PORT_Strcasecmp(_this->verString[i], cmp->verString[i])) {
            return PR_FALSE;
        }
    }
    return PR_TRUE;
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:  operator <=
// Class:   Pk11Install_PlatformName
// Returns: PR_TRUE if the platform have the same OS and arch and a lower
//          or equal release.
*/
PRBool
Pk11Install_PlatformName_lteq(Pk11Install_PlatformName* _this,
                              Pk11Install_PlatformName* cmp)
{
    return (Pk11Install_PlatformName_equal(_this, cmp) ||
            Pk11Install_PlatformName_lt(_this, cmp))
               ? PR_TRUE
               : PR_FALSE;
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:  operator <
// Class:   Pk11Install_PlatformName
// Returns: PR_TRUE if the platform have the same OS and arch and a greater
//          release.
*/
PRBool
Pk11Install_PlatformName_lt(Pk11Install_PlatformName* _this,
                            Pk11Install_PlatformName* cmp)
{
    int i, scmp;

    if (!_this->OS || !_this->arch || !cmp->OS || !cmp->arch) {
        return PR_FALSE;
    }

    if (PORT_Strcasecmp(_this->OS, cmp->OS)) {
        return PR_FALSE;
    }
    if (PORT_Strcasecmp(_this->arch, cmp->arch)) {
        return PR_FALSE;
    }

    for (i = 0; (i < _this->numDigits) && (i < cmp->numDigits); i++) {
        scmp = PORT_Strcasecmp(_this->verString[i], cmp->verString[i]);
        if (scmp > 0) {
            return PR_FALSE;
        } else if (scmp < 0) {
            return PR_TRUE;
        }
    }
    /* All the digits they have in common are the same. */
    if (_this->numDigits < cmp->numDigits) {
        return PR_TRUE;
    }

    return PR_FALSE;
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:  GetString
// Class:   Pk11Install_PlatformName
// Returns: String composed of OS, release, and architecture separated
//          by the separator char.  Memory is allocated by this function
//          but is the responsibility of the caller to de-allocate.
*/
char*
Pk11Install_PlatformName_GetString(Pk11Install_PlatformName* _this)
{
    char* ret;
    char* ver;
    char* OS_;
    char* arch_;

    OS_ = NULL;
    arch_ = NULL;

    OS_ = _this->OS ? _this->OS : "";
    arch_ = _this->arch ? _this->arch : "";

    ver = Pk11Install_PlatformName_GetVerString(_this);
    ret = PR_smprintf("%s%c%s%c%s", OS_, PLATFORM_SEPARATOR_CHAR, ver,
                      PLATFORM_SEPARATOR_CHAR, arch_);

    PR_Free(ver);

    return ret;
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:  GetVerString
// Class:   Pk11Install_PlatformName
// Returns: The version string for this platform, in the form x.x.x with an
//          arbitrary number of digits.  Memory allocated by function,
//          must be de-allocated by caller.
*/
char*
Pk11Install_PlatformName_GetVerString(Pk11Install_PlatformName* _this)
{
    char* tmp;
    char* ret;
    int i;
    char buf[80];

    tmp = (char*)PR_Malloc(80 * _this->numDigits + 1);
    tmp[0] = '\0';

    for (i = 0; i < _this->numDigits - 1; i++) {
        sprintf(buf, "%s.", _this->verString[i]);
        strcat(tmp, buf);
    }
    if (i < _this->numDigits) {
        sprintf(buf, "%s", _this->verString[i]);
        strcat(tmp, buf);
    }

    ret = PR_Strdup(tmp);
    free(tmp);

    return ret;
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:  Print
// Class:   Pk11Install_PlatformName
*/
void
Pk11Install_PlatformName_Print(Pk11Install_PlatformName* _this, int pad)
{
    char* str = NULL;
    PAD(pad);
    printf("OS: %s\n", _this->OS ? _this->OS : "<NULL>");
    PAD(pad);
    printf("Digits: ");
    if (_this->numDigits == 0) {
        printf("None\n");
    } else {
        str = Pk11Install_PlatformName_GetVerString(_this);
        printf("%s\n", str);
        PR_Free(str);
    }
    PAD(pad);
    printf("arch: %s\n", _this->arch ? _this->arch : "<NULL>");
}

Pk11Install_Platform*
Pk11Install_Platform_new()
{
    Pk11Install_Platform* new_this;
    new_this = (Pk11Install_Platform*)PR_Malloc(sizeof(Pk11Install_Platform));
    Pk11Install_Platform_init(new_this);
    return new_this;
}

void
Pk11Install_Platform_init(Pk11Install_Platform* _this)
{
    Pk11Install_PlatformName_init(&_this->name);
    Pk11Install_PlatformName_init(&_this->equivName);
    _this->equiv = NULL;
    _this->usesEquiv = PR_FALSE;
    _this->moduleFile = NULL;
    _this->moduleName = NULL;
    _this->modFile = -1;
    _this->mechFlags = 0;
    _this->cipherFlags = 0;
    _this->files = NULL;
    _this->numFiles = 0;
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:  ~Pk11Install_Platform
// Class:   Pk11Install_Platform
*/
void
Pk11Install_Platform_delete(Pk11Install_Platform* _this)
{
    Pk11Install_Platform_Cleanup(_this);
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:  Cleanup
// Class:   Pk11Install_Platform
*/
void
Pk11Install_Platform_Cleanup(Pk11Install_Platform* _this)
{
    int i;
    if (_this->moduleFile) {
        PR_Free(_this->moduleFile);
        _this->moduleFile = NULL;
    }
    if (_this->moduleName) {
        PR_Free(_this->moduleName);
        _this->moduleName = NULL;
    }
    if (_this->files) {
        for (i = 0; i < _this->numFiles; i++) {
            Pk11Install_File_delete(&_this->files[i]);
        }
        PR_Free(_this->files);
        _this->files = NULL;
    }
    _this->equiv = NULL;
    _this->usesEquiv = PR_FALSE;
    _this->modFile = -1;
    _this->numFiles = 0;
    _this->mechFlags = _this->cipherFlags = 0;
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:  Generate
// Class:   Pk11Install_Platform
// Notes:   Creates a platform data structure from a syntax tree.
// Returns: NULL for success, otherwise an error message.
*/
char*
Pk11Install_Platform_Generate(Pk11Install_Platform* _this,
                              const Pk11Install_Pair* pair)
{
    char* errStr;
    char* endptr;
    char* tmp;
    int i;
    Pk11Install_ListIter* iter;
    Pk11Install_Value* val;
    Pk11Install_Value* subval;
    Pk11Install_Pair* subpair;
    Pk11Install_ListIter* subiter;
    PRBool gotModuleFile, gotModuleName, gotMech,
        gotCipher, gotFiles, gotEquiv;

    errStr = NULL;
    iter = subiter = NULL;
    val = subval = NULL;
    subpair = NULL;
    gotModuleFile = gotModuleName = gotMech = gotCipher = gotFiles = gotEquiv = PR_FALSE;
    Pk11Install_Platform_Cleanup(_this);

    errStr = Pk11Install_PlatformName_Generate(&_this->name, pair->key);
    if (errStr) {
        tmp = PR_smprintf("%s: %s", pair->key, errStr);
        PR_smprintf_free(errStr);
        errStr = tmp;
        goto loser;
    }

    iter = Pk11Install_ListIter_new(pair->list);
    for (; (val = iter->current); Pk11Install_ListIter_nextItem(iter)) {
        if (val->type == PAIR_VALUE) {
            subpair = val->pair;

            if (!PORT_Strcasecmp(subpair->key, MODULE_FILE_STRING)) {
                if (gotModuleFile) {
                    errStr = PR_smprintf(errString[REPEAT_MODULE_FILE],
                                         Pk11Install_PlatformName_GetString(&_this->name));
                    goto loser;
                }
                subiter = Pk11Install_ListIter_new(subpair->list);
                subval = subiter->current;
                if (!subval || (subval->type != STRING_VALUE)) {
                    errStr = PR_smprintf(errString[BOGUS_MODULE_FILE],
                                         Pk11Install_PlatformName_GetString(&_this->name));
                    goto loser;
                }
                _this->moduleFile = PR_Strdup(subval->string);
                Pk11Install_ListIter_delete(&subiter);
                gotModuleFile = PR_TRUE;
            } else if (!PORT_Strcasecmp(subpair->key, MODULE_NAME_STRING)) {
                if (gotModuleName) {
                    errStr = PR_smprintf(errString[REPEAT_MODULE_NAME],
                                         Pk11Install_PlatformName_GetString(&_this->name));
                    goto loser;
                }
                subiter = Pk11Install_ListIter_new(subpair->list);
                subval = subiter->current;
                if (!subval || (subval->type != STRING_VALUE)) {
                    errStr = PR_smprintf(errString[BOGUS_MODULE_NAME],
                                         Pk11Install_PlatformName_GetString(&_this->name));
                    goto loser;
                }
                _this->moduleName = PR_Strdup(subval->string);
                Pk11Install_ListIter_delete(&subiter);
                gotModuleName = PR_TRUE;
            } else if (!PORT_Strcasecmp(subpair->key, MECH_FLAGS_STRING)) {
                endptr = NULL;

                if (gotMech) {
                    errStr = PR_smprintf(errString[REPEAT_MECH],
                                         Pk11Install_PlatformName_GetString(&_this->name));
                    goto loser;
                }
                subiter = Pk11Install_ListIter_new(subpair->list);
                subval = subiter->current;
                if (!subval || (subval->type != STRING_VALUE)) {
                    errStr = PR_smprintf(errString[BOGUS_MECH_FLAGS],
                                         Pk11Install_PlatformName_GetString(&_this->name));
                    goto loser;
                }
                _this->mechFlags = strtol(subval->string, &endptr, 0);
                if (*endptr != '\0' || (endptr == subval->string)) {
                    errStr = PR_smprintf(errString[BOGUS_MECH_FLAGS],
                                         Pk11Install_PlatformName_GetString(&_this->name));
                    goto loser;
                }
                Pk11Install_ListIter_delete(&subiter);
                gotMech = PR_TRUE;
            } else if (!PORT_Strcasecmp(subpair->key, CIPHER_FLAGS_STRING)) {
                endptr = NULL;

                if (gotCipher) {
                    errStr = PR_smprintf(errString[REPEAT_CIPHER],
                                         Pk11Install_PlatformName_GetString(&_this->name));
                    goto loser;
                }
                subiter = Pk11Install_ListIter_new(subpair->list);
                subval = subiter->current;
                if (!subval || (subval->type != STRING_VALUE)) {
                    errStr = PR_smprintf(errString[BOGUS_CIPHER_FLAGS],
                                         Pk11Install_PlatformName_GetString(&_this->name));
                    goto loser;
                }
                _this->cipherFlags = strtol(subval->string, &endptr, 0);
                if (*endptr != '\0' || (endptr == subval->string)) {
                    errStr = PR_smprintf(errString[BOGUS_CIPHER_FLAGS],
                                         Pk11Install_PlatformName_GetString(&_this->name));
                    goto loser;
                }
                Pk11Install_ListIter_delete(&subiter);
                gotCipher = PR_TRUE;
            } else if (!PORT_Strcasecmp(subpair->key, FILES_STRING)) {
                if (gotFiles) {
                    errStr = PR_smprintf(errString[REPEAT_FILES],
                                         Pk11Install_PlatformName_GetString(&_this->name));
                    goto loser;
                }
                subiter = Pk11Install_ListIter_new(subpair->list);
                _this->numFiles = subpair->list->numPairs;
                _this->files = (Pk11Install_File*)
                    PR_Malloc(sizeof(Pk11Install_File) * _this->numFiles);
                for (i = 0; i < _this->numFiles; i++,
                    Pk11Install_ListIter_nextItem(subiter)) {
                    Pk11Install_File_init(&_this->files[i]);
                    val = subiter->current;
                    if (val && (val->type == PAIR_VALUE)) {
                        errStr = Pk11Install_File_Generate(&_this->files[i], val->pair);
                        if (errStr) {
                            tmp = PR_smprintf("%s: %s",
                                              Pk11Install_PlatformName_GetString(&_this->name), errStr);
                            PR_smprintf_free(errStr);
                            errStr = tmp;
                            goto loser;
                        }
                    }
                }
                gotFiles = PR_TRUE;
            } else if (!PORT_Strcasecmp(subpair->key,
                                        EQUIVALENT_PLATFORM_STRING)) {
                if (gotEquiv) {
                    errStr = PR_smprintf(errString[REPEAT_EQUIV],
                                         Pk11Install_PlatformName_GetString(&_this->name));
                    goto loser;
                }
                subiter = Pk11Install_ListIter_new(subpair->list);
                subval = subiter->current;
                if (!subval || (subval->type != STRING_VALUE)) {
                    errStr = PR_smprintf(errString[BOGUS_EQUIV],
                                         Pk11Install_PlatformName_GetString(&_this->name));
                    goto loser;
                }
                errStr = Pk11Install_PlatformName_Generate(&_this->equivName,
                                                           subval->string);
                if (errStr) {
                    tmp = PR_smprintf("%s: %s",
                                      Pk11Install_PlatformName_GetString(&_this->name), errStr);
                    tmp = PR_smprintf("%s: %s",
                                      Pk11Install_PlatformName_GetString(&_this->name), errStr);
                    PR_smprintf_free(errStr);
                    errStr = tmp;
                    goto loser;
                }
                _this->usesEquiv = PR_TRUE;
            }
        }
    }

    /* Make sure we either have an EquivalentPlatform or all the other info */
    if (_this->usesEquiv &&
        (gotFiles || gotModuleFile || gotModuleName || gotMech || gotCipher)) {
        errStr = PR_smprintf(errString[EQUIV_TOO_MUCH_INFO],
                             Pk11Install_PlatformName_GetString(&_this->name));
        goto loser;
    }
    if (!gotFiles && !_this->usesEquiv) {
        errStr = PR_smprintf(errString[NO_FILES],
                             Pk11Install_PlatformName_GetString(&_this->name));
        goto loser;
    }
    if (!gotModuleFile && !_this->usesEquiv) {
        errStr = PR_smprintf(errString[NO_MODULE_FILE],
                             Pk11Install_PlatformName_GetString(&_this->name));
        goto loser;
    }
    if (!gotModuleName && !_this->usesEquiv) {
        errStr = PR_smprintf(errString[NO_MODULE_NAME],
                             Pk11Install_PlatformName_GetString(&_this->name));
        goto loser;
    }

    /* Point the modFile pointer to the correct file */
    if (gotModuleFile) {
        for (i = 0; i < _this->numFiles; i++) {
            if (!PORT_Strcasecmp(_this->moduleFile, _this->files[i].jarPath)) {
                _this->modFile = i;
                break;
            }
        }
        if (_this->modFile == -1) {
            errStr = PR_smprintf(errString[UNKNOWN_MODULE_FILE],
                                 _this->moduleFile,
                                 Pk11Install_PlatformName_GetString(&_this->name));
            goto loser;
        }
    }

loser:
    if (iter) {
        PR_Free(iter);
    }
    if (subiter) {
        PR_Free(subiter);
    }
    return errStr;
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:      Print
// Class:       Pk11Install_Platform
*/
void
Pk11Install_Platform_Print(Pk11Install_Platform* _this, int pad)
{
    int i;

    PAD(pad);
    printf("Name:\n");
    Pk11Install_PlatformName_Print(&_this->name, pad + PADINC);
    PAD(pad);
    printf("equivName:\n");
    Pk11Install_PlatformName_Print(&_this->equivName, pad + PADINC);
    PAD(pad);
    if (_this->usesEquiv) {
        printf("Uses equiv, which points to:\n");
        Pk11Install_Platform_Print(_this->equiv, pad + PADINC);
    } else {
        printf("Doesn't use equiv\n");
    }
    PAD(pad);
    printf("Module File: %s\n", _this->moduleFile ? _this->moduleFile
                                                  : "<NULL>");
    PAD(pad);
    printf("mechFlags: %lx\n", _this->mechFlags);
    PAD(pad);
    printf("cipherFlags: %lx\n", _this->cipherFlags);
    PAD(pad);
    printf("Files:\n");
    for (i = 0; i < _this->numFiles; i++) {
        Pk11Install_File_Print(&_this->files[i], pad + PADINC);
        PAD(pad);
        printf("--------------------\n");
    }
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:      Pk11Install_Info
// Class:       Pk11Install_Info
*/
Pk11Install_Info*
Pk11Install_Info_new()
{
    Pk11Install_Info* new_this;
    new_this = (Pk11Install_Info*)PR_Malloc(sizeof(Pk11Install_Info));
    Pk11Install_Info_init(new_this);
    return new_this;
}

void
Pk11Install_Info_init(Pk11Install_Info* _this)
{
    _this->platforms = NULL;
    _this->numPlatforms = 0;
    _this->forwardCompatible = NULL;
    _this->numForwardCompatible = 0;
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:      ~Pk11Install_Info
// Class:       Pk11Install_Info
*/
void
Pk11Install_Info_delete(Pk11Install_Info* _this)
{
    Pk11Install_Info_Cleanup(_this);
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:      Cleanup
// Class:       Pk11Install_Info
*/
void
Pk11Install_Info_Cleanup(Pk11Install_Info* _this)
{
    int i;
    if (_this->platforms) {
        for (i = 0; i < _this->numPlatforms; i++) {
            Pk11Install_Platform_delete(&_this->platforms[i]);
        }
        PR_Free(&_this->platforms);
        _this->platforms = NULL;
        _this->numPlatforms = 0;
    }

    if (_this->forwardCompatible) {
        for (i = 0; i < _this->numForwardCompatible; i++) {
            Pk11Install_PlatformName_delete(&_this->forwardCompatible[i]);
        }
        PR_Free(&_this->forwardCompatible);
        _this->numForwardCompatible = 0;
    }
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:      Generate
// Class:       Pk11Install_Info
// Takes:       Pk11Install_ValueList *list, the top-level list
//              resulting from parsing an installer file.
// Returns:     char*, NULL if successful, otherwise an error string.
//              Caller is responsible for freeing memory.
*/
char*
Pk11Install_Info_Generate(Pk11Install_Info* _this,
                          const Pk11Install_ValueList* list)
{
    char* errStr;
    Pk11Install_ListIter* iter;
    Pk11Install_Value* val;
    Pk11Install_Pair* pair;
    Pk11Install_ListIter* subiter;
    Pk11Install_Value* subval;
    Pk11Install_Platform *first, *second;
    int i, j;

    errStr = NULL;
    iter = subiter = NULL;
    Pk11Install_Info_Cleanup(_this);

    iter = Pk11Install_ListIter_new(list);
    for (; (val = iter->current); Pk11Install_ListIter_nextItem(iter)) {
        if (val->type == PAIR_VALUE) {
            pair = val->pair;

            if (!PORT_Strcasecmp(pair->key, FORWARD_COMPATIBLE_STRING)) {
                subiter = Pk11Install_ListIter_new(pair->list);
                _this->numForwardCompatible = pair->list->numStrings;
                _this->forwardCompatible = (Pk11Install_PlatformName*)
                    PR_Malloc(sizeof(Pk11Install_PlatformName) *
                              _this->numForwardCompatible);
                for (i = 0; i < _this->numForwardCompatible; i++,
                    Pk11Install_ListIter_nextItem(subiter)) {
                    subval = subiter->current;
                    if (subval->type == STRING_VALUE) {
                        errStr = Pk11Install_PlatformName_Generate(
                            &_this->forwardCompatible[i], subval->string);
                        if (errStr) {
                            goto loser;
                        }
                    }
                }
                Pk11Install_ListIter_delete(&subiter);
            } else if (!PORT_Strcasecmp(pair->key, PLATFORMS_STRING)) {
                subiter = Pk11Install_ListIter_new(pair->list);
                _this->numPlatforms = pair->list->numPairs;
                _this->platforms = (Pk11Install_Platform*)
                    PR_Malloc(sizeof(Pk11Install_Platform) *
                              _this->numPlatforms);
                for (i = 0; i < _this->numPlatforms; i++,
                    Pk11Install_ListIter_nextItem(subiter)) {
                    Pk11Install_Platform_init(&_this->platforms[i]);
                    subval = subiter->current;
                    if (subval->type == PAIR_VALUE) {
                        errStr = Pk11Install_Platform_Generate(&_this->platforms[i], subval->pair);
                        if (errStr) {
                            goto loser;
                        }
                    }
                }
                Pk11Install_ListIter_delete(&subiter);
            }
        }
    }

    if (_this->numPlatforms == 0) {
        errStr = PR_smprintf(errString[NO_PLATFORMS]);
        goto loser;
    }

    /*
    //
    // Now process equivalent platforms
    //

    // First the naive pass
    */
    for (i = 0; i < _this->numPlatforms; i++) {
        if (_this->platforms[i].usesEquiv) {
            _this->platforms[i].equiv = NULL;
            for (j = 0; j < _this->numPlatforms; j++) {
                if (Pk11Install_PlatformName_equal(&_this->platforms[i].equivName,
                                                   &_this->platforms[j].name)) {
                    if (i == j) {
                        errStr = PR_smprintf(errString[EQUIV_LOOP],
                                             Pk11Install_PlatformName_GetString(&_this->platforms[i].name));
                        goto loser;
                    }
                    _this->platforms[i].equiv = &_this->platforms[j];
                    break;
                }
            }
            if (_this->platforms[i].equiv == NULL) {
                errStr = PR_smprintf(errString[BOGUS_EQUIV],
                                     Pk11Install_PlatformName_GetString(&_this->platforms[i].name));
                goto loser;
            }
        }
    }

    /*
    // Now the intelligent pass, which will also detect loops.
    // We will send two pointers through the linked list of equivalent
    // platforms. Both start with the current node.  "first" traverses
    // two nodes for each iteration.  "second" lags behind, only traversing
    // one node per iteration.  Eventually one of two things will happen:
    // first will hit the end of the list (a platform that doesn't use
    // an equivalency), or first will equal second if there is a loop.
    */
    for (i = 0; i < _this->numPlatforms; i++) {
        if (_this->platforms[i].usesEquiv) {
            second = _this->platforms[i].equiv;
            if (!second->usesEquiv) {
                /* The first link is the terminal node */
                continue;
            }
            first = second->equiv;
            while (first->usesEquiv) {
                if (first == second) {
                    errStr = PR_smprintf(errString[EQUIV_LOOP],
                                         Pk11Install_PlatformName_GetString(&_this->platforms[i].name));
                    goto loser;
                }
                first = first->equiv;
                if (!first->usesEquiv) {
                    break;
                }
                if (first == second) {
                    errStr = PR_smprintf(errString[EQUIV_LOOP],
                                         Pk11Install_PlatformName_GetString(&_this->platforms[i].name));
                    goto loser;
                }
                second = second->equiv;
                first = first->equiv;
            }
            _this->platforms[i].equiv = first;
        }
    }

loser:
    if (iter) {
        Pk11Install_ListIter_delete(&iter);
    }
    if (subiter) {
        Pk11Install_ListIter_delete(&subiter);
    }
    return errStr;
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:      GetBestPlatform
// Class:       Pk11Install_Info
// Takes:       char *myPlatform, the platform we are currently running
//              on.
*/
Pk11Install_Platform*
Pk11Install_Info_GetBestPlatform(Pk11Install_Info* _this, char* myPlatform)
{
    Pk11Install_PlatformName plat;
    char* errStr;
    int i, j;

    errStr = NULL;

    Pk11Install_PlatformName_init(&plat);
    if ((errStr = Pk11Install_PlatformName_Generate(&plat, myPlatform))) {
        PR_smprintf_free(errStr);
        return NULL;
    }

    /* First try real platforms */
    for (i = 0; i < _this->numPlatforms; i++) {
        if (Pk11Install_PlatformName_equal(&_this->platforms[i].name, &plat)) {
            if (_this->platforms[i].equiv) {
                return _this->platforms[i].equiv;
            } else {
                return &_this->platforms[i];
            }
        }
    }

    /* Now try forward compatible platforms */
    for (i = 0; i < _this->numForwardCompatible; i++) {
        if (Pk11Install_PlatformName_lteq(&_this->forwardCompatible[i], &plat)) {
            break;
        }
    }
    if (i == _this->numForwardCompatible) {
        return NULL;
    }

    /* Got a forward compatible name, find the actual platform. */
    for (j = 0; j < _this->numPlatforms; j++) {
        if (Pk11Install_PlatformName_equal(&_this->platforms[j].name,
                                           &_this->forwardCompatible[i])) {
            if (_this->platforms[j].equiv) {
                return _this->platforms[j].equiv;
            } else {
                return &_this->platforms[j];
            }
        }
    }

    return NULL;
}

/*
//////////////////////////////////////////////////////////////////////////
// Method:      Print
// Class:       Pk11Install_Info
*/
void
Pk11Install_Info_Print(Pk11Install_Info* _this, int pad)
{
    int i;

    PAD(pad);
    printf("Forward Compatible:\n");
    for (i = 0; i < _this->numForwardCompatible; i++) {
        Pk11Install_PlatformName_Print(&_this->forwardCompatible[i], pad + PADINC);
        PAD(pad);
        printf("-------------------\n");
    }
    PAD(pad);
    printf("Platforms:\n");
    for (i = 0; i < _this->numPlatforms; i++) {
        Pk11Install_Platform_Print(&_this->platforms[i], pad + PADINC);
        PAD(pad);
        printf("-------------------\n");
    }
}

/*
//////////////////////////////////////////////////////////////////////////
*/
static char*
PR_Strdup(const char* str)
{
    char* tmp;
    tmp = (char*)PR_Malloc((unsigned int)(strlen(str) + 1));
    strcpy(tmp, str);
    return tmp;
}

/* The global value list, the top of the tree */
Pk11Install_ValueList* Pk11Install_valueList = NULL;

/****************************************************************************/
void
Pk11Install_ValueList_AddItem(Pk11Install_ValueList* _this,
                              Pk11Install_Value* item)
{
    _this->numItems++;
    if (item->type == STRING_VALUE) {
        _this->numStrings++;
    } else {
        _this->numPairs++;
    }
    item->next = _this->head;
    _this->head = item;
}

/****************************************************************************/
Pk11Install_ListIter*
Pk11Install_ListIter_new_default()
{
    Pk11Install_ListIter* new_this;
    new_this = (Pk11Install_ListIter*)
        PR_Malloc(sizeof(Pk11Install_ListIter));
    Pk11Install_ListIter_init(new_this);
    return new_this;
}

/****************************************************************************/
void
Pk11Install_ListIter_init(Pk11Install_ListIter* _this)
{
    _this->list = NULL;
    _this->current = NULL;
}

/****************************************************************************/
Pk11Install_ListIter*
Pk11Install_ListIter_new(const Pk11Install_ValueList* _list)
{
    Pk11Install_ListIter* new_this;
    new_this = (Pk11Install_ListIter*)
        PR_Malloc(sizeof(Pk11Install_ListIter));
    new_this->list = _list;
    new_this->current = _list->head;
    return new_this;
}

/****************************************************************************/
void
Pk11Install_ListIter_delete(Pk11Install_ListIter** _this)
{
    (*_this)->list = NULL;
    (*_this)->current = NULL;
    PR_Free(*_this);
    *_this = NULL;
}

/****************************************************************************/
void
Pk11Install_ListIter_reset(Pk11Install_ListIter* _this)
{
    if (_this->list) {
        _this->current = _this->list->head;
    }
}

/*************************************************************************/
Pk11Install_Value*
Pk11Install_ListIter_nextItem(Pk11Install_ListIter* _this)
{
    if (_this->current) {
        _this->current = _this->current->next;
    }

    return _this->current;
}

/****************************************************************************/
Pk11Install_ValueList*
Pk11Install_ValueList_new()
{
    Pk11Install_ValueList* new_this;
    new_this = (Pk11Install_ValueList*)
        PR_Malloc(sizeof(Pk11Install_ValueList));
    new_this->numItems = 0;
    new_this->numPairs = 0;
    new_this->numStrings = 0;
    new_this->head = NULL;
    return new_this;
}

/****************************************************************************/
void
Pk11Install_ValueList_delete(Pk11Install_ValueList* _this)
{

    Pk11Install_Value* tmp;
    Pk11Install_Value* list;
    list = _this->head;

    while (list != NULL) {
        tmp = list;
        list = list->next;
        PR_Free(tmp);
    }
    PR_Free(_this);
}

/****************************************************************************/
Pk11Install_Value*
Pk11Install_Value_new_default()
{
    Pk11Install_Value* new_this;
    new_this = (Pk11Install_Value*)PR_Malloc(sizeof(Pk11Install_Value));
    new_this->type = STRING_VALUE;
    new_this->string = NULL;
    new_this->pair = NULL;
    new_this->next = NULL;
    return new_this;
}

/****************************************************************************/
Pk11Install_Value*
Pk11Install_Value_new(ValueType _type, Pk11Install_Pointer ptr)
{
    Pk11Install_Value* new_this;
    new_this = Pk11Install_Value_new_default();
    new_this->type = _type;
    if (_type == STRING_VALUE) {
        new_this->pair = NULL;
        new_this->string = ptr.string;
    } else {
        new_this->string = NULL;
        new_this->pair = ptr.pair;
    }
    return new_this;
}

/****************************************************************************/
void
Pk11Install_Value_delete(Pk11Install_Value* _this)
{
    if (_this->type == STRING_VALUE) {
        PR_Free(_this->string);
    } else {
        PR_Free(_this->pair);
    }
}

/****************************************************************************/
Pk11Install_Pair*
Pk11Install_Pair_new_default()
{
    return Pk11Install_Pair_new(NULL, NULL);
}

/****************************************************************************/
Pk11Install_Pair*
Pk11Install_Pair_new(char* _key, Pk11Install_ValueList* _list)
{
    Pk11Install_Pair* new_this;
    new_this = (Pk11Install_Pair*)PR_Malloc(sizeof(Pk11Install_Pair));
    new_this->key = _key;
    new_this->list = _list;
    return new_this;
}

/****************************************************************************/
void
Pk11Install_Pair_delete(Pk11Install_Pair* _this)
{
    PR_Free(_this->key);
    Pk11Install_ValueList_delete(_this->list);
}

/*************************************************************************/
void
Pk11Install_Pair_Print(Pk11Install_Pair* _this, int pad)
{
    while (_this) {
        /*PAD(pad); printf("**Pair\n");
        PAD(pad); printf("***Key====\n");*/
        PAD(pad);
        printf("%s {\n", _this->key);
        /*PAD(pad); printf("====\n");*/
        /*PAD(pad); printf("***ValueList\n");*/
        Pk11Install_ValueList_Print(_this->list, pad + PADINC);
        PAD(pad);
        printf("}\n");
    }
}

/*************************************************************************/
void
Pk11Install_ValueList_Print(Pk11Install_ValueList* _this, int pad)
{
    Pk11Install_Value* v;

    /*PAD(pad);printf("**Value List**\n");*/
    for (v = _this->head; v != NULL; v = v->next) {
        Pk11Install_Value_Print(v, pad);
    }
}

/*************************************************************************/
void
Pk11Install_Value_Print(Pk11Install_Value* _this, int pad)
{
    /*PAD(pad); printf("**Value, type=%s\n",
        type==STRING_VALUE ? "string" : "pair");*/
    if (_this->type == STRING_VALUE) {
        /*PAD(pad+PADINC); printf("====\n");*/
        PAD(pad);
        printf("%s\n", _this->string);
        /*PAD(pad+PADINC); printf("====\n");*/
    } else {
        Pk11Install_Pair_Print(_this->pair, pad + PADINC);
    }
}
