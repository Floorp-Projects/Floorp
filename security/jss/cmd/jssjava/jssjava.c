/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Security Services for Java.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/* 
 * Crypto VM - Java VM with statically linked crypto aka libsec routines. 
 * 
 * static routines are registered to VM using JNI API. 
 */

#include <jni.h>
#include "registerNatives.h"
#include "nspr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <svrplcy.h>

PR_IMPLEMENT( SVRPLCYPolicyType )
JSS_getExportControlPolicyType( void );

#if defined(_WINDOWS)
#define USAGE "Usage: %s [-version] [-debug] [-nojit] [-classpath classpath] [-ms<size>] [-mx<size>] [-D property] <java_class>\n"
#else
#define USAGE "Usage: %s [-version] [-debug] [-classpath classpath] [-ms<size>] [-mx<size>] [-D property] <java_class>\n"
#endif

/* Unique "jssjava" version information */
/* NOTE:  Must be changed for ALL new releases!!! */
#define JSSJAVA_MAJOR_VERSION "2"
#define JSSJAVA_MINOR_VERSION "1"
#define JDK_MAJOR_VERSION     "1.2"
#define JDK_MINOR_VERSION     "2"

/* args & options */
char *     prog_name = 0;
char *     classpath = 0;
char *     javaclass = 0;
char **    javaArgs = 0;
int        numJavaArgs = 0;
int        debug = 0;
int        jssjava_version = 0;

/* set property to not load jdkcertsec10 from the beg */
char **    userProps = 0;
int        numUserProps = 0;
static int maxUserProps = 0;


static void errExit(int exitCode)
{
#if defined(DEBUG) && defined(_WINDOWS)
	_sleep(10);
#endif
	exit(exitCode);
}

static void addUserProp(char *keyval)
{
	char **newUserProps = 0;
	char *val = 0;

	if (maxUserProps < numUserProps+2) {
		newUserProps = (char **)calloc(numUserProps+4, sizeof(char *));
		maxUserProps = numUserProps+4;
		memcpy(newUserProps, userProps, numUserProps*sizeof(char *));
		userProps = newUserProps;
	}
	if (val = (char *)strtok(keyval, (const char *)"=")) 
		*val++ = 0;
	userProps[numUserProps++] = keyval;
	userProps[numUserProps++] = val;
}


static void getArgs (int argc, char *argv[])
{
	int i,j;
        char* msptr;
        char* mxptr;

#ifdef DEBUG_nelsonb
	for (i = 0; i < argc; ++i) 
		puts(argv[i]);
#endif

	prog_name = *argv++; argc--;

	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-version")) {
			jssjava_version = 1;
		}
		else if (!strcmp(argv[i], "-debug")) {
			debug = 1;
		}
		else if (!strcmp(argv[i], "-classpath")) {
			if (++i == argc) 
				break;
			classpath = argv[i];
		}
		else if (!strcmp(argv[i], "-D")) {
			if (++i == argc) 
				break;
			addUserProp(argv[i]);
		}
#if defined(_WINDOWS)
		else if (!strcmp(argv[i], "-nojit")) {
			addUserProp(argv[i]);
		}
#endif
		else if (!strncmp(argv[i], "-ms", 3)) {
                        msptr = (char*)malloc(strlen(argv[i]) + 5);
                        sprintf(msptr, "-X%s", argv[i] + 1); /* skip '-' */
                        addUserProp(msptr);
                }
                else if (!strncmp(argv[i], "-mx", 3)) {
                        mxptr = (char*)malloc(strlen(argv[i]) + 5);
                        sprintf(mxptr, "-X%s", argv[i] + 1); /* skip '-' */
                        addUserProp(mxptr);
		} else if(!strcmp(argv[i], "-info")) {
			/* -info is a dummy argument where information can be placed
			 * that will show up in a ps listing.  Its argument is
			 * ignored. */
			if( i+1 < argc ) {
				++i;
			}
		} else {
			javaclass = argv[i++]; 
			break;
		}
	}
	if (jssjava_version == 1) {
		SVRPLCYPolicyType policy;
		char policyString[50];

		/* First, initialize export control policy information. */
		if( SVRPLCY_InstallUtilityPolicy() != PR_SUCCESS ) {
			errExit(-1);
		}

		/* Second, establish which export control policy is being used. */
		policy = JSS_getExportControlPolicyType();

		switch( policy ) {
			case SVRPLCYNull:
				strcpy( policyString, "null" );
				break;
			case SVRPLCYDomestic:
				strcpy( policyString, "domestic" );
				break;
			case SVRPLCYExport:
				strcpy( policyString, "export" );
				break;
			case SVRPLCYFrance:
				strcpy( policyString, "france" );
				break;
			default:
				strcpy( policyString, "none" );
				break;
		}

		/* Third, print the export control policy & version information: */
		printf( "%s version \"%s.%s\" [%s]\n"
				" (uses JDK \"%s.%s\")\n",
				prog_name, JSSJAVA_MAJOR_VERSION, JSSJAVA_MINOR_VERSION,
				policyString, JDK_MAJOR_VERSION, JDK_MINOR_VERSION );
		errExit(0);
	}
	if (javaclass == 0) {
		printf(USAGE, prog_name);
		errExit(1);
	}
	numJavaArgs = argc - i;
	if (numJavaArgs > 0) 
		javaArgs = &argv[i];

	for (j = strlen(javaclass)-1; j >= 0; j--) {
		if (javaclass[j] == '.')
			javaclass[j] = '/';
	}

}


static int setUserProps(JNIEnv *env)
{
    jclass system_cls;
	jobject system_props;
    jmethodID getprop_mid;
    jmethodID setprop_mid;
    jclass prop_cls;
    jmethodID put_mid;
	jstring key, val;
	jthrowable exc;
	int i;

	/* get java.lang.System class and its get/setProperties methods */
    system_cls = (*env)->FindClass(env, "java/lang/System");
    if (system_cls == 0) {
        fprintf(stderr, "Can't find java/lang/System.\n");
        return -1;
    }
    getprop_mid = (*env)->GetStaticMethodID(env, system_cls, "getProperties",
									"()Ljava/util/Properties;");
    if (getprop_mid == 0) {
        fprintf(stderr,"Can't find getProperties method in java.lang.System\n");
        return -1;
	}
    setprop_mid = (*env)->GetStaticMethodID(env, system_cls, "setProperties",
									"(Ljava/util/Properties;)V");
    if (setprop_mid == 0) {
        fprintf(stderr,"Can't find setProperties method in java.lang.System\n");
        return -1;
	}

	/* get system properties, java.util.Properties class and its put method */
    system_props = (*env)->CallStaticObjectMethod(env, system_cls, getprop_mid);
	exc = (*env)->ExceptionOccurred(env);
	if (exc) {
		(*env)->ExceptionDescribe(env);
		return -1;
	}
	prop_cls = (*env)->GetObjectClass(env, system_props);
	put_mid = (*env)->GetMethodID(env, prop_cls, "put", 
				"(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
	if (put_mid == 0) {
        fprintf(stderr, "Can't find put method in java.util.Properties\n");
        return -1;
	}

	/* add default prop to not load JSS library */
	key = (*env)->NewStringUTF(env, "jss.load");
	if (key == 0) {
		fprintf(stderr, "Out of memory\n");
		return -1;
	}
	val = (*env)->NewStringUTF(env, "no");
	if (val == 0) {
		fprintf(stderr, "Out of memory\n");
		return -1;
	}
	(void)(*env)->CallObjectMethod(env, system_props, put_mid, 
									(jobject)key, (jobject)val);
	exc = (*env)->ExceptionOccurred(env);
	if (exc) {
		(*env)->ExceptionDescribe(env);
		return -1;
	}

	/* add user properties */
	i = 0;
	while (i < numUserProps) {
		key = (*env)->NewStringUTF(env, userProps[i++]);
		if (key == 0) {
			fprintf(stderr, "Out of memory\n");
			errExit(1);
		}
		val = (*env)->NewStringUTF(env, userProps[i++]);
		if (val == 0) {
			fprintf(stderr, "Out of memory\n");
			errExit(1);
		}
		(void)(*env)->CallObjectMethod(env, system_props, put_mid, 
										(jobject)key, (jobject)val);
		exc = (*env)->ExceptionOccurred(env);
		if (exc) {
			(*env)->ExceptionDescribe(env);
			return -1;
		}
	}

	/* set new set of system properties */
    (*env)->CallStaticVoidMethod(env, system_cls, setprop_mid, system_props);
	exc = (*env)->ExceptionOccurred(env);
	if (exc) {
		(*env)->ExceptionDescribe(env);
		errExit(1);
	}

	return 0;
}


static jobjectArray setJavaArgs(JNIEnv *env)
{
	jclass jstr_cls;
    jstring jstr;
    jobjectArray java_args;
	int i;

	jstr_cls = (*env)->FindClass(env, "java/lang/String"); 
	if (numJavaArgs != 0) {
		jstr = (*env)->NewStringUTF(env, javaArgs[0]);
		if (jstr == 0) {
			fprintf(stderr, "Out of memory\n");
			return 0;
		}
		java_args = (*env)->NewObjectArray(env, numJavaArgs, jstr_cls, jstr);
		if (java_args == 0) {
			fprintf(stderr, "Out of memory\n");
			return 0;
		}
		for (i=1; i < numJavaArgs; i++) {
			jstr = (*env)->NewStringUTF(env, javaArgs[i]);
			if (jstr == 0) {
				fprintf(stderr, "Out of memory\n");
				return 0;
			}
			(*env)->SetObjectArrayElement(env, java_args, i, jstr);
		}
	}
	else {
		java_args = (*env)->NewObjectArray(env, 0, jstr_cls, 0);
		if (java_args == 0) {
			fprintf(stderr, "Out of memory\n");
			return 0;
		}
	}
	return java_args;
}


int main (int argc, char *argv[])
{
    JDK1_1InitArgs vm_args;
    JNIEnv *	 env;
    JavaVM *	 jvm;
    jclass       cls;
    jmethodID    mid;
	jobjectArray java_args;
	jthrowable   exc;
    jint         res;

	/* get options, check usage */
	getArgs(argc, argv);

	/* initialize VM args */
    /* IMPORTANT: specify vm_args version # for JDK1.1.2 and beyond */
    vm_args.version = 0x00010001;
    JNI_GetDefaultJavaVMInitArgs(&vm_args);

	/* set VM args from options */
	if (classpath || (classpath = getenv("CLASSPATH")))
		vm_args.classpath = classpath;
	if (debug) 
		vm_args.debugging = JNI_TRUE;

	/* create Java VM */
    res = JNI_CreateJavaVM(&jvm, &env, &vm_args);
    if (res < 0) {
        fprintf(stderr, "Can't create Java VM\n");
        errExit(1);
    }

	/* set additional system properties */
	if (setUserProps(env) < 0) {
		fprintf(stderr, "Error setting system properties.\n");
		errExit(1);
	}

	/* register all statically linked native methods. */
	if (registerNatives(env) < 0) {
        fprintf(stderr,"Error registering statically linked native methods\n");
        errExit(1);
	}

	/* set up java args */
	java_args = setJavaArgs(env);
	if (java_args == 0) {
		fprintf(stderr, "Error setting up arguments to Java class");
		errExit(1);
	}

	PR_Init( PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 0);

	/* find the java class & main method to invoke */
    cls = (*env)->FindClass(env, javaclass);
    if (cls == 0) {
        fprintf(stderr, "Can't find %s class\n", javaclass);
        errExit(1);
    }
    mid = (*env)->GetStaticMethodID(env,cls,"main","([Ljava/lang/String;)V");
    if (mid == 0) {
        fprintf(stderr, "Can't find %s.main\n", javaclass);
        errExit(1);
    }

	/* call java main */
    (*env)->CallStaticVoidMethod(env, cls, mid, java_args);
	exc = (*env)->ExceptionOccurred(env);
	if (exc) {
		(*env)->ExceptionDescribe(env);
	}

#ifdef DEBUG
    /* Garbage collect to run finalizers before exiting. Also, try to
     * verify the NativeProxy class, but keep in mind that it might not
     * be loaded at all. */
    {
    jclass systemClass;
    jclass nativeProxyClass;
    jmethodID gc;
    jmethodID finalize;
    jmethodID assertRegistryEmpty;

    /*
     * garbage collect
     */
    systemClass = (*env)->FindClass(env, "java/lang/System");
    PR_ASSERT(systemClass != NULL);

    /* This is hanging on Solaris for some reason :( */
#if 0
    gc = (*env)->GetStaticMethodID(env, systemClass, "gc", "()V");
    PR_ASSERT( gc != NULL);

    (*env)->CallStaticVoidMethod(env, systemClass, gc);
    PR_ASSERT( (*env)->ExceptionOccurred(env) == NULL );
#endif

	finalize = (*env)->GetStaticMethodID(env, systemClass, "runFinalization",
							"()V");
	PR_ASSERT( finalize != NULL );

	(*env)->CallStaticVoidMethod(env, systemClass, finalize);
    PR_ASSERT( (*env)->ExceptionOccurred(env) == NULL );

    /*
     * Make sure the registry is empty
     */
    nativeProxyClass = (*env)->FindClass(env,
        "org/mozilla/jss/util/NativeProxy");
    /* If it's NULL, don't worry, maybe they just aren't using the class */
    (*env)->ExceptionClear(env);

    if(nativeProxyClass != NULL) {
        /* OK, the class is loaded, so we should validate it */
        assertRegistryEmpty = (*env)->GetStaticMethodID(env, nativeProxyClass,
            "assertRegistryEmpty", "()V");
        PR_ASSERT(assertRegistryEmpty != NULL);
        (*env)->CallStaticVoidMethod(env, nativeProxyClass,
            assertRegistryEmpty);
        if( (*env)->ExceptionOccurred(env) != NULL ) {
			(*env)->ExceptionDescribe(env);
		}
    }

    }
#endif

    (*jvm)->DestroyJavaVM(jvm);


#if defined(DEBUG) && defined(WIN32)
	_sleep(10 * 1000);	// milliseconds
#endif
 	return 0;
}

