/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 *
 *  $RCSfile: rdfjava.c,v $ $Revision: 3.1 $
 *	Copyright © 1997 Netscape Communications Corporation, All Rights Reserved.
 */

#ifdef XP_WIN32

/* this is win32 only for now until the JNI issue is resolved
for other platforms.  it may well be that this needs to become JRI code */

#include <jni.h>
#include "netscape_rdf_core_NativeRDF.h"
#include "netscape_rdf_core_NativeRDFEnumeration.h"

#include "prtypes.h"
#include "prmem.h"
#include "rdf.h"
#include "rdf-int.h"            /* XXX bad */


static RDF db;					/* there can only be one */

#define pr2jnibool(v) ((v == PR_TRUE) ? JNI_TRUE : JNI_FALSE)
#define jni2prbool(v) ((v == JNI_TRUE) ? PR_TRUE : PR_FALSE)

#define RDF_ERROR NULL			/* XXX need error resource here */

static RDF_Resource 
native_resource(JNIEnv *ee, jobject resource) 
{
    static jfieldID fid = NULL;
	jstring jstr;
	RDF_Resource nresource = NULL;

	if (fid == NULL) {
	  /* only do this once since its a well known class */
	  jclass cls = (*ee)->GetObjectClass(ee, resource);
	  fid = (*ee)->GetFieldID(ee, cls, "resourceID", "Ljava/lang/String;");
	  if(fid == NULL)
		  return NULL;
	}

	jstr = (*ee)->GetObjectField(ee, resource, fid);
	if (jstr) {
	    const char *str;
	    str = (*ee)->GetStringUTFChars(ee, jstr, 0);
        /* XXX no RDF_GetResource */
		nresource = RDF_GetResource(NULL, (char *)str, PR_TRUE);
		(*ee)->ReleaseStringUTFChars(ee, jstr, str);
	}
    return nresource;
}


/* XXX this needs to be real */
static jchar *
char2unicode(char *str, int32 len)
{
    char *r;
    if (r = PR_Calloc(len, sizeof(jchar))) {
        while (*str && --len >= 0) {
            *r++ = (char)0xff;
            *r++ = *str++;
        }
    }
    return (jchar *)r;
}

static jobject
java_resource(JNIEnv *ee, RDF_Resource resource)
{
    jobject jresource = NULL;
    jclass cls = (*ee)->FindClass(ee, "netscape.rdf.Resource");
    if (cls) {
        jmethodID mid = (*ee)->GetStaticMethodID(ee, cls, "getResource", 
                                                 "(Ljava/lang/String;)Lnetscape/rdf/Resource;");
        if (mid != NULL) {
            jchar *ustring;
            int32 len = strlen(resource->id);
            if (ustring = char2unicode(resource->id, len)) {
                jstring jid = (*ee)->NewString(ee, ustring, len);
                if (jid != NULL) {
                    jresource = (*ee)->CallStaticObjectMethod(ee, cls, mid, jid);
                }
            }
        }
    }
    return jresource;
}


JNIEXPORT void JNICALL
Java_netscape_rdf_core_NativeRDF_NativeRDF0
	/* ([Ljava/lang/String;)V */
(JNIEnv *ee, jobject this, jobjectArray array) 
{
    /* allow only one instance of the db for now */
    if(!db) {
	    jsize i, len;
		const char **dbs;
		jobject obj;

		len = (*ee)->GetArrayLength(ee, array);
		dbs = PR_Calloc(len, sizeof(char *));
		if (dbs) {
		    for (i=0; i<len; i++) {
			    obj = (*ee)->GetObjectArrayElement(ee, array, i);
				/* XXX check for a string */
				dbs[i] = (*ee)->GetStringUTFChars(ee, obj, 0);
			}
            /* XXX API should take const */
			db = RDF_GetDB((char **)dbs);
		    for (i=0; i<len; i++) {
			    obj = (*ee)->GetObjectArrayElement(ee, array, i);
				(*ee)->ReleaseStringUTFChars(ee, obj, dbs[i]);
			}
	   }
	}
	return;
}

static jobject 
native_assert(JNIEnv *ee, jobject this, jobject src, jobject arcLabel, RDF_ValueType targetType, void *nTarget)
{
    PRBool v;
    RDF_Resource nSrc, narcLabel;

	nSrc = native_resource(ee, src);
	narcLabel = native_resource(ee, arcLabel);
	if(!(nSrc && narcLabel && nTarget)) {
	    return RDF_ERROR;
	}
	v = RDF_Assert(db, nSrc, narcLabel, nTarget, targetType);
	return (v == PR_TRUE)? NULL : RDF_ERROR;
}

JNIEXPORT jobject JNICALL 
Java_netscape_rdf_core_NativeRDF_assert__Lnetscape_rdf_Resource_2Lnetscape_rdf_Resource_2Lnetscape_rdf_Resource_2
    /* (Lnetscape/rdf/Resource;Lnetscape/rdf/Resource;Lnetscape/rdf/Resource;)Lnetscape/rdf/Resource */
(JNIEnv *ee, jobject this, jobject src, jobject arcLabel, jobject target) 
{
    void *nTarget = (void *)native_resource(ee, target);
    return native_assert(ee, this, src, arcLabel, RDF_RESOURCE_TYPE, nTarget);
}

JNIEXPORT jobject JNICALL 
Java_netscape_rdf_core_NativeRDF_assert__Lnetscape_rdf_Resource_2Lnetscape_rdf_Resource_2Ljava_lang_String_2
    /* (Lnetscape/rdf/Resource;Lnetscape/rdf/Resource;Ljava/lang/String;)Lnetscape/rdf/Resource */
(JNIEnv *ee, jobject this, jobject src, jobject arcLabel, jstring target) 
{
    jobject v;
    const char *nTarget = (*ee)->GetStringUTFChars(ee, target, 0);
    v =  native_assert(ee, this, src, arcLabel, RDF_STRING_TYPE, (void *)nTarget);
	(*ee)->ReleaseStringUTFChars(ee, target, nTarget);
	return v;
}

JNIEXPORT jobject JNICALL 
Java_netscape_rdf_core_NativeRDF_assert__Lnetscape_rdf_Resource_2Lnetscape_rdf_Resource_2I
  	/* (Lnetscape/rdf/Resource;Lnetscape/rdf/Resource;I)Lnetscape/rdf/Resource */
(JNIEnv *ee, jobject this, jobject src, jobject arcLabel, jint target) 
{
    void *nTarget = (void *)&target;
    return native_assert(ee, this, src, arcLabel, RDF_INT_TYPE, nTarget);
}

static jobject 
native_assert_false(JNIEnv *ee, jobject this, jobject src, jobject arcLabel, RDF_ValueType targetType, void *nTarget)
{
    PRBool v;
    RDF_Resource nSrc, narcLabel;

	nSrc = native_resource(ee, src);
	narcLabel = native_resource(ee, arcLabel);
	if(!(nSrc && narcLabel && nTarget)) {
	    return RDF_ERROR;
	}
	v = RDF_AssertFalse(db, nSrc, narcLabel, nTarget, targetType);
	return (v == PR_TRUE)? NULL : RDF_ERROR;
}

JNIEXPORT jobject JNICALL 
Java_netscape_rdf_core_NativeRDF_assertFalse__Lnetscape_rdf_Resource_2Lnetscape_rdf_Resource_2Lnetscape_rdf_Resource_2
    /* (Lnetscape/rdf/Resource;Lnetscape/rdf/Resource;Lnetscape/rdf/Resource;)Lnetscape/rdf/Resource */
(JNIEnv *ee, jobject this, jobject src, jobject arcLabel, jobject target) 
{
    void *nTarget = (void *)native_resource(ee, target);
    return native_assert_false(ee, this, src, arcLabel, RDF_RESOURCE_TYPE, nTarget);
}

JNIEXPORT jobject JNICALL 
Java_netscape_rdf_core_NativeRDF_assertFalse__Lnetscape_rdf_Resource_2Lnetscape_rdf_Resource_2I
	/* (Lnetscape/rdf/Resource;Lnetscape/rdf/Resource;I)Lnetscape/rdf/Resource */
(JNIEnv *ee, jobject this, jobject src, jobject arcLabel, jint target) 
{
    void *nTarget = (void *)&target;
    return native_assert_false(ee, this, src, arcLabel, RDF_INT_TYPE, nTarget);
}

JNIEXPORT jobject JNICALL 
Java_netscape_rdf_core_NativeRDF_assertFalse__Lnetscape_rdf_Resource_2Lnetscape_rdf_Resource_2Ljava_lang_String_2
    /* (Lnetscape/rdf/Resource;Lnetscape/rdf/Resource;Ljava/lang/String;)Lnetscape/rdf/Resource */
(JNIEnv *ee, jobject this, jobject src, jobject arcLabel, jstring target) 
{
    jobject v;
    const char *nTarget = (*ee)->GetStringUTFChars(ee, target, 0);
    v =  native_assert_false(ee, this, src, arcLabel, RDF_STRING_TYPE, (void *)nTarget);
	(*ee)->ReleaseStringUTFChars(ee, target, nTarget);
	return v;
}

static jobject 
native_unassert(JNIEnv *ee, jobject this, jobject src, jobject arcLabel, RDF_ValueType targetType, void *nTarget)
{
    PRBool v;
    RDF_Resource nSrc, narcLabel;

	nSrc = native_resource(ee, src);
	narcLabel = native_resource(ee, arcLabel);
	if(!(nSrc && narcLabel && nTarget)) {
	    return RDF_ERROR;
	}
	v = RDF_Unassert(db, nSrc, narcLabel, nTarget, targetType);
	return (v == PR_TRUE)? NULL : RDF_ERROR;
}

JNIEXPORT jobject JNICALL 
Java_netscape_rdf_core_NativeRDF_unassert__Lnetscape_rdf_Resource_2Lnetscape_rdf_Resource_2Lnetscape_rdf_Resource_2
    /* (Lnetscape/rdf/Resource;Lnetscape/rdf/Resource;Lnetscape/rdf/Resource;)Lnetscape/rdf/Resource */
(JNIEnv *ee, jobject this, jobject src, jobject arcLabel, jobject target) 
{
    void *nTarget = (void *)native_resource(ee, target);
    return native_unassert(ee, this, src, arcLabel, RDF_RESOURCE_TYPE, nTarget);
}

JNIEXPORT jobject JNICALL 
Java_netscape_rdf_core_NativeRDF_unassert__Lnetscape_rdf_Resource_2Lnetscape_rdf_Resource_2I
	/* (Lnetscape/rdf/Resource;Lnetscape/rdf/Resource;I)Lnetscape/rdf/Resource */
(JNIEnv *ee, jobject this, jobject src, jobject arcLabel, jint target) 
{
    void *nTarget = (void *)&target;
    return native_unassert(ee, this, src, arcLabel, RDF_INT_TYPE, nTarget);
}

JNIEXPORT jobject JNICALL 
Java_netscape_rdf_core_NativeRDF_unassert__Lnetscape_rdf_Resource_2Lnetscape_rdf_Resource_2Ljava_lang_String_2
    /* (Lnetscape/rdf/Resource;Lnetscape/rdf/Resource;Ljava/lang/String;)Lnetscape/rdf/Resource */
(JNIEnv *ee, jobject this, jobject src, jobject arcLabel, jstring target) 
{
    jobject v;
    const char *nTarget = (*ee)->GetStringUTFChars(ee, target, 0);
    v =  native_unassert(ee, this, src, arcLabel, RDF_STRING_TYPE, (void *)nTarget);
	(*ee)->ReleaseStringUTFChars(ee, target, nTarget);
	return v;
}

static jboolean 
native_has_assertion(JNIEnv *ee, jobject this, jobject src, jobject arcLabel, RDF_ValueType targetType, 
					 void *nTarget, jboolean isTrue)
{
    PRBool v;
    RDF_Resource nSrc, narcLabel;

	nSrc = native_resource(ee, src);
	narcLabel = native_resource(ee, arcLabel);
	if(!(nSrc && narcLabel && nTarget)) {
	    return JNI_FALSE;
	}
	v = RDF_HasAssertion(db, nSrc, narcLabel, nTarget, targetType, jni2prbool(isTrue));
	return pr2jnibool(v);
}

JNIEXPORT jboolean JNICALL 
Java_netscape_rdf_core_NativeRDF_hasAssertion__Lnetscape_rdf_Resource_2Lnetscape_rdf_Resource_2Lnetscape_rdf_Resource_2Z
    /* (Lnetscape/rdf/Resource;Lnetscape/rdf/Resource;Lnetscape/rdf/Resource;Z) */
(JNIEnv *ee, jobject this, jobject src, jobject arcLabel, jobject target, jboolean isTrue)
{
    void *nTarget = (void *)native_resource(ee, target);
    return native_has_assertion(ee, this, src, arcLabel, RDF_RESOURCE_TYPE, nTarget, isTrue);
}

JNIEXPORT jboolean JNICALL 
Java_netscape_rdf_core_NativeRDF_hasAssertion__Lnetscape_rdf_Resource_2Lnetscape_rdf_Resource_2Ljava_lang_String_2Z
    /* (Lnetscape/rdf/Resource;Lnetscape/rdf/Resource;Ljava/lang/String;Z) */
(JNIEnv *ee, jobject this, jobject src, jobject arcLabel, jstring target, jboolean isTrue) 
{
    jboolean v;
    const char *nTarget = (*ee)->GetStringUTFChars(ee, target, 0);
    v = native_has_assertion(ee, this, src, arcLabel, RDF_STRING_TYPE, (void *)nTarget, isTrue);
	(*ee)->ReleaseStringUTFChars(ee, target, nTarget);
	return v;
}

JNIEXPORT jboolean JNICALL 
Java_netscape_rdf_core_NativeRDF_hasAssertion__Lnetscape_rdf_Resource_2Lnetscape_rdf_Resource_2IZ
	/* (Lnetscape/rdf/Resource;Lnetscape/rdf/Resource;IZ)Z */
(JNIEnv *ee, jobject this, jobject src, jobject arcLabel, jint target, jboolean isTrue) 
{
    void *nTarget = (void *)&target;
    return native_has_assertion(ee, this, src, arcLabel, RDF_INT_TYPE, nTarget, isTrue);
}

static jfieldID cursor_fid(JNIEnv *ee, jclass class)
{
    static jfieldID fid = NULL;
	/* its ok to cache this, but not the class itself */
	if (fid==NULL && class != NULL) {
	    fid = (*ee)->GetFieldID(ee, class, "cursor", "Ljava/lang/Double;");
	}
	return fid;
}

static jobject new_enumeration(JNIEnv *ee, RDF_Cursor cursor)
{
	jobject enumeration = NULL;
    jclass class = (*ee)->FindClass(ee, "netscape.rdf.core.NativeRDFEnumeration");

    if (class != NULL) {
        enumeration = (*ee)->AllocObject(ee, class);
        if (enumeration != NULL) {
            jfieldID fid = cursor_fid(ee, class);
            if (fid != NULL) {
                jdouble jhack;
                memcpy(&jhack, cursor, sizeof(jdouble));
                (*ee)->SetDoubleField(ee, enumeration, fid, jhack);
            }
        }
    }
    return enumeration;
}

JNIEXPORT jobject JNICALL 
Java_netscape_rdf_core_NativeRDF_getTargets
    /* (Lnetscape/rdf/Resource;Lnetscape/rdf/Resource;Z)Ljava/util/Enumeration */
(JNIEnv *ee, jobject this, jobject src, jobject arcLabel, jboolean isTrue) 
{
    RDF_Resource nSrc, narcLabel;
	RDF_Cursor cursor;
	jobject enumeration = NULL;

	nSrc = native_resource(ee, src);
	narcLabel = native_resource(ee, arcLabel);

	/* XXX API mismatch */
	cursor = RDF_GetTargets (db, nSrc, narcLabel, RDF_RESOURCE_TYPE, jni2prbool(isTrue));
	if (cursor != NULL) {
        enumeration = new_enumeration(ee, cursor);
	}
	return enumeration;
}

JNIEXPORT jobject JNICALL 
Java_netscape_rdf_core_NativeRDF_getTarget
    /* (Lnetscape/rdf/Resource;Lnetscape/rdf/Resource;Z)Ljava/lang/Object */
(JNIEnv *ee, jobject this, jobject src, jobject arcLabel, jboolean isTrue) 
{
    /* XXX API mismatch */
	return NULL;
}

static jobject 
native_get_sources(JNIEnv *ee, jobject this, jobject arcLabel, RDF_ValueType targetType, void *nTarget, jboolean isTrue)
{
    RDF_Resource narcLabel;
	RDF_Cursor cursor;
    jobject enumeration = NULL;
    
	narcLabel = native_resource(ee, arcLabel);
	if(narcLabel && nTarget) {
        cursor = RDF_GetSources(db, nTarget, narcLabel, targetType, jni2prbool(isTrue));
        if (cursor != NULL) {
            enumeration = new_enumeration(ee, cursor);
        }
    }	
    return enumeration;
}

JNIEXPORT jobject JNICALL 
Java_netscape_rdf_core_NativeRDF_getSources__Lnetscape_rdf_Resource_2Lnetscape_rdf_Resource_2Z
    /* (Lnetscape/rdf/Resource;Lnetscape/rdf/Resource;Z)Ljava/util/Enumeration */
(JNIEnv *ee, jobject this, jobject arcLabel, jobject target, jboolean isTrue) 
{
    void *nTarget = (void *)native_resource(ee, target);
    return native_get_sources(ee, this, arcLabel, RDF_RESOURCE_TYPE, nTarget, isTrue);
}

JNIEXPORT jobject JNICALL 
Java_netscape_rdf_core_NativeRDF_getSources__Lnetscape_rdf_Resource_2IZ
	/* (Lnetscape/rdf/Resource;IZ)Ljava/util/Enumeration */
(JNIEnv *ee, jobject this, jobject arcLabel, jint target, jboolean isTrue) 
{
    void *nTarget = (void *)&target;
    return native_get_sources(ee, this, arcLabel, RDF_INT_TYPE, nTarget, isTrue);
}

JNIEXPORT jobject JNICALL 
Java_netscape_rdf_core_NativeRDF_getSources__Lnetscape_rdf_Resource_2Ljava_lang_String_2Z
    /* (Lnetscape/rdf/Resource;Ljava/lang/String;Z)Ljava/util/Enumeration */
(JNIEnv *ee, jobject this, jobject arcLabel, jstring target, jboolean isTrue) 
{
    jobject v;
    const char *nTarget = (*ee)->GetStringUTFChars(ee, target, 0);
    v =  native_get_sources(ee, this, arcLabel, RDF_STRING_TYPE, (void *)nTarget, isTrue);
	(*ee)->ReleaseStringUTFChars(ee, target, nTarget);
	return v;
}

static jobject 
native_get_source(JNIEnv *ee, jobject this, jobject arcLabel, RDF_ValueType targetType, void *nTarget, jboolean isTrue)
{
    RDF_Resource narcLabel;
    jobject resource = NULL;
    
	narcLabel = native_resource(ee, arcLabel);
	if(narcLabel && nTarget) {
        /* XXX API mismatch */
        RDF_Cursor cursor = RDF_GetSources(db, nTarget, narcLabel, targetType, jni2prbool(isTrue));
        if (cursor != NULL) {
            /* XXX no constraint */
            RDF_Resource first = RDF_NextValue(cursor);
            if (first != NULL) {
                resource = java_resource(ee, first);
            }
            RDF_DisposeCursor(cursor);
        }
    }
    return resource;
}

JNIEXPORT jobject JNICALL 
Java_netscape_rdf_core_NativeRDF_getSource__Lnetscape_rdf_Resource_2Lnetscape_rdf_Resource_2Z
    /* (Lnetscape/rdf/Resource;Lnetscape/rdf/Resource;Z)Lnetscape/rdf/Resource */
(JNIEnv *ee, jobject this, jobject arcLabel, jobject target, jboolean isTrue) 
{
    void *nTarget = (void *)native_resource(ee, target);
    return native_get_source(ee, this, arcLabel, RDF_RESOURCE_TYPE, nTarget, isTrue);
}

JNIEXPORT jobject JNICALL 
Java_netscape_rdf_core_NativeRDF_getSource__Lnetscape_rdf_Resource_2IZ
	/* (Lnetscape/rdf/Resource;IZ)Lnetscape/rdf/Resource */
(JNIEnv *ee, jobject this, jobject arcLabel, jint target, jboolean isTrue) 
{
    void *nTarget = (void *)&target;
    return native_get_source(ee, this, arcLabel, RDF_INT_TYPE, nTarget, isTrue);
}

JNIEXPORT jobject JNICALL 
Java_netscape_rdf_core_NativeRDF_getSource__Lnetscape_rdf_Resource_2Ljava_lang_String_2Z
    /* (Lnetscape/rdf/Resource;Ljava/lang/String;Z)Lnetscape/rdf/Resource */
(JNIEnv *ee, jobject this, jobject arcLabel, jstring target, jboolean isTrue) 
{
    jobject v;
    const char *nTarget = (*ee)->GetStringUTFChars(ee, target, 0);
    v =  native_get_source(ee, this, arcLabel, RDF_STRING_TYPE, (void *)nTarget, isTrue);
	(*ee)->ReleaseStringUTFChars(ee, target, nTarget);
	return v;
}

JNIEXPORT jboolean JNICALL 
Java_netscape_rdf_core_NativeRDF_addNotifiable
    /* (Lnetscape/rdf/IRDFNotifiable;Lnetscape/rdf/RDFEvent;) */
(JNIEnv *ee, jobject this, jobject observer, jobject event) 
{
    /* XXX not implemented yet */
	return JNI_FALSE;
}

JNIEXPORT void JNICALL 
Java_netscape_rdf_core_NativeRDF_deleteNotifiable
    /* (Lnetscape/rdf/IRDFNotifiable;Lnetscape/rdf/RDFEvent;) */
(JNIEnv *ee, jobject this, jobject observer, jobject event) 
{
    /* XXX not implemented yet */
	return;
}

JNIEXPORT jboolean JNICALL 
Java_netscape_rdf_core_NativeRDFEnumeration_hasMoreElements
    /* ()Z */
(JNIEnv *ee, jobject this) 
{
    RDF_Cursor cursor = NULL;
	/* the fid has to exist because the enumeration was constructed already */
	jfieldID fid = cursor_fid(ee, NULL);
	if (fid != NULL) {
	    cursor = (RDF_Cursor)(*ee)->GetObjectField(ee, this, fid);
	}
	/* XXX need to know for sure */
	return JNI_TRUE;
}

JNIEXPORT jobject JNICALL 
Java_netscape_rdf_core_NativeRDFEnumeration_nextElement
    /* ()Ljava/lang/Object */
(JNIEnv *ee, jobject this)
{
    jobject resource = NULL;
    RDF_Cursor cursor = NULL;
	/* the fid has to exist because the enumeration was constructed already */
	jfieldID fid = cursor_fid(ee, NULL);
	if (fid != NULL) {
	    cursor = (RDF_Cursor)(*ee)->GetObjectField(ee, this, fid);
		if (cursor != NULL) {
		    /* XXX cursor is not constrained */
		    void *datum = RDF_NextValue(cursor);
		}
	}
	/* XXX RDF_DisposeCursor? */
	return resource;
}

#endif
