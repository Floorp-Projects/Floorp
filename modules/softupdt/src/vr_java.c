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
 *   Native implementation for the netscape.softupdate.VersionRegistry,
 *   netscape.softupdate.Registry classes and helper classes
 */

#define IMPLEMENT_netscape_softupdate_VersionRegistry
#define IMPLEMENT_netscape_softupdate_VerRegEnumerator
#define IMPLEMENT_netscape_softupdate_VersionInfo
#define IMPLEMENT_netscape_softupdate_Registry
#define IMPLEMENT_netscape_softupdate_RegistryNode
#define IMPLEMENT_netscape_softupdate_RegKeyEnumerator
#define IMPLEMENT_netscape_softupdate_RegEntryEnumerator
#define IMPLEMENT_netscape_softupdate_RegistryException

#ifndef XP_MAC
#include "_jri/netscape_softupdate_VersionRegistry.c"
#include "_jri/netscape_softupdate_VerRegEnumerator.c"
#include "_jri/netscape_softupdate_VersionInfo.c"
#include "_jri/netscape_softupdate_Registry.c"
#include "_jri/netscape_softupdate_RegistryNode.c"
#include "_jri/netscape_softupdate_RegKeyEnumerator.c"
#include "_jri/netscape_softupdate_RegEntryEnumerator.c"
#include "_jri/netscape_softupdate_RegistryException.c"
#else
#include "n_softupdate_VersionRegistry.c"
#include "n_softupdate_VerRegEnumerator.c"
#include "n_softupdate_VersionInfo.c"
#include "netscape_softupdate_Registry.c"
#include "n_softupdate_RegistryNode.c"   
#include "n_softupdate_RegKeyEnumerator.c"
#include "n_s_RegEntryEnumerator.c"
#include "n_s_RegistryException.c"
#endif

#ifndef XP_MAC
#include "_jri/java_lang_Integer.c"
#else
#include "java_lang_Integer.c"
#endif

#include "xp_mcom.h"
#include "NSReg.h"
#include "VerReg.h"
#include "prefapi.h"

void SU_Reg_Initialize(JRIEnv* env)
{
   use_netscape_softupdate_VersionRegistry(env);
   use_netscape_softupdate_VerRegEnumerator(env);
   use_netscape_softupdate_VersionInfo(env);
   use_netscape_softupdate_Registry(env);
   use_netscape_softupdate_RegistryNode(env);
   use_netscape_softupdate_RegKeyEnumerator(env);
   use_netscape_softupdate_RegEntryEnumerator(env);
   use_netscape_softupdate_RegistryException(env);
   use_java_lang_Integer(env);
}

/* ------------------------------------------------------------------
 * VersionRegistry native methods
 * ------------------------------------------------------------------
 */

/*
 *  VersionRegistry::componentPath()
 */
JRI_PUBLIC_API(struct java_lang_String *)
native_netscape_softupdate_VersionRegistry_componentPath(
   JRIEnv*                     env, 
   struct java_lang_Class*     clazz,
   struct java_lang_String*    component )
{
   char*    szComponent;
   char     pathbuf[MAXREGPATHLEN];
   REGERR   status;

   /* (return NULL if an error occurs) */
   struct java_lang_String *javaPath = NULL;

   /* convert component from a Java string to a C String */
   szComponent = (char*)JRI_GetStringUTFChars( env, component );

   /* If conversion is successful */
   if ( szComponent != NULL ) {
      /* get component path from the registry */
      status = VR_GetPath( szComponent, MAXREGPATHLEN, pathbuf );
   
      /* if we get a path */
      if ( status == REGERR_OK ) {
         /* convert native path string to a Java String */
         javaPath = JRI_NewStringPlatform( env, pathbuf, XP_STRLEN(pathbuf), "", 0 );
      }
   }
   return (javaPath);
}



/*
 *  VersionRegistry::getDefaultDirectory()
 */
JRI_PUBLIC_API(struct java_lang_String *)
native_netscape_softupdate_VersionRegistry_getDefaultDirectory(
   JRIEnv*                     env, 
   struct java_lang_Class*     clazz, 
   struct java_lang_String*    component )
{
   char*    szComponent = NULL;
   char     pathbuf[MAXREGPATHLEN];
   REGERR   status;

   /* (return NULL if an error occurs) */
   struct java_lang_String *javaPath = NULL;

   /* convert component from a Java string to a C String */
   if (component)
	   szComponent = (char*)JRI_GetStringUTFChars( env, component );

   /* If conversion is successful */
   if ( szComponent != NULL ) {
      /* get component path from the registry */
      status = VR_GetDefaultDirectory( szComponent, MAXREGPATHLEN, pathbuf );
   
      /* if we get a path */
      if ( status == REGERR_OK ) {
         /* convert native path string to a Java String */
         javaPath = JRI_NewStringPlatform( env, pathbuf, XP_STRLEN(pathbuf), "", 0 );
      }
   }
   return (javaPath);
}



/*
 *  VersionRegistry::setDefaultDirectory()
 */
JRI_PUBLIC_API(jint)
native_netscape_softupdate_VersionRegistry_setDefaultDirectory(
   JRIEnv*                     env,
   struct java_lang_Class*     clazz,
   struct java_lang_String*    component,
   struct java_lang_String*    directory )
{
   char*    szComponent;
   char*    szDirectory;
   REGERR   status = REGERR_FAIL;

   /* convert component from a Java string to a UTF String for registry */
   szComponent = (char*)JRI_GetStringUTFChars( env, component );
   /* convert directory to a string in the platform charset */
   szDirectory = (char*)JRI_GetStringPlatformChars( env, directory, "", 0 );

   /* If conversion is successful */
   if ( szComponent != NULL && szDirectory != NULL ) {
      /* get component path from the registry */
      status = VR_SetDefaultDirectory( szComponent, szDirectory );
   }

   return (status);
}



/*
 *  VersionRegistry::componentVersion()
 */
JRI_PUBLIC_API(struct netscape_softupdate_VersionInfo *)
native_netscape_softupdate_VersionRegistry_componentVersion(
   JRIEnv*                     env, 
   struct java_lang_Class*     clazz, 
   struct java_lang_String*    component )
{
   char*          szComponent;
   REGERR         status;
   VERSION        cVersion;

   /* (return NULL if an error occurs) */
   struct netscape_softupdate_VersionInfo * javaVersion = NULL;

   /* convert component from a Java string to a C String */
   szComponent = (char*)JRI_GetStringUTFChars( env, component );

   /* if conversion is successful */
   if ( szComponent != NULL ) {
      /* get component version from the registry */
      status = VR_GetVersion( szComponent, &cVersion );
   
      /* if we got the version */
      if ( status == REGERR_OK ) {
         /* convert to a java VersionInfo structure */
         javaVersion = netscape_softupdate_VersionInfo_new_1(
            env,
            class_netscape_softupdate_VersionInfo(env),
            cVersion.major, 
            cVersion.minor, 
            cVersion.release,
            cVersion.build, 
            cVersion.check);
      }
   }
   return (javaVersion);
}



/*
 *  VersionRegistry::installComponent()
 */
JRI_PUBLIC_API(jint)
native_netscape_softupdate_VersionRegistry_installComponent(
   JRIEnv*                                   env, 
   struct java_lang_Class*                   clazz, 
   struct java_lang_String*                  component, 
   struct java_lang_String*                  path, 
   struct netscape_softupdate_VersionInfo*   version )
{
   char *   szComponent = NULL;
   char *   szPath = NULL;
   char *   szVersion = NULL;
   REGERR   status = REGERR_FAIL;
   struct java_lang_String *jVersion = NULL;

   /* convert java component to UTF for registry name */
   szComponent = (char*)JRI_GetStringUTFChars( env, component );
   /* convert path to native OS charset */
   szPath      = (char*)JRI_GetStringPlatformChars( env, path, "", 0 );

   if (version != NULL) {
      jVersion = netscape_softupdate_VersionInfo_toString( env, version );
      szVersion = (char*)JRI_GetStringUTFChars( env, jVersion );
   }

   /* if java-to-C conversion was successful */
   if ( szComponent != NULL ) {
      /* call registry with converted data */
      /* XXX need to allow Directory installs also, change in Java */
      status = VR_Install( szComponent, szPath, szVersion, 0 );
   }

   return (status);
}



/*
 *  VersionRegistry::deleteComponent()
 */
JRI_PUBLIC_API(jint)
native_netscape_softupdate_VersionRegistry_deleteComponent(
   JRIEnv*                    env,
   struct java_lang_Class*    clazz,
   struct java_lang_String*   component )
{
   char*    szComponent;
   REGERR   status = REGERR_FAIL;

   /* convert component from a Java string to a C String */
   szComponent = (char*)JRI_GetStringUTFChars( env, component );

   /* If conversion is successful */
   if ( szComponent != NULL ) {
      /* delete entry from registry */
      status = VR_Remove( szComponent );
   }
   /* return status */
   return (status);
}



/*
 *  VersionRegistry::validateComponent()
 */
JRI_PUBLIC_API(jint)
native_netscape_softupdate_VersionRegistry_validateComponent(
   JRIEnv*                    env,
   struct java_lang_Class*    clazz,
   struct java_lang_String*   component )
{
   char* szComponent = (char*)JRI_GetStringUTFChars( env, component );

   if ( szComponent == NULL ) {
      return REGERR_FAIL;
   }

   return ( VR_ValidateComponent( szComponent ) );
}



/*
 *  VersionRegistry::inRegistry()
 */
JRI_PUBLIC_API(jint)
native_netscape_softupdate_VersionRegistry_inRegistry(
   JRIEnv*                    env,
   struct java_lang_Class*    clazz,
   struct java_lang_String*   component )
{
   char* szComponent = (char*)JRI_GetStringUTFChars( env, component );

   if ( szComponent == NULL ) {
      return REGERR_FAIL;
   }

   return ( VR_InRegistry( szComponent ) );
}



/*
 *  VersionRegistry::close
 */
JRI_PUBLIC_API(jint)
native_netscape_softupdate_VersionRegistry_close(
   JRIEnv*                 env,
   struct java_lang_Class* clazz )
{
    return ( VR_Close() );
}

/*
 *  VersionRegistry::setRefCount()
 */
JRI_PUBLIC_API(jint)
native_netscape_softupdate_VersionRegistry_setRefCount(
   JRIEnv*                    env,
   struct java_lang_Class*    clazz,
   struct java_lang_String*   component, 
   jint                       refcount )
{
    char* szComponent = (char*)JRI_GetStringUTFChars( env, component );
    if ( szComponent == NULL ) {
        return REGERR_FAIL;
    }

    return VR_SetRefCount( szComponent, refcount );
}

/*
 *  VersionRegistry::getRefCount()
 */
JRI_PUBLIC_API(struct java_lang_Integer*)
native_netscape_softupdate_VersionRegistry_getRefCount(
   JRIEnv*                    env,
   struct java_lang_Class*    clazz,
   struct java_lang_String*   component )
{
    REGERR status;
    int cRefCount;

    char* szComponent = (char*)JRI_GetStringUTFChars( env, component );
    if ( szComponent != NULL ) {
        
        status = VR_GetRefCount( szComponent, &cRefCount );
        if ( status == REGERR_OK ) {
            return java_lang_Integer_new( env,
                class_java_lang_Integer(env),
                cRefCount);

        }
    }

    return NULL;
}

/*
 *  VersionRegistry::uninstallCreateNode()
 */
extern JRI_PUBLIC_API(jint)
native_netscape_softupdate_VersionRegistry_uninstallCreateNode(
   JRIEnv* env, 
   struct java_lang_Class* clazz, 
   struct java_lang_String *regPackageName, 
   struct java_lang_String *userPackageName)
{
    char* szRegPackageName = (char*)JRI_GetStringUTFChars( env, regPackageName );
    char* szUserPackageName = (char*)JRI_GetStringUTFChars( env, userPackageName );

    if ( szRegPackageName == NULL ) {
        return REGERR_FAIL;
    }

    if ( szUserPackageName == NULL ) {
        return REGERR_FAIL;
    }

    return VR_UninstallCreateNode( szRegPackageName, szUserPackageName );
}

/*
 *  VersionRegistry::uninstallAddFileToList()
 */
extern JRI_PUBLIC_API(jint)
native_netscape_softupdate_VersionRegistry_uninstallAddFileToList(
   JRIEnv* env, 
   struct java_lang_Class* clazz, 
   struct java_lang_String *regPackageName, 
   struct java_lang_String *vrName)
{
    char* szRegPackageName = (char*)JRI_GetStringUTFChars( env, regPackageName );
    char* szVrName = (char*)JRI_GetStringUTFChars( env, vrName );

    if ( szRegPackageName == NULL ) {
        return REGERR_FAIL;
    }

    if ( szVrName == NULL ) {
        return REGERR_FAIL;
    }

    return VR_UninstallAddFileToList( szRegPackageName, szVrName );
}

/*
 *  VersionRegistry::uninstallFileExistsInList()
 */
extern JRI_PUBLIC_API(jint)
native_netscape_softupdate_VersionRegistry_uninstallFileExistsInList(
JRIEnv* env,
struct java_lang_Class* clazz,
struct java_lang_String *regPackageName, 
struct java_lang_String *vrName)
{
    char* szRegPackageName = (char*)JRI_GetStringUTFChars( env, regPackageName );
    char* szVrName = (char*)JRI_GetStringUTFChars( env, vrName );

    if ( szRegPackageName == NULL ) {
        return REGERR_FAIL;
    }

    if ( szVrName == NULL ) {
        return REGERR_FAIL;
    }

    return VR_UninstallFileExistsInList( szRegPackageName, szVrName );
}

/* ------------------------------------------------------------------
 * VerRegEnumerator native methods
 * ------------------------------------------------------------------
 */

/*** private native regNext ()Ljava/lang/String; ***/
/*
 *  VerRegEnumerator::regNext
 */
JRIEnv* tmpEnv = NULL;

JRI_PUBLIC_API(struct java_lang_String *)
native_netscape_softupdate_VerRegEnumerator_regNext(
   JRIEnv*                 env,
   struct netscape_softupdate_VerRegEnumerator* self )
{
   REGERR                     status  = REGERR_FAIL;
   char                       pathbuf[MAXREGPATHLEN+1] = {0};
   char*                      pszPath = NULL;
   struct java_lang_String*   javaPath = NULL;
   REGENUM                    state = 0;

   tmpEnv = env;
   /* convert path to C string */
   pszPath = (char*)JRI_GetStringUTFChars( env,
      get_netscape_softupdate_VerRegEnumerator_path( env, self ) );

   state = get_netscape_softupdate_VerRegEnumerator_state( env, self );

   if ( pszPath != NULL ) {
      XP_STRCPY( pathbuf, pszPath );

      /* Get next path from Registry */
      status = VR_Enum( NULL, &state, pathbuf, MAXREGPATHLEN );

      /* if we got a good path */
      if (status == REGERR_OK) {
         /* convert new path back to java */
         javaPath = JRI_NewStringUTF( tmpEnv, pathbuf, XP_STRLEN(pathbuf) );
         set_netscape_softupdate_VerRegEnumerator_state( env, self, state );
      }
   }

   return (javaPath);
}


/* ------------------------------------------------------------------
 * Registry native methods
 * ------------------------------------------------------------------
 */

/*** private native nOpen ()I ***/
JRI_PUBLIC_API(jint)
native_netscape_softupdate_Registry_nOpen(
    JRIEnv* env,
    struct netscape_softupdate_Registry* self)
{
    char*   pFilename;
    HREG    hReg;
    REGERR  status = REGERR_FAIL;

    struct  java_lang_String *filename;

    filename = get_netscape_softupdate_Registry_regName( env, self);
    hReg = (HREG)get_netscape_softupdate_Registry_hReg( env, self );

    /* Registry must not be already open */
    if ( hReg == NULL ) {

        pFilename = (char*)JRI_GetStringPlatformChars( env, filename, "", 0 );

        if ( pFilename != NULL ) {

            status = NR_RegOpen( pFilename, &hReg );

            if ( REGERR_OK == status ) {
                set_netscape_softupdate_Registry_hReg( env, self, (jint)hReg );
            }
        }
    }
    return (status);
}



/*** private native nClose ()I ***/
JRI_PUBLIC_API(jint)
native_netscape_softupdate_Registry_nClose(
    JRIEnv* env,
    struct netscape_softupdate_Registry* self)
{
    REGERR  status = REGERR_FAIL;
    HREG    hReg   = (HREG)get_netscape_softupdate_Registry_hReg( env, self );

    /* Registry must not be already closed */
    if ( hReg != NULL) {
        status = NR_RegClose( hReg );

        if ( REGERR_OK == status ) {
            set_netscape_softupdate_Registry_hReg( env, self, (jint)NULL );
        }
    }

    return (status);
}



/*** private native nAddKey (ILjava/lang/String;)I ***/
JRI_PUBLIC_API(jint)
native_netscape_softupdate_Registry_nAddKey(
    JRIEnv* env,
    struct netscape_softupdate_Registry* self,
    jint rootKey,
    struct java_lang_String *keyName)
{
    char*   pKey;
    REGERR  status = REGERR_FAIL;
    HREG    hReg   = (HREG)get_netscape_softupdate_Registry_hReg( env, self );

    pKey = (char*)JRI_GetStringUTFChars( env, keyName );

    if ( pKey != NULL ) {
        status = NR_RegAddKey( hReg, rootKey, pKey, NULL );
    }
    return(status);
}



/*** private native nDeleteKey (ILjava/lang/String;)I ***/
JRI_PUBLIC_API(jint)
native_netscape_softupdate_Registry_nDeleteKey(
    JRIEnv* env,
    struct netscape_softupdate_Registry* self,
    jint rootKey,
    struct java_lang_String *keyName)
{
    char*   pKey;
    REGERR  status = REGERR_FAIL;
    HREG    hReg   = (HREG)get_netscape_softupdate_Registry_hReg( env, self );

    pKey = (char*)JRI_GetStringUTFChars( env, keyName );

    if ( pKey != NULL ) {
        status = NR_RegDeleteKey( hReg, rootKey, pKey );
    }
    return(status);
}



/*** private native nGetKey (ILjava/lang/String;)Ljava/lang/Object; ***/
JRI_PUBLIC_API(struct netscape_softupdate_RegistryNode *)
native_netscape_softupdate_Registry_nGetKey(
    JRIEnv* env,
    struct netscape_softupdate_Registry* self,
    jint rootKey,
    struct java_lang_String *keyName,
    struct java_lang_String *target )
{
    char*                                   pKey;
    HREG                                    hReg;
    RKEY                                    newkey;
    REGERR                                  status = REGERR_FAIL;
    struct netscape_softupdate_RegistryNode  *regKey = NULL;

    hReg   = (HREG)get_netscape_softupdate_Registry_hReg( env, self );
    pKey = (char*)JRI_GetStringUTFChars( env, keyName );

    if ( pKey != NULL ) {
        status = NR_RegGetKey( hReg, rootKey, pKey, &newkey );

        if ( REGERR_OK == status ) {
            regKey = netscape_softupdate_RegistryNode_new(
                        env,
                        class_netscape_softupdate_RegistryNode(env),
                        self,
                        newkey,
                        target);
        }
        else {
            JRI_ThrowNew(env, 
                class_netscape_softupdate_RegistryException(env),
                "");
        }
    }
    return (regKey);
}



/*** private native nUserName ()Ljava/lang/String; ***/
JRI_PUBLIC_API(struct java_lang_String *)
native_netscape_softupdate_Registry_nUserName(
    JRIEnv* env, 
    struct netscape_softupdate_Registry* self)
{
    char*  profName;
    int    err;
    struct java_lang_String *jname = NULL;

    err = PREF_CopyDefaultCharPref( "profile.name", &profName );
    if (err == PREF_NOERROR ) {
        jname = JRI_NewStringPlatform(env, profName, XP_STRLEN(profName),"",0);
    }
    return jname;
}




/* ------------------------------------------------------------------
 * RegistryNode native methods
 * ------------------------------------------------------------------
 */

/*** public native deleteEntry (Ljava/lang/String;)I ***/
JRI_PUBLIC_API(jint)
native_netscape_softupdate_RegistryNode_nDeleteEntry(
    JRIEnv* env,
    struct netscape_softupdate_RegistryNode* self,
    struct java_lang_String *name)
{
    char*                                   pName;
    HREG                                    hReg;
    RKEY                                    key;
    REGERR                                  status = REGERR_FAIL;
    struct netscape_softupdate_Registry     *reg;

    reg   = get_netscape_softupdate_RegistryNode_reg( env, self );
    hReg  = (HREG)get_netscape_softupdate_Registry_hReg( env, reg );
    key   = get_netscape_softupdate_RegistryNode_key( env, self );
    pName = (char*)JRI_GetStringUTFChars( env, name );

    if ( pName != NULL && hReg != NULL ) {
        status = NR_RegDeleteEntry( hReg, key, pName );
    }
    return status;
}



/*** public native setEntry (Ljava/lang/String;Ljava/lang/String;)I ***/
JRI_PUBLIC_API(jint)
native_netscape_softupdate_RegistryNode_setEntryS(
    JRIEnv* env,
    struct netscape_softupdate_RegistryNode* self,
    struct java_lang_String *name,
    struct java_lang_String *value)
{
    char*                                   pName;
    char*                                   pValue;
    HREG                                    hReg;
    RKEY                                    key;
    REGERR                                  status = REGERR_FAIL;
    struct netscape_softupdate_Registry     *reg;

    reg    = get_netscape_softupdate_RegistryNode_reg( env, self );
    hReg   = (HREG)get_netscape_softupdate_Registry_hReg( env, reg );
    key    = get_netscape_softupdate_RegistryNode_key( env, self );
    pName  = (char*)JRI_GetStringUTFChars( env, name );
    pValue = (char*)JRI_GetStringUTFChars( env, value );

    if ( pName != NULL && pValue != NULL && hReg != NULL ) {
        status = NR_RegSetEntryString( hReg, key, pName, pValue );
    }
    return status;
}



/*** public native setEntry (Ljava/lang/String;[I)I ***/
JRI_PUBLIC_API(jint)
native_netscape_softupdate_RegistryNode_setEntryI(
    JRIEnv* env,
    struct netscape_softupdate_RegistryNode* self,
    struct java_lang_String *name,
    jintArray value)
{
    char*                                   pName;
    char*                                   pValue = NULL;
    uint32                                  datalen;
    HREG                                    hReg;
    RKEY                                    key;
    REGERR                                  status = REGERR_FAIL;
    struct netscape_softupdate_Registry     *reg;

    reg    = get_netscape_softupdate_RegistryNode_reg( env, self );
    hReg   = (HREG)get_netscape_softupdate_Registry_hReg( env, reg );
    key    = get_netscape_softupdate_RegistryNode_key( env, self );
    pName  = (char*)JRI_GetStringUTFChars( env, name );
    pValue = (char*)JRI_GetIntArrayElements( env, value );

    if ( pName != NULL && pValue != NULL && hReg != NULL ) {
        datalen = JRI_GetIntArrayLength( env, value );
        status = NR_RegSetEntry( hReg, key, pName,
            REGTYPE_ENTRY_INT32_ARRAY, pValue, datalen );
    }
    return status;
}



/*** public native setEntry (Ljava/lang/String;[B)I ***/
JRI_PUBLIC_API(jint)
native_netscape_softupdate_RegistryNode_setEntryB(
    JRIEnv* env,
    struct netscape_softupdate_RegistryNode* self,
    struct java_lang_String *name,
    jbyteArray value)
{
    char*                                   pName = NULL;
    char*                                   pValue = NULL;
    uint32                                  datalen;
    HREG                                    hReg;
    RKEY                                    key;
    REGERR                                  status = REGERR_FAIL;
    struct netscape_softupdate_Registry     *reg;

    reg    = get_netscape_softupdate_RegistryNode_reg( env, self );
    hReg   = (HREG)get_netscape_softupdate_Registry_hReg( env, reg );
    key    = get_netscape_softupdate_RegistryNode_key( env, self );
    pName  = (char*)JRI_GetStringUTFChars( env, name );
    pValue = (char*)JRI_GetByteArrayElements( env, value );

    if ( pName != NULL && pValue != NULL && hReg != NULL ) {
        datalen = JRI_GetByteArrayLength( env, value );
        status = NR_RegSetEntry( hReg, key, pName,
            REGTYPE_ENTRY_BYTES, pValue, datalen );
    }
    return status;
}



/*** public native getEntryType (Ljava/lang/String;)I ***/
JRI_PUBLIC_API(jint)
native_netscape_softupdate_RegistryNode_nGetEntryType(
    JRIEnv* env,
    struct netscape_softupdate_RegistryNode* self,
    struct java_lang_String *name)
{
    char*                                   pName;
    jint                                    type;
    HREG                                    hReg;
    RKEY                                    key;
    REGERR                                  status = REGERR_FAIL;
    REGINFO                                 info;
    struct netscape_softupdate_Registry     *reg;

    XP_ASSERT(REGTYPE_ENTRY_STRING_UTF == netscape_softupdate_Registry_TYPE_STRING);
    XP_ASSERT(REGTYPE_ENTRY_INT32_ARRAY == netscape_softupdate_Registry_TYPE_INT_ARRAY);
    XP_ASSERT(REGTYPE_ENTRY_BYTES == netscape_softupdate_Registry_TYPE_BYTES);

    reg    = get_netscape_softupdate_RegistryNode_reg( env, self );
    hReg   = (HREG)get_netscape_softupdate_Registry_hReg( env, reg );
    key    = get_netscape_softupdate_RegistryNode_key( env, self );
    pName  = (char*)JRI_GetStringUTFChars( env, name );

    if ( pName != NULL && hReg != NULL )
    {
        info.size = sizeof(REGINFO);
        status    = NR_RegGetEntryInfo( hReg, key, pName, &info );

        if ( REGERR_OK == status ) {
            type = info.entryType;
        }
    }

    if ( REGERR_OK == status )
        return type;
    else
        return ( -status );
}



/*** public native getEntry (Ljava/lang/String;)Ljava/lang/Object; ***/
JRI_PUBLIC_API(struct java_lang_Object *)
native_netscape_softupdate_RegistryNode_nGetEntry(
    JRIEnv* env,
    struct netscape_softupdate_RegistryNode* self,
    struct java_lang_String *name)
{
    char*                                   pName;
    void*                                   pValue;
    uint32                                  size;
    HREG                                    hReg;
    RKEY                                    key;
    REGINFO                                 info;
    REGERR                                  status = REGERR_FAIL;
    struct netscape_softupdate_Registry     *reg;
    struct java_lang_Object                 *valObj = NULL;

    reg    = get_netscape_softupdate_RegistryNode_reg( env, self );
    hReg   = (HREG)get_netscape_softupdate_Registry_hReg( env, reg );
    key    = get_netscape_softupdate_RegistryNode_key( env, self );
    pName  = (char*)JRI_GetStringUTFChars( env, name );

    if ( pName != NULL && hReg != NULL )
    {
        info.size = sizeof(REGINFO);
        status    = NR_RegGetEntryInfo( hReg, key, pName, &info );

        if ( REGERR_OK == status )
        {
            size = info.entryLength;
            pValue = XP_ALLOC(size);
            if ( pValue != NULL ) 
            {
                status = NR_RegGetEntry( hReg, key, pName, pValue, &size );
                if ( REGERR_OK == status )
                {
                    switch ( info.entryType )
                    {
                    case REGTYPE_ENTRY_STRING_UTF:
                        valObj = (struct java_lang_Object *)JRI_NewStringUTF( 
                            env, 
                            (char*)pValue, 
                            XP_STRLEN((char*)pValue) );
                        break;


                    case REGTYPE_ENTRY_INT32_ARRAY:
                        valObj = (struct java_lang_Object *)JRI_NewByteArray( 
                            env, 
                            size, 
                            (char*)pValue );
                        break;



                    case REGTYPE_ENTRY_BYTES:
                    default:         /* for unknown types we return raw bits */
                        valObj = (struct java_lang_Object *)JRI_NewByteArray( 
                            env, 
                            size, 
                            (char*)pValue );
                        break;
                    }
                }
    
                XP_FREE(pValue);
            }   /* pValue != NULL */
        }
    }
    return valObj;
}



/* ------------------------------------------------------------------
 * RegKeyEnumerator native methods
 * ------------------------------------------------------------------
 */

/*** private native regNext ()Ljava/lang/String; ***/
JRI_PUBLIC_API(struct java_lang_String *)
native_netscape_softupdate_RegKeyEnumerator_regNext(
    JRIEnv* env,
    struct netscape_softupdate_RegKeyEnumerator* self,
    jbool skip)
{
    HREG                                    hReg;
    RKEY                                    key;
    REGERR                                  status = REGERR_FAIL;
    REGENUM                                 state = 0;
    uint32                                  style;
    char                                    pathbuf[MAXREGPATHLEN+1] = {0};
    char*                                   pPath = NULL;
    struct netscape_softupdate_Registry     *reg;
    struct java_lang_String                 *path;
    struct java_lang_String                 *javaPath = NULL;

    reg    = get_netscape_softupdate_RegKeyEnumerator_reg( env, self );
    hReg   = (HREG)get_netscape_softupdate_Registry_hReg( env, reg );

    key    = get_netscape_softupdate_RegKeyEnumerator_key( env, self );
    state  = get_netscape_softupdate_RegKeyEnumerator_state( env, self );
    style  = get_netscape_softupdate_RegKeyEnumerator_style( env, self );

    /* convert path to C string */
    path = get_netscape_softupdate_RegKeyEnumerator_path( env, self );
    pPath = (char*)JRI_GetStringUTFChars( env, path );

    if ( pPath != NULL ) {
        XP_STRCPY( pathbuf, pPath );

        /* Get next path from Registry */
        status = NR_RegEnumSubkeys( hReg, key, &state, pathbuf, 
            sizeof(pathbuf), style );

        /* if we got a good path */
        if (status == REGERR_OK) {
            /* convert new path back to java and save state */
            javaPath = JRI_NewStringUTF( env, pathbuf, XP_STRLEN(pathbuf) );
            if ( skip ) {
                set_netscape_softupdate_RegKeyEnumerator_state( env, self, state );
            }
        }
    }

   return (javaPath);
}



/* ------------------------------------------------------------------
 * RegEntryEnumerator native methods
 * ------------------------------------------------------------------
 */

/*** private native regNext ()Ljava/lang/String; ***/
JRI_PUBLIC_API(struct java_lang_String *)
native_netscape_softupdate_RegEntryEnumerator_regNext(
    JRIEnv* env,
    struct netscape_softupdate_RegEntryEnumerator* self,
    jbool skip)
{
    HREG                                    hReg;
    RKEY                                    key;
    REGERR                                  status = REGERR_FAIL;
    REGENUM                                 state = 0;
    char                                    namebuf[MAXREGPATHLEN+1] = {0};
    char*                                   pName = NULL;
    struct netscape_softupdate_Registry     *reg;
    struct java_lang_String                 *name;
    struct java_lang_String                 *javaName = NULL;

    reg    = get_netscape_softupdate_RegEntryEnumerator_reg( env, self );
    hReg   = (HREG)get_netscape_softupdate_Registry_hReg( env, reg );

    key    = get_netscape_softupdate_RegEntryEnumerator_key( env, self );
    state  = get_netscape_softupdate_RegEntryEnumerator_state( env, self );

    /* convert name to C string */
    name = get_netscape_softupdate_RegEntryEnumerator_name( env, self );
    pName = (char*)JRI_GetStringUTFChars( env, name );

    if ( pName != NULL ) {
        XP_STRCPY( namebuf, pName );

        /* Get next name from Registry */
        status = NR_RegEnumEntries( hReg, key, &state, namebuf, 
            sizeof(namebuf), NULL );

        /* if we got a good name */
        if (status == REGERR_OK) {
            /* convert new name back to java and save state */
            javaName = JRI_NewStringUTF( env, namebuf, XP_STRLEN(namebuf) );
            if (skip)
                set_netscape_softupdate_RegEntryEnumerator_state( env, self, state );
        }
    }

   return (javaName);
}

