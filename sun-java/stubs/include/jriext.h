/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*******************************************************************************
 * Java Runtime Interface -- Extension Interfaces
 ******************************************************************************/
 
#ifndef JRIEXT_H
#define JRIEXT_H

#include "jri.h"
#include "minicom.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const GUID JRINativePkgID;

/*******************************************************************************
 * Optional Embedding
 ******************************************************************************/

/* Runtime */
extern const GUID JRIRuntimePkgID;

typedef struct JRIRuntimeInterface		JRIRuntimeInterface;
typedef const JRIRuntimeInterface*		JRIRuntimeInstance;

typedef void
(JRI_CALLBACK* JRICollectionStartProc)(JRIRuntimeInstance* runtime);

typedef void
(JRI_CALLBACK* JRICollectionEndProc)(JRIRuntimeInstance* runtime);

/* Passed in JRIRuntimeInitargs: */
typedef enum JRIVerifyMode {
    JRIVerifyNone,
    JRIVerifyRemote,
    JRIVerifyAll
} JRIVerifyMode;

typedef struct JRIRuntimeInitargsStruct {
    /* Required arguments */
    short					majorVersion;
    short					minorVersion;
    jsize					initialHeapSize;
    jsize					maxHeapSize;
    JRICollectionStartProc	collectionStartProc;
    JRICollectionEndProc	collectionEndProc;
    JRIVerifyMode			verifyMode;
    int                     insideNavigator;
} JRIRuntimeInitargs;

/* JRIIOMode control bits. These can be or'd together */
typedef enum JRIIOModeFlags {
    JRIIOMode_Unrestricted		= ~0,
    JRIIOMode_None				= 0,
    JRIIOMode_AllowStdin		= 0x01,
    JRIIOMode_AllowStdout		= 0x02,
    JRIIOMode_AllowSocket		= 0x04,
    JRIIOMode_AllowFileInput	= 0x08,
    JRIIOMode_AllowFileOutput	= 0x10
} JRIIOModeFlags;

typedef enum JRIFSModeFlags {
    JRIFSMode_Unrestricted,	/* no C level filesystem checks */
    JRIFSMode_None		/* no filesystem access allowed */
} JRIFSModeFlags;

typedef enum JRIRTModeFlags {
    JRIRTMode_Unrestricted,	/* no C level runtime checks */
    JRIRTMode_None		/* no runtime access allowed */
} JRIRTModeFlags;

extern JRI_PUBLIC_API(JRIRuntimeInstance*)
JRI_NewRuntime(JRIRuntimeInitargs* initargs);

typedef void
(*JRI_DisposeRuntime_t)(JRIRuntimeInstance* runtime);

/*
** Change the JRI io mode. The JRI io mode is the lowest level security
** check done by the C implementation of the native i/o methods.
*/
typedef void
(*JRI_SetIOMode_t)(JRIRuntimeInstance* runtime, JRIIOModeFlags mode);

/*
** Change the JRI fs mode. The JRI fs mode is the lowest level security
** check done by the C implementation of the native filesystem methods.
*/
typedef void
(*JRI_SetFSMode_t)(JRIRuntimeInstance* runtime, JRIFSModeFlags mode);

/*
** Change the JRI runtime mode. The JRI runtime mode is the lowest
** level security check done by the C implementation of the native
** runtime methods.
*/
typedef void
(*JRI_SetRTMode_t)(JRIRuntimeInstance* runtime, JRIRTModeFlags mode);

/* Environments */

typedef JRIEnv*
(*JRI_NewEnv_t)(JRIRuntimeInstance* runtime, void* thread);

typedef void
(*JRI_DisposeEnv_t)(JRIEnv* env);

typedef JRIRuntimeInstance*
(*JRI_GetRuntime_t)(JRIEnv* env);

typedef void*
(*JRI_GetThread_t)(JRIEnv* env);

typedef void
(*JRI_SetClassLoader_t)(JRIEnv* env, jref classLoader);

struct JRIRuntimeInterface {
    MCOM_QueryInterface_t		QueryInterface;
    MCOM_AddRef_t				AddRef;
    MCOM_Release_t				Release;
	void*						reserved3;
	JRI_DisposeRuntime_t		DisposeRuntime;
	JRI_SetIOMode_t				SetIOMode;
	JRI_SetFSMode_t				SetFSMode;
	JRI_SetRTMode_t				SetRTMode;
	JRI_NewEnv_t				NewEnv;
	JRI_DisposeEnv_t			DisposeEnv;
	JRI_GetRuntime_t			GetRuntime;
	JRI_GetThread_t				GetThread;
	JRI_SetClassLoader_t		SetClassLoader;
};

#define JRI_DisposeRuntime(runtime)		\
	((*(runtime))->DisposeRuntime(runtime))

/*
** Change the JRI io mode. The JRI io mode is the lowest level security
** check done by the C implementation of the native i/o methods.
*/
#define JRI_SetIOMode(runtime, mode)		\
	((*(runtime))->SetIOMode(runtime, mode))

/*
** Change the JRI fs mode. The JRI fs mode is the lowest level security
** check done by the C implementation of the native filesystem methods.
*/
#define JRI_SetFSMode(runtime, mode)		\
	((*(runtime))->SetFSMode(runtime, mode))

/*
** Change the JRI runtime mode. The JRI runtime mode is the lowest
** level security check done by the C implementation of the native
** runtime methods.
*/
#define JRI_SetRTMode(runtime, mode)		\
	((*(runtime))->SetRTMode(runtime, mode))

/* Environments */

#define JRI_NewEnv(runtime, thread)	\
	((*(runtime))->NewEnv(runtime, thread))

#define JRI_DisposeEnv(env)	\
	((*(env))->DisposeEnv(env))

#define JRI_GetRuntime(env)	\
	((*(env))->GetRuntime(env))

#define JRI_GetThread(env)		\
	((*(env))->GetThread(env))

#define JRI_SetClassLoader(env, classLoader)		\
	((*(env))->SetClassLoader(env, classLoader))

/*******************************************************************************
 * Optional Reflection
 ******************************************************************************/

extern const GUID JRIReflectionPkgID;

typedef struct JRIReflectionInterface	JRIReflectionInterface;
typedef const JRIReflectionInterface*	JRIReflectionEnv;

typedef enum JRIAccessFlags {
    /* Field and Method Access */
    JRIAccessPublic			= 0x0001,
    JRIAccessPrivate		= 0x0002,
    JRIAccessProtected		= 0x0004,
    JRIAccessStatic			= 0x0008,
    JRIAccessFinal			= 0x0010,
    JRIAccessSynchronized	= 0x0020,
    JRIAccessNative			= 0x0100,
    /* Class Access */
    JRIAccessInterface		= 0x0200,
    JRIAccessAbstract		= 0x0400
} JRIAccessFlags;

typedef jsize
(*JRI_GetClassCount_t)(JRIReflectionEnv* env);

typedef jref
(*JRI_GetClass_t)(JRIReflectionEnv* env, jsize index);

typedef const char*
(*JRI_GetClassName_t)(JRIReflectionEnv* env, struct java_lang_Class* clazz);

typedef jbool
(*JRI_VerifyClass_t)(JRIReflectionEnv* env, struct java_lang_Class* clazz);

typedef jref
(*JRI_GetClassSuperclass_t)(JRIReflectionEnv* env, struct java_lang_Class* clazz);

/* These next two routines can be used to iterate through all the
   interfaces of a class: */

typedef jsize
(*JRI_GetClassInterfaceCount_t)(JRIReflectionEnv* env, struct java_lang_Class* clazz);

typedef jref
(*JRI_GetClassInterface_t)(JRIReflectionEnv* env, struct java_lang_Class* clazz, jsize index);

/* These next two routines can be used to iterate through all the
   fields of a class, getting info about them: */

typedef jsize
(*JRI_GetClassFieldCount_t)(JRIReflectionEnv* env, struct java_lang_Class* clazz);

typedef void
(*JRI_GetClassFieldInfo_t)(JRIReflectionEnv* env, struct java_lang_Class* clazz,
						   jsize fieldIndex, char* *fieldName, char* *fieldSig, 
						   JRIAccessFlags *fieldAccess, jref *fieldClass);

/* These next two routines can be used to iterate through all the
   methods of a class, getting info about them: */

typedef jsize
(*JRI_GetClassMethodCount_t)(JRIReflectionEnv* env, struct java_lang_Class* clazz);

typedef void
(*JRI_GetClassMethodInfo_t)(JRIReflectionEnv* env, struct java_lang_Class* clazz,
							jsize methodIndex, char* *methodName, char* *methodSig, 
							JRIAccessFlags *methodAccess,
							jref *methodClass, void* *methodNativeProc);

typedef JRIAccessFlags
(*JRI_GetClassAccessFlags_t)(JRIReflectionEnv* env, struct java_lang_Class* clazz);

/******************************************************************************/

struct JRIReflectionInterface {
    MCOM_QueryInterface_t				QueryInterface;
    MCOM_AddRef_t						AddRef;
    MCOM_Release_t						Release;
	void*								reserved3;
	JRI_GetClassCount_t					GetClassCount;
	JRI_GetClass_t						GetClass;
	JRI_GetClassName_t					GetClassName;
	JRI_VerifyClass_t					VerifyClass;
	JRI_GetClassSuperclass_t			GetClassSuperclass;
	JRI_GetClassInterfaceCount_t		GetClassInterfaceCount;
	JRI_GetClassInterface_t				GetClassInterface;
	JRI_GetClassFieldCount_t			GetClassFieldCount;
	JRI_GetClassFieldInfo_t				GetClassFieldInfo;
	JRI_GetClassMethodCount_t			GetClassMethodCount;
	JRI_GetClassMethodInfo_t			GetClassMethodInfo;
	JRI_GetClassAccessFlags_t			GetClassAccessFlags;
};

#define JRI_GetClassCount(env)	\
	((*(env))->GetClassCount(env))

#define JRI_GetClass(env, index)	\
	((*(env))->GetClass(env, index))

#define JRI_GetClassName(env, clazz)		\
	((*(env))->GetClassName(env, clazz))

#define JRI_VerifyClass(env, clazz)		\
	((*(env))->VerifyClass(env, clazz))

#define JRI_GetClassSuperclass(env, clazz)		\
	((*(env))->GetClassSuperclass(env, clazz))

/* These next two routines can be used to iterate through all the
   interfaces of a class: */

#define JRI_GetClassInterfaceCount(env, clazz)	\
	((*(env))->GetClassInterfaceCount(env, clazz))

#define JRI_GetClassInterface(env, clazz, index)	\
	((*(env))->GetClassInterface(env, clazz, index))

/* These next two routines can be used to iterate through all the
   fields of a class, getting info about them: */

#define JRI_GetClassFieldCount(env, clazz)		\
	((*(env))->GetClassFieldCount(env, clazz))

#define JRI_GetClassFieldInfo(env, clazz, fieldIndex, fieldName, fieldSig, fieldAccess, fieldClass)	\
	((*(env))->GetClassFieldInfo(env, clazz, fieldIndex, fieldName, fieldSig, fieldAccess, fieldClass))

/* These next two routines can be used to iterate through all the
   methods of a class, getting info about them: */

#define JRI_GetClassMethodCount(env, clazz)		\
	((*(env))->GetClassMethodCount(env, clazz))

#define JRI_GetClassMethodInfo(env, clazz, methodIndex, methodName, methodSig, methodAccess, methodClass, methodNativeProc)		\
	((*(env))->GetClassMethodInfo(env, clazz, methodIndex, methodName, methodSig, methodAccess, methodClass, methodNativeProc))

#define JRI_GetClassAccessFlags(env, clazz)		\
	((*(env))->GetClassAccessFlags(env, clazz))

/*******************************************************************************
 * Optional Debugger
 ******************************************************************************/

extern const GUID JRIDebuggerPkgID;

typedef struct JRIDebuggerInterface	JRIDebuggerInterface;
typedef const JRIDebuggerInterface*	JRIDebuggerEnv;

/* Manipulating Stacks */

typedef jsize
(*JRI_GetFrameCount_t)(JRIDebuggerEnv* env);

typedef jbool
(*JRI_GetFrameInfo_t)(JRIDebuggerEnv* env, jsize frameIndex,
				  jref *methodClass, jsize *methodIndex,
				  jsize *pc, jsize *varsCount);

typedef void
(*JRI_GetVarInfo_t)(JRIDebuggerEnv* env, jsize frameIndex, jsize varIndex,
					char* *name, char* *signature,
					jbool *isArgument, jsize *startScope, jsize *endScope);

#define JRIVarNotInScope	((JRIFieldID)-1)

typedef void
(*JRI_GetSourceInfo_t)(JRIDebuggerEnv* env, jsize frameIndex,
				   const char* *filename, jsize *lineNumber);

/******************************************************************************/

typedef jref
(*JRI_GetVar_t)(JRIDebuggerEnv* env, jsize frameIndex, jsize varIndex);

typedef jbool
(*JRI_GetVar_boolean_t)(JRIDebuggerEnv* env, jsize frameIndex, jsize varIndex);

typedef jbyte
(*JRI_GetVar_byte_t)(JRIDebuggerEnv* env, jsize frameIndex, jsize varIndex);

typedef jchar
(*JRI_GetVar_char_t)(JRIDebuggerEnv* env, jsize frameIndex, jsize varIndex);

typedef jshort
(*JRI_GetVar_short_t)(JRIDebuggerEnv* env, jsize frameIndex, jsize varIndex);

typedef jint
(*JRI_GetVar_int_t)(JRIDebuggerEnv* env, jsize frameIndex, jsize varIndex);

typedef jlong
(*JRI_GetVar_long_t)(JRIDebuggerEnv* env, jsize frameIndex, jsize varIndex);

typedef jfloat
(*JRI_GetVar_float_t)(JRIDebuggerEnv* env, jsize frameIndex, jsize varIndex);

typedef jdouble
(*JRI_GetVar_double_t)(JRIDebuggerEnv* env, jsize frameIndex, jsize varIndex);

/******************************************************************************/

typedef void
(*JRI_SetVar_t)(JRIDebuggerEnv* env, jsize frameIndex, jsize varIndex, jref value);

typedef void
(*JRI_SetVar_boolean_t)(JRIDebuggerEnv* env, jsize frameIndex, jsize varIndex, jbool value);

typedef void
(*JRI_SetVar_byte_t)(JRIDebuggerEnv* env, jsize frameIndex, jsize varIndex, jbyte value);

typedef void
(*JRI_SetVar_char_t)(JRIDebuggerEnv* env, jsize frameIndex, jsize varIndex, jchar value);

typedef void
(*JRI_SetVar_short_t)(JRIDebuggerEnv* env, jsize frameIndex, jsize varIndex, jshort value);

typedef void
(*JRI_SetVar_int_t)(JRIDebuggerEnv* env, jsize frameIndex, jsize varIndex, jint value);

typedef void
(*JRI_SetVar_long_t)(JRIDebuggerEnv* env, jsize frameIndex, jsize varIndex, jlong value);

typedef void
(*JRI_SetVar_float_t)(JRIDebuggerEnv* env, jsize frameIndex, jsize varIndex, jfloat value);

typedef void
(*JRI_SetVar_double_t)(JRIDebuggerEnv* env, jsize frameIndex, jsize varIndex, jdouble value);

/******************************************************************************/

/* Controlling Execution */

typedef void
(*JRI_StepOver_t)(JRIDebuggerEnv* env);

typedef void
(*JRI_StepIn_t)(JRIDebuggerEnv* env);

typedef void
(*JRI_StepOut_t)(JRIDebuggerEnv* env);

typedef void
(*JRI_Continue_t)(JRIDebuggerEnv* env);

typedef void
(*JRI_Return_t)(JRIDebuggerEnv* env, jsize frameIndex, JRIValue value);

/******************************************************************************/

struct JRIDebuggerInterface {
    MCOM_QueryInterface_t		QueryInterface;
    MCOM_AddRef_t				AddRef;
    MCOM_Release_t				Release;
	void*						reserved3;
	JRI_GetFrameCount_t			GetFrameCount;
	JRI_GetFrameInfo_t			GetFrameInfo;
	JRI_GetVarInfo_t			GetVarInfo;
	JRI_GetSourceInfo_t			GetSourceInfo;
	JRI_GetVar_t				GetVar;
	JRI_GetVar_boolean_t		GetVar_boolean;
	JRI_GetVar_byte_t			GetVar_byte;
	JRI_GetVar_char_t			GetVar_char;
	JRI_GetVar_short_t			GetVar_short;
	JRI_GetVar_int_t			GetVar_int;
	JRI_GetVar_long_t			GetVar_long;
	JRI_GetVar_float_t			GetVar_float;
	JRI_GetVar_double_t			GetVar_double;
	JRI_SetVar_t				SetVar;
	JRI_SetVar_boolean_t		SetVar_boolean;
	JRI_SetVar_byte_t			SetVar_byte;
	JRI_SetVar_char_t			SetVar_char;
	JRI_SetVar_short_t			SetVar_short;
	JRI_SetVar_int_t			SetVar_int;
	JRI_SetVar_long_t			SetVar_long;
	JRI_SetVar_float_t			SetVar_float;
	JRI_SetVar_double_t			SetVar_double;
	JRI_StepOver_t				StepOver;
	JRI_StepIn_t				StepIn;
	JRI_StepOut_t				StepOut;
	JRI_Continue_t				Continue;
	JRI_Return_t				Return;
};


#define JRI_GetFrameCount(env)	\
	((*(env))->GetFrameCount(env))

#define JRI_GetFrameInfo(env, frameIndex, methodClass, methodIndex, pc, varsCount)	\
	((*(env))->GetFrameInfo(env, frameIndex, methodClass, methodIndex, pc, varsCount))

#define JRI_GetVarInfo(env, frameIndex, varIndex, name, signature, pos, isArgument, startScope, endScope)		\
	((*(env))->GetVarInfo(env, frameIndex, varIndex, name, signature, pos, isArgument, startScope, endScope))

#define JRI_GetSourceInfo(env, frameIndex, filename, lineNumber)	\
	((*(env))->GetSourceInfo(env, frameIndex, filename, lineNumber))

/******************************************************************************/

#define JRI_GetVar(env, frameIndex, varIndex)		\
	((*(env))->GetVar(env, frameIndex, varIndex))

#define JRI_GetVar_boolean(env, frameIndex, varIndex)		\
	((*(env))->GetVar_boolean(env, frameIndex, varIndex))

#define JRI_GetVar_byte(env, frameIndex, varIndex)	\
	((*(env))->GetVar_byte(env, frameIndex, varIndex))

#define JRI_GetVar_char(env, frameIndex, varIndex)	\
	((*(env))->GetVar_char(env, frameIndex, varIndex))

#define JRI_GetVar_short(env, frameIndex, varIndex)		\
	((*(env))->GetVar_short(env, frameIndex, varIndex))

#define JRI_GetVar_int(env, frameIndex, varIndex)	\
	((*(env))->GetVar_int(env, frameIndex, varIndex))

#define JRI_GetVar_long(env, frameIndex, varIndex)	\
	((*(env))->GetVar_long(env, frameIndex, varIndex))

#define JRI_GetVar_float(env, frameIndex, varIndex)		\
	((*(env))->GetVar_float(env, frameIndex, varIndex))

#define JRI_GetVar_double(env, frameIndex, varIndex)		\
	((*(env))->GetVar_double(env, frameIndex, varIndex))

/******************************************************************************/

#define JRI_SetVar(env, frameIndex, varIndex, value)	\
	((*(env))->SetVar(env, frameIndex, varIndex, value))

#define JRI_SetVar_boolean(env, frameIndex, varIndex, value)	\
	((*(env))->SetVar_boolean(env, frameIndex, varIndex, value))

#define JRI_SetVar_byte(env, frameIndex, varIndex, value)	\
	((*(env))->SetVar_byte(env, frameIndex, varIndex, value))

#define JRI_SetVar_char(env, frameIndex, varIndex, value)	\
	((*(env))->SetVar_char(env, frameIndex, varIndex, value))

#define JRI_SetVar_short(env, frameIndex, varIndex, value)	\
	((*(env))->SetVar_short(env, frameIndex, varIndex, value))

#define JRI_SetVar_int(env, frameIndex, varIndex, value)		\
	((*(env))->SetVar_int(env, frameIndex, varIndex, value))

#define JRI_SetVar_long(env, frameIndex, varIndex, value)	\
	((*(env))->SetVar_long(env, frameIndex, varIndex, value))

#define JRI_SetVar_float(env, frameIndex, varIndex, value)	\
	((*(env))->SetVar_float(env, frameIndex, varIndex, value))

#define JRI_SetVar_double(env, frameIndex, varIndex, value)		\
	((*(env))->SetVar_double(env, frameIndex, varIndex, value))

/******************************************************************************/

/* Controlling Execution */

#define JRI_StepOver(env)		\
	((*(env))->StepOver(env))

#define JRI_StepIn(env)		\
	((*(env))->StepIn(env))

#define JRI_StepOut(env)		\
	((*(env))->StepOut(env))

#define JRI_Continue(env)		\
	((*(env))->Continue(env))

#define JRI_Return(env, frameIndex, value)		\
	((*(env))->Return(env, frameIndex, value))

/*******************************************************************************
 * Optional Compiling
 ******************************************************************************/

extern const GUID JRICompilerPkgID;

typedef struct JRICompilerInterface	JRICompilerInterface;
typedef const JRICompilerInterface*	JRICompilerEnv;

typedef void
(*JRI_CompileClass_t)(JRICompilerEnv* env,
					  const char* classSrc, jsize classSrcLen,
					  jbyte* *resultingClassData, jsize *classDataLen);

struct JRICompilerInterface {
    MCOM_QueryInterface_t	QueryInterface;
    MCOM_AddRef_t			AddRef;
    MCOM_Release_t			Release;
	void*					reserved3;
	JRI_CompileClass_t		CompileClass;
};

#define JRI_CompileClass(env, classSrc, classSrcLen, resultingClassData, classDataLen)		\
	((*(env))->CompileClass(env, classSrc, classSrcLen, resultingClassData, classDataLen))

/*******************************************************************************
 * Optional Expression Evaluation
 ******************************************************************************/

extern const GUID JRIExprPkgID;

typedef struct JRIExprInterface	JRIExprInterface;
typedef const JRIExprInterface*	JRIExprEnv;

typedef jref
(*JRI_CompileExpr_t)(JRIExprEnv* env,
					 const char* exprSrc, jsize exprSrcLen);

typedef jref
(*JRI_EvalExpr_t)(JRIExprEnv* env, jref expr);

struct JRIExprInterface {
    MCOM_QueryInterface_t	QueryInterface;
    MCOM_AddRef_t			AddRef;
    MCOM_Release_t			Release;
	void*					reserved3;
	JRI_CompileExpr_t		CompileExpr;
	JRI_EvalExpr_t			EvalExpr;
};

#define JRI_CompileExpr(env, exprSrc, exprSrcLen)		\
	((*(env))->CompileExpr(env, exprSrc, exprSrcLen))

#define JRI_EvalExpr(env, expr)		\
	((*(env))->EvalExpr(env, expr))

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif /* JRIEXT_H */
/******************************************************************************/
