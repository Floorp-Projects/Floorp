/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "xp.h"
#include "jsapi.h"

#include "prefapi.h"
#include "VerReg.h"
#include "su_mocha.h"
#include "softupdt.h"

void SU_Initialize()
{
}

char su_defaultMode_str[]       = "DEFAULT_MODE";
char su_forceMode_str[]         = "FORCE_MODE";
char su_silentMode_str[]        = "SILENT_MODE";
char su_equal_str[]             = "EQUAL";
char su_bldDiff_str[]           = "BLD_DIFF";
char su_relDiff_str[]           = "REL_DIFF";
char su_minorDiff_str[]         = "MINOR_DIFF";
char su_majorDiff_str[]         = "MAJOR_DIFF";



/* prototypes */

static JSBool PR_CALLBACK compareVersion(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK conditionalSU(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK getVersion(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK startSU(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK suEnabled(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);

static int compareRegWithVersion(char *regname, VERSION *version);

/* structures */

static JSClass trigger_class = {
    "InstallTrigger",
    0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

static JSFunctionSpec trigger_static_methods[] = {
    {"CompareVersion",              compareVersion, 2},
    {"ConditionalSoftwareUpdate",   conditionalSU,  3},
    {"GetVersionInfo",              getVersion,     1},
    {"StartSoftwareUpdate",         startSU,        1},
    {"UpdateEnabled",               suEnabled,      0},
    {0}
};

static JSConstDoubleSpec trigger_constants[] = {
    { SU_DEFAULT_MODE,  su_defaultMode_str  },
    { SU_FORCE_MODE,    su_forceMode_str    },
    { SU_SILENT_MODE,   su_silentMode_str   },
    { SU_EQUAL,         su_equal_str        },
    { SU_BLD_DIFF,      su_bldDiff_str      },
    { SU_REL_DIFF,      su_relDiff_str      },
    { SU_MINOR_DIFF,    su_minorDiff_str    },
    { SU_MAJOR_DIFF,    su_majorDiff_str    },
    {0}
};



/* implementation */

/**
 * Add InstallTrigger class to a JS context
 */
JSBool su_DefineTrigger(JSContext *cx, JSObject *obj)
{
    JSObject *proto;

    proto = JS_InitClass(cx, obj, NULL, &trigger_class, NULL, 0,
        NULL, NULL, NULL, trigger_static_methods);

    if ( proto != NULL )
        if ( JS_DefineConstDoubles(cx, proto, trigger_constants) )
            return JS_TRUE;
    
    return JS_FALSE;
}


/**
 * implements InstallTrigger.GetVersionInfo()
 */
static JSBool PR_CALLBACK getVersion(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    REGERR      err;
    VERSION     version;
    char        *regname;
    JSObject    *verobj;

    /* initialize in case of errors */
    *rval = JSVAL_NULL;

    /* don't give out any info if user has us turned off */
    if ( !SU_IsUpdateEnabled() )
        return JS_TRUE;

    /* find the requested version */
    if ( argc > 0 && JSVAL_IS_STRING(argv[0]) ) {
        regname = JS_GetStringBytes( JSVAL_TO_STRING(argv[0]) );

        err = VR_GetVersion( regname, &version );
        if ( err == REGERR_OK ) {
            verobj = JS_NewObject( cx, &su_version_class, NULL, NULL );
            if ( verobj != NULL ) {
                su_versToObj( cx, &version, verobj );

                *rval = OBJECT_TO_JSVAL(verobj);
            }
        }
    }

    return JS_TRUE;
}



/**
 *  implements InstallTrigger.CompareVersion()
 */
static JSBool PR_CALLBACK compareVersion(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    char    *regname;
    VERSION compver;
    int     result = 0;



    /* basic parameter error checking */
    *rval = JSVAL_ZERO;

    if ( !SU_IsUpdateEnabled() ) {
        /* no further information given! */
        return JS_TRUE;
    }

    if ( argc < 2 || !JSVAL_IS_STRING(argv[0]) ) {
        /* XXX report this error -- bad params */
        return JS_FALSE;
    }


    /* get registry version */
    regname = JS_GetStringBytes( JSVAL_TO_STRING(argv[0]) );
    

    /* convert remaining args into the other version */
    compver.major = compver.minor = compver.release = compver.build = 0;
    
    if ( JSVAL_IS_STRING(argv[1]) ) {
        char *verstr = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
        su_strToVersion( verstr, &compver);
    }
    else if ( JSVAL_IS_OBJECT(argv[1]) ) {
        /* check to make sure it's an InstallVersion first */
        JSObject *argobj;
        JSClass *argclass;

        argobj = JSVAL_TO_OBJECT(argv[1]);
        argclass = JS_GetClass(cx, argobj);

        if ( argclass != &su_version_class ) {
            /* XXX report error here */
            return JS_FALSE;
        }

        su_objToVers( cx, argobj, &compver );
    }
    else {
        if ( JSVAL_IS_INT(argv[1]) )
            compver.major = JSVAL_TO_INT(argv[1]);

        if ( argc > 2 && JSVAL_IS_INT(argv[2]) )
            compver.minor = JSVAL_TO_INT(argv[2]);

        if ( argc > 3 && JSVAL_IS_INT(argv[3]) )
            compver.release = JSVAL_TO_INT(argv[3]);

        if ( argc > 4 && JSVAL_IS_INT(argv[4]) )
            compver.build = JSVAL_TO_INT(argv[4]);
    }
    

    /* finally, do the actual comparison */
    result = compareRegWithVersion( regname, &compver );
    *rval = INT_TO_JSVAL(result);

    return JS_TRUE;
}



/**
 *  implements InstallTrigger.ConditionalSoftwareUpdate()
 *
 *  This method has multiple versions (mode is always optional):
 *    String URL, String regname, InstallVersion version, int mode
 *    String URL, String regname, String version, int mode
 *    String URL, String regname, int difflevel, String version, int mode
 */
static JSBool PR_CALLBACK conditionalSU(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    char        *url;
    char        *regname;
    PRBool      bTriggered;
    PRBool      bNeedJar = PR_FALSE;
    MWContext   *mwcx;
    VERSION     compver;
    int         flags = SU_DEFAULT_MODE;
    int         difflevel = SU_BLD_DIFF;
    uint        verarg = 2; /* version is usually argv[2] */
    int         compresult;

    /* basic parameter validation */
    *rval = JSVAL_FALSE;

    if ( argc < 3 || !JSVAL_IS_STRING(argv[0]) || !JSVAL_IS_STRING(argv[1]) ) {
        /* XXX return some error message */
        return JS_TRUE;
    }

    if ( JSVAL_IS_INT(argv[2]) ) {
        
        difflevel = JSVAL_TO_INT(argv[2]);
        verarg = 3; /* version is argv[3] if there's a difflevel */

        if ( argc < 4 ) {
            /* XXX report this */
            /* variants with a difflevel must also have a version */
            return JS_TRUE;
        }
    }


    /* get the URL and registry name */
    url     = JS_GetStringBytes( JSVAL_TO_STRING(argv[0]) );
    regname = JS_GetStringBytes( JSVAL_TO_STRING(argv[1]) );


    /* get the version to compare against */
    if ( JSVAL_IS_STRING(argv[verarg]) ) {
        char *verstr = JS_GetStringBytes(JSVAL_TO_STRING(argv[verarg]));
        su_strToVersion( verstr, &compver);
    }
    else if ( JSVAL_IS_OBJECT(argv[verarg]) ) {
        /* check to make sure it's an InstallVersion first */
        JSObject *argobj;
        JSClass *argclass;

        argobj = JSVAL_TO_OBJECT(argv[verarg]);
        argclass = JS_GetClass(cx, argobj);

        if ( argclass != &su_version_class ) {
            /* XXX report error here */
            return JS_FALSE;
        }

        su_objToVers( cx, argobj, &compver );
    }


    /* get install mode, if any */
    if ( argc > (verarg+1) && JSVAL_IS_INT(argv[verarg+1]) ) {
        flags = JSVAL_TO_INT(argv[verarg+1]);
    }


    /* perform the comparison */
    compresult = -(compareRegWithVersion( regname, &compver ));

    if ( difflevel < 0 ) {
        /* trigger if installed version is *higher* */
        bNeedJar = (compresult <= difflevel);
    }
    else {
        /* >= because default is BLD_DIFF, and you can 
           trigger on EQUAL if you want */
        bNeedJar = (compresult >= difflevel);
    }


    if ( bNeedJar ) {
        /* XXX This is a potential problem */
        /* We are grabbing any context, but we really need ours */
        /* talk to JS guys to see if we can get it from JS context */
        mwcx = XP_FindSomeContext();
        if (mwcx) {
            bTriggered = SU_StartSoftwareUpdate(mwcx,url,NULL,NULL,NULL,flags);
            *rval = BOOLEAN_TO_JSVAL( bTriggered );
        }
    }

    return JS_TRUE;
}



/**
 *  implements InstallTrigger.StartSoftwareUpdate()
 */
static JSBool PR_CALLBACK startSU(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    char*       url;
    int         flags = SU_DEFAULT_MODE;
    MWContext   *mwcx;
    PRBool      bTriggered;

    if ( argc == 0 || !JSVAL_IS_STRING(argv[0]) ) {
        /* XXX too few or invalid params */
        return JS_FALSE;
    }

    if ( argc > 1 ) {
        if ( JSVAL_IS_INT(argv[1]) )
            flags = JSVAL_TO_INT(argv[1]);
        else
            return JS_FALSE;
    }

    /* XXX This is a potential problem */
    /* We are grabbing any context, but we really need ours */
    /* talk to JS guys to see if we can get it from JS context */
    mwcx = XP_FindSomeContext();
    if (mwcx) {
        url = JS_GetStringBytes( JSVAL_TO_STRING(argv[0]) );

        bTriggered = SU_StartSoftwareUpdate(mwcx,url,NULL,NULL,NULL,flags);
        *rval = BOOLEAN_TO_JSVAL( bTriggered );
    }
    else 
        *rval = JSVAL_FALSE;

    return JS_TRUE;
}


/**
 *  implements InstallTrigger.UpdateEnabled()
 */
static JSBool PR_CALLBACK suEnabled(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    *rval = BOOLEAN_TO_JSVAL( SU_IsUpdateEnabled() );
    return JS_TRUE;
}




static int compareRegWithVersion(char *regname, VERSION *version)
{
    REGERR  err;
    VERSION regver;

    err = VR_ValidateComponent( regname );
    if ( err == REGERR_OK || err == REGERR_NOPATH ) {
        /* the component exists */
        err = VR_GetVersion( regname, &regver );
    }

    if ( err != REGERR_OK ) {
        regver.major = regver.minor = regver.release = regver.build = 0;
    }

    return (su_compareVersions( &regver, version ));
}
