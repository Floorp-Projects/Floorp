/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Daniel Veditz <dveditz@netscape.com>
 */

#include "xp.h"
#include "jsapi.h"

#include "su_mocha.h"
#include "nsSoftwareUpdate.h"




/**
 * Outside modules call SU_InitMochaClasses() to add our classes to their JS
 * context, generally anywhere JS_InitStandardClasses() is called.
 */
JSBool SU_InitMochaClasses(JSContext *cx, JSObject *obj)
{
    if ( !su_DefineInstall(cx, obj) )
        return JS_FALSE;

    if ( !su_DefineTrigger(cx, obj) )
        return JS_FALSE;

    if ( !su_DefineVersion(cx, obj) )
        return JS_FALSE;

    return JS_TRUE;
}



// 
// prototypes
//

static void PR_CALLBACK inst_destroy(JSContext *cx, JSObject *obj);

static JSBool PR_CALLBACK NewInstall(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);

static JSBool PR_CALLBACK inst_abortInstall(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK inst_addDirectory(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK inst_addSubcomponent(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK inst_deleteComponent(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK inst_deleteFile(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK inst_diskSpaceAvailable(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK inst_execute(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK inst_finalizeInstall(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK inst_gestalt(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK inst_getComponentFolder(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK inst_getFolder(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK inst_getLastError(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK inst_getWinProfile(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK inst_getWinRegistry(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK inst_patch(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK inst_resetError(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK inst_setPackageFolder(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK inst_startInstall(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK inst_uninstall(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);



//
// structures
//

static JSClass install_class = {
    "Install", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, inst_destroy
};

static JSFunctionSpec install_methods[] = {
    {"AbortInstall",        inst_abortInstall,          0},
    {"AddDirectory",        inst_addDirectory,          6},
    {"AddSubcomponent",     inst_addSubcomponent,       6},
    {"DeleteComponent",     inst_deleteComponent,       1},
    {"DeleteFile",          inst_deleteFile,            2},
    {"DiskSpaceAvailable",  inst_diskSpaceAvailable,    1},
    {"Execute",             inst_execute,               2},
    {"FinalizeInstall",     inst_finalizeInstall,       0},
    {"Gestalt",             inst_gestalt,               1},
    {"GetComponentFolder",  inst_getComponentFolder,    2},
    {"GetFolder",           inst_getFolder,             2},
    {"GetLastError",        inst_getLastError,          0},
    {"GetWinProfile",       inst_getWinProfile,         2},
    {"GetWinRegistry",      inst_getWinRegistry,        0},
    {"Patch",               inst_patch,                 5},
    {"ResetError",          inst_resetError,            0},
    {"SetPackageFolder",    inst_setPackageFolder,      1},
    {"StartInstall",        inst_startInstall,          3},
    {"Uninstall",           inst_uninstall,             1},
    {0}
};



//
// functions
//

JSBool su_DefineInstall(JSContext *cx, JSObject *obj)
{
    JSObject *proto;

    proto = JS_InitClass(cx, obj, NULL, &install_class, NewInstall, 2,
        NULL, install_methods, NULL, NULL);

    return (proto != NULL);
}


static JSBool PR_CALLBACK NewInstall(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    JSObject            *install;
    nsSoftwareUpdate    *su;

    // sanity checks
    if ( argc < 2 )
        return JS_FALSE;

    if ( !JSVAL_IS_OBJECT(argv[0]) || !JSVAL_IS_STRING(argv[1]) )
        return JS_FALSE;

    // create the object
    install = JS_NewObject(cx, &install_class, NULL, obj);
    if ( install != NULL ) {
        su = new nsSoftwareUpdate( JSVAL_TO_OBJECT(argv[0]), 
                 JS_GetStringBytes(JSVAL_TO_STRING(argv[1])) );
        if ( su != NULL ) {
            if ( JS_SetPrivate( cx, install, su ) ) {
                *rval = OBJECT_TO_JSVAL(install);
                return JS_TRUE;
            }
        }
    }

    return JS_FALSE;
}


static void PR_CALLBACK inst_destroy(JSContext *cx, JSObject *obj)
{
}

static JSBool PR_CALLBACK inst_abortInstall(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSBool PR_CALLBACK inst_addDirectory(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSBool PR_CALLBACK inst_addSubcomponent(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSBool PR_CALLBACK inst_deleteComponent(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSBool PR_CALLBACK inst_deleteFile(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSBool PR_CALLBACK inst_diskSpaceAvailable(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSBool PR_CALLBACK inst_execute(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSBool PR_CALLBACK inst_finalizeInstall(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSBool PR_CALLBACK inst_gestalt(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSBool PR_CALLBACK inst_getComponentFolder(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSBool PR_CALLBACK inst_getFolder(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSBool PR_CALLBACK inst_getLastError(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSBool PR_CALLBACK inst_getWinProfile(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSBool PR_CALLBACK inst_getWinRegistry(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSBool PR_CALLBACK inst_patch(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSBool PR_CALLBACK inst_resetError(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSBool PR_CALLBACK inst_setPackageFolder(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSBool PR_CALLBACK inst_startInstall(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

static JSBool PR_CALLBACK inst_uninstall(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

