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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
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
#ifndef _PK11_TABLE_H_
#define _PK11_TABLE_H_

/*
 * Supported functions..
 */
#include <pkcs11.h>
#include "nspr.h"
#include "prtypes.h"

typedef enum {
    F_No_Function,
#undef CK_NEED_ARG_LIST
#define CK_PKCS11_FUNCTION_INFO(func) F_##func,
#include "pkcs11f.h"
#undef CK_NEED_ARG_LISt
#undef CK_PKCS11_FUNCTION_INFO
    F_SetVar,
    F_SetStringVar,
    F_NewArray,
    F_NewInitializeArgs,
    F_NewTemplate,
    F_NewMechanism,
    F_BuildTemplate,
    F_SetTemplate,
    F_Print,
    F_SaveVar,
    F_RestoreVar,
    F_Increment,
    F_Decrement,
    F_Delete,
    F_List,
    F_Run,
    F_Load,
    F_Unload,
    F_System,
    F_Loop,
    F_Time,
    F_Help,
    F_Quit,
    F_QuitIf,
    F_QuitIfString
} FunctionType;

/*
 * Supported Argument Types
 */
typedef enum {
    ArgNone,
    ArgVar,
    ArgULong,
    ArgChar,
    ArgUTF8,
    ArgInfo,
    ArgSlotInfo,
    ArgTokenInfo,
    ArgSessionInfo,
    ArgAttribute,
    ArgMechanism,
    ArgMechanismInfo,
    ArgInitializeArgs,
    ArgFunctionList,
/* Modifier Flags */
    ArgMask = 0xff,
    ArgOut = 0x100,
    ArgArray = 0x200,
    ArgNew = 0x400,
    ArgFile = 0x800,
    ArgStatic = 0x1000,
    ArgOpt = 0x2000,
    ArgFull = 0x4000
} ArgType;

typedef enum _constType
{
    ConstNone,
    ConstBool,
    ConstInfoFlags,
    ConstSlotFlags,
    ConstTokenFlags,
    ConstSessionFlags,
    ConstMechanismFlags,
    ConstInitializeFlags,
    ConstUsers,
    ConstSessionState,
    ConstObject,
    ConstHardware,
    ConstKeyType,
    ConstCertType,
    ConstAttribute,
    ConstMechanism,
    ConstResult,
    ConstTrust,
    ConstAvailableSizes,
    ConstCurrentSize
} ConstType;

typedef struct _constant {
    const char *name;
    CK_ULONG value;
    ConstType type;
    ConstType attrType;
} Constant ;

/*
 * Values structures.
 */
typedef struct _values {
    ArgType	type;
    ConstType	constType;
    int		size;
    char	*filename;
    void	*data;
    int 	reference;
    int		arraySize;
} Value;

/*
 * Variables
 */
typedef struct _variable Variable;
struct _variable {
    Variable *next;
    char *vname;
    Value *value;
};

/* NOTE: if you change MAX_ARGS, you need to change the commands array
 * below as well.
 */

#define MAX_ARGS 10
/*
 * structure for master command array
 */
typedef struct _commands {
    char	*fname;
    FunctionType	fType;
    char	*helpString;
    ArgType	args[MAX_ARGS];
} Commands;

typedef struct _module {
    PRLibrary *library;
    CK_FUNCTION_LIST *functionList;
} Module;

typedef struct _topics {
    char	*name;
    char	*helpString;
} Topics;

/*
 * the command array itself. Make name to function and it's arguments
 */

extern const char **valueString;
extern const int valueCount;
extern const char **constTypeString;
extern const int constTypeCount;
extern const Constant *consts;
extern const int constCount;
extern const Commands *commands;
extern const int commandCount;
extern const Topics *topics;
extern const int topicCount;

extern const char *
getName(CK_ULONG value, ConstType type);

extern const char *
getNameFromAttribute(CK_ATTRIBUTE_TYPE type);

extern int totalKnownType(ConstType type);

#endif /* _PK11_TABLE_H_ */

