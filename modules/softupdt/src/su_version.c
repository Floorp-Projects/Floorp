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
#include "prprf.h"

#include "su_mocha.h"
#include "softupdt.h"



/* prototypes */

static JSBool PR_CALLBACK NewVersion(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);

static JSBool PR_CALLBACK su_versionCompareTo(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);
static JSBool PR_CALLBACK su_verobjToString(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);



/* structures */

char su_major_str[] =   "major";
char su_minor_str[] =   "minor";
char su_release_str[] = "release";
char su_build_str[] =   "build";

JSClass su_version_class = {
    "InstallVersion",
    0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

static JSFunctionSpec version_methods[] = {
    {"compareTo",   su_versionCompareTo,    1},
    {"toString",    su_verobjToString,     0},
    {0}
};

static JSConstDoubleSpec version_constants[] = {
    { SU_EQUAL,         su_equal_str        },
    { SU_BLD_DIFF,      su_bldDiff_str      },
    { SU_REL_DIFF,      su_relDiff_str      },
    { SU_MINOR_DIFF,    su_minorDiff_str    },
    { SU_MAJOR_DIFF,    su_majorDiff_str    },
    {0}
};



/* implementations */

/**
 * adds the InstallVersion class to a JS context
 */
JSBool su_DefineVersion(JSContext *cx, JSObject *obj)
{
    JSObject *su_version_proto;

    su_version_proto = JS_InitClass(cx, obj, NULL, &su_version_class, NewVersion, 1,
        NULL, version_methods, NULL, NULL);

    if ( su_version_proto != NULL )
        if ( JS_DefineConstDoubles(cx, su_version_proto, version_constants) )
            return JS_TRUE;
    
    return JS_FALSE;
}



/**
 *  InstallVersion constructor
 */
static JSBool PR_CALLBACK NewVersion(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    VERSION vers;

    if ( argc > 0 && JSVAL_IS_STRING(argv[0]) ) {
        su_strToVersion( JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), &vers );
    }
    else {
        vers.major = vers.minor = vers.release = vers.build = 0;

        if ( argc > 0 && JSVAL_IS_INT(argv[0]) )
            vers.major = JSVAL_TO_INT(argv[0]);

        if ( argc > 1 && JSVAL_IS_INT(argv[1]) )
            vers.minor = JSVAL_TO_INT(argv[1]);

        if ( argc > 2 && JSVAL_IS_INT(argv[2]) )
            vers.release = JSVAL_TO_INT(argv[2]);

        if ( argc > 3 && JSVAL_IS_INT(argv[3]) )
            vers.build = JSVAL_TO_INT(argv[3]);
    }

    su_versToObj(cx, &vers, obj);
    return JS_TRUE;
}



/**
 *  implementation of InstallVersion.compareTo()
 */
static JSBool PR_CALLBACK su_versionCompareTo(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    int     compval;
    VERSION thisver, v;

    if ( argc == 0 )
        return JS_FALSE;

    if ( JSVAL_IS_STRING(argv[0]) ) {
        su_strToVersion( JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), &v );
    }
    else if (JSVAL_IS_OBJECT(argv[0]) ) {
        /* check to make sure it's an InstallVersion */
        JSObject *argobj;
        JSClass *argclass;

        argobj = JSVAL_TO_OBJECT(argv[0]);
        argclass = JS_GetClass(cx, argobj);

        if ( argclass != &su_version_class ) {
            /* XXX report error here */
            return JS_FALSE;
        }

        su_objToVers( cx, argobj, &v );
    }
    else {
        v.major = v.minor = v.release = v.build = 0;

        if ( argc > 0 && JSVAL_IS_INT(argv[0]) )
            v.major = JSVAL_TO_INT(argv[0]);

        if ( argc > 1 && JSVAL_IS_INT(argv[1]) )
            v.minor = JSVAL_TO_INT(argv[1]);

        if ( argc > 2 && JSVAL_IS_INT(argv[2]) )
            v.release = JSVAL_TO_INT(argv[2]);

        if ( argc > 3 && JSVAL_IS_INT(argv[3]) )
            v.build = JSVAL_TO_INT(argv[3]);
    }

    su_objToVers( cx, obj, &thisver );

    compval = su_compareVersions( &thisver, &v );
    *rval = INT_TO_JSVAL(compval);

    return JS_TRUE;
}



/**
 * implementation of InstallVersion.toString()
 */
static JSBool PR_CALLBACK su_verobjToString(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    char     *str;
    VERSION  v;

    su_objToVers(cx, obj, &v );
    
    str = PR_smprintf("%d.%d.%d.%d", v.major, v.minor, v.release, v.build);

    *rval = STRING_TO_JSVAL( JS_NewStringCopyZ(cx,str) );

    PR_smprintf_free(str);
    return JS_TRUE;
}




/* Convert VERSION type to Version JS object */
void su_versToObj(JSContext *cx, VERSION* vers, JSObject* versObj)
{
    jsval val = INT_TO_JSVAL(vers->major);
    JS_SetProperty(cx, versObj, su_major_str, &val);
    val = INT_TO_JSVAL(vers->minor);
    JS_SetProperty(cx, versObj, su_minor_str, &val);
    val = INT_TO_JSVAL(vers->release);
    JS_SetProperty(cx, versObj, su_release_str, &val);
    val = INT_TO_JSVAL(vers->build);
    JS_SetProperty(cx, versObj, su_build_str, &val);
}

/* Convert Version JS object to VERSION type */
void su_objToVers(JSContext *cx, JSObject* versObj, VERSION* vers)
{
    jsval val;
    JS_GetProperty(cx, versObj, su_major_str, &val);
    vers->major = JSVAL_TO_INT(val);
    JS_GetProperty(cx, versObj, su_minor_str, &val);
    vers->minor = JSVAL_TO_INT(val);
    JS_GetProperty(cx, versObj, su_release_str, &val);
    vers->release = JSVAL_TO_INT(val);
    JS_GetProperty(cx, versObj, su_build_str, &val);
    vers->build = JSVAL_TO_INT(val);
}

/* Convert version string ("4.1.0.8743") to VERSION type */
void su_strToVersion(char * verstr, VERSION* vers)
{
    char *p;
    char *o = verstr;

    vers->major = vers->minor = vers->release = vers->build = 0;

    if ( o != NULL ) {
        p = strchr(o, '.');
        vers->major = atoi(o);

        if ( p != NULL ) {
            o = p+1;
            p = strchr(o, '.');
            vers->minor = atoi(o);
            
            if ( p != NULL ) {
                o = p+1;
                p = strchr(o,'.');
                vers->release = atoi(o);

                if ( p != NULL ) {
                    o = p+1;
                    p = strchr(o,'.');
                    vers->build = atoi(o);
                }
            }
        }
    }
}

/*
 * Returns 0 if versions are equal; < 0 if vers1 is older; > 0 if vers1 is newer
 */
int su_compareVersions(VERSION* vers1, VERSION* vers2)
{
	int diff;
	if (vers1 == NULL)
		diff = -SU_MAJOR_DIFF;
	else if (vers2 == NULL)
		diff = SU_MAJOR_DIFF;
	else if (vers1->major == vers2->major) {
		if (vers1->minor == vers2->minor) {
			if (vers1->release == vers2->release) {
				if (vers1->build == vers2->build) 
					diff = SU_EQUAL;
				else diff = (vers1->build > vers2->build) ? SU_BLD_DIFF: -SU_BLD_DIFF;
			}
			else diff = (vers1->release > vers2->release) ? SU_REL_DIFF: -SU_REL_DIFF;
		}
		else diff = (vers1->minor > vers2->minor) ? SU_MINOR_DIFF: -SU_MINOR_DIFF;
	}
	else diff = (vers1->major > vers2->major) ? SU_MAJOR_DIFF : -SU_MAJOR_DIFF;

	return diff;
}
