/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#include "prtypes.h"
#include "prmem.h"
#include "xp_mcom.h"
#include "nsVersionRegistry.h"
#include "NSReg.h"
#include "VerReg.h"
#include "prefapi.h"

PR_BEGIN_EXTERN_C

/* PUBLIC METHODS */

/**
 * Return the physical disk path for the specified component.
 *
 * Caller should free the returned string 
 *
 * @param   component  Registry path of the item to look up in the Registry
 * @return  The disk path for the specified component; NULL indicates error
 * @see     VersionRegistry#checkComponent
 */
char* nsVersionRegistry::componentPath( char* component )
{
  char     pathbuf[MAXREGPATHLEN];
  REGERR   status;
  char*    ret_val = NULL;
  
  /* If conversion is successful */
  if ( component != NULL ) {
    /* get component path from the registry */
    status = VR_GetPath( component, MAXREGPATHLEN, pathbuf );
    
    /* if we get a path */
    if ( status == REGERR_OK ) {
      ret_val = XP_STRDUP(pathbuf);
    }
  }
  return ret_val;
}

/**
 * Return the version information for the specified component.
 * @param   component  Registry path of the item to look up in the Registry
 * @return  A VersionInfo object for the specified component; 
 *          NULL indicates error
 * @see     VersionRegistry#checkComponent
 */
nsVersionInfo* nsVersionRegistry::componentVersion( char* component )
{
  REGERR         status;
  VERSION        cVersion;
  nsVersionInfo* verInfo = NULL;
  
  /* if conversion is successful */
  if ( component != NULL ) {
    /* get component version from the registry */
    status = VR_GetVersion( component, &cVersion );
    
    /* if we got the version */
    if ( status == REGERR_OK ) {
      verInfo = new nsVersionInfo(cVersion.major, 
                                  cVersion.minor, 
                                  cVersion.release,
                                  cVersion.build, 
                                  cVersion.check);
    }
  }
  return verInfo;
}

/**
 * Return the default directory.
 *
 * Caller should free the returned string 
 *
 */
char* nsVersionRegistry::getDefaultDirectory( char* component )
{
  char     pathbuf[MAXREGPATHLEN];
  REGERR   status;
  char*    ret_val = NULL;
  
  /* If conversion is successful */
  if ( component != NULL ) {
    /* get component path from the registry */
    status = VR_GetDefaultDirectory( component, MAXREGPATHLEN, pathbuf );
    
    /* if we get a path */
    if ( status == REGERR_OK ) {
      ret_val = XP_STRDUP(pathbuf);
    }
  }
  return ret_val;
}

PRInt32 nsVersionRegistry::setDefaultDirectory( char* component, 
                                                char* directory )
{
  REGERR   status = REGERR_FAIL;
  
  /* If conversion is successful */
  if ( component != NULL && directory != NULL ) {
    /* get component path from the registry */
    status = VR_SetDefaultDirectory( component, directory );
  }
  
  return (status);
}

PRInt32 nsVersionRegistry::installComponent( char* component, 
                                             char* path, 
                                             nsVersionInfo* version )
{
  char *   szVersion = NULL;
  REGERR   status = REGERR_FAIL;
  
  if (version != NULL) {
    szVersion = version->toString();
  }
  
  if ( component != NULL ) {
    /* call registry with converted data */
    /* XXX need to allow Directory installs also, change in Java */
    status = VR_Install( component, path, szVersion, 0 );
  }
  
  PR_FREEIF(szVersion);
  return (status);
}

PRInt32 nsVersionRegistry::installComponent( char* name, 
                                             char* path, 
                                             nsVersionInfo* version, 
                                             PRInt32 refCount )
{
  int  err = installComponent( name, path, version );
  
  if ( err == REGERR_OK )
    err = setRefCount( name, refCount );
  return err;
}

/**
 * Delete component and all sub-components.
 * @param   component  Registry path of the item to delete
 * @return  Error code
 */
PRInt32 nsVersionRegistry::deleteComponent( char* component )
{
  REGERR   status = REGERR_FAIL;
  
  /* If conversion is successful */
  if ( component != NULL ) {
    /* delete entry from registry */
    status = VR_Remove( component );
  }
  /* return status */
  return (status);
}

/**
 * Check the status of a named components.
 * @param   component  Registry path of the item to check
 * @return  Error code.  REGERR_OK means the named component was found in
 * the registry, the filepath referred to an existing file, and the
 * checksum matched the physical file. Other error codes can be used to
 * distinguish which of the above was not true.
 */
PRInt32 nsVersionRegistry::validateComponent( char* component )
{
  if ( component == NULL ) {
    return REGERR_FAIL;
  }
  
  return ( VR_ValidateComponent( component ) );
}

/**
 * verify that the named component is in the registry (light-weight
 * version of validateComponent since it does no disk access).
 * @param  component   Registry path of the item to verify
 * @return Error code. REGERR_OK means it is in the registry. 
 * REGERR_NOFIND will usually be the result otherwise.
 */
PRInt32 nsVersionRegistry::inRegistry( char* component )
{
  if ( component == NULL ) {
    return REGERR_FAIL;
  }
  
  return ( VR_InRegistry( component ) );
}

/**
 * Closes the registry file.
 * @return  Error code
 */
PRInt32 nsVersionRegistry::close()
{
  return ( VR_Close() );
}

/**
 * Returns an enumeration of the Version Registry contents.  Use the
 * enumeration methods of the returned object to fetch individual
 * elements sequentially.
 */
void* nsVersionRegistry::elements()
{
  /* XXX: This method is not called anywhere. Who uses it? */
  PR_ASSERT(PR_FALSE);
  return NULL;
}

/**
 * Set the refcount of a named component.
 * @param   component  Registry path of the item to check
 * @param   refcount  value to be set
 * @return  Error code
 */
PRInt32 nsVersionRegistry::setRefCount( char* component, PRInt32 refcount )
{
  if ( component == NULL ) {
    return REGERR_FAIL;
  }
  
  return VR_SetRefCount( component, refcount );
}

/**
 * Return the refcount of a named component.
 * @param   component  Registry path of the item to check
 * @return  the value of refCount
 */
PRInt32 nsVersionRegistry::getRefCount( char* component )
{
  REGERR status;
  int cRefCount = 0;
  
  if ( component != NULL ) {
    
    status = VR_GetRefCount( component, &cRefCount );
    if ( status != REGERR_OK ) {
      cRefCount = 0;
    }
  }
  
  return cRefCount;
}

/**
 * Creates a node for the item in the Uninstall list.
 * @param   regPackageName  Registry name of the package we are installing
 * @return  userPackagename User-readable package name
 * @return  Error code
 */
PRInt32 nsVersionRegistry::uninstallCreate( char* regPackageName, 
                                            char* userPackageName )
{
  char* temp = convertPackageName(regPackageName);
  regPackageName = temp;
  PRInt32 ret_val = uninstallCreateNode(regPackageName, userPackageName);
  XP_FREEIF(temp);
  return ret_val;
}

PRInt32 nsVersionRegistry::uninstallCreateNode( char* regPackageName, char* userPackageName )
{
  if ( regPackageName == NULL ) {
    return REGERR_FAIL;
  }
  
  if ( userPackageName == NULL ) {
    return REGERR_FAIL;
  }
  
  return VR_UninstallCreateNode( regPackageName, userPackageName );
}

/**
 * Adds the file as a property of the Shared Files node under the appropriate 
 * packageName node in the Uninstall list.
 * @param   regPackageName  Registry name of the package installed
 * @param   vrName  registry name of the shared file
 * @return  Error code
 */
PRInt32 nsVersionRegistry::uninstallAddFile( char* regPackageName, char* vrName )
{
  char* temp = convertPackageName(regPackageName);
  regPackageName = temp;
  PRInt32 ret_val = uninstallAddFileToList(regPackageName, vrName);
  XP_FREEIF(temp);
  return ret_val;
}

PRInt32 nsVersionRegistry::uninstallAddFileToList( char* regPackageName, char* vrName )
{
  if ( regPackageName == NULL ) {
    return REGERR_FAIL;
  }
  
  if ( vrName == NULL ) {
    return REGERR_FAIL;
  }
  
  return VR_UninstallAddFileToList( regPackageName, vrName );
}

/**
 * Checks if the shared file exists in the uninstall list of the package
 * @param   regPackageName  Registry name of the package installed
 * @param   vrName  registry name of the shared file
 * @return true or false 
 */
PRInt32 nsVersionRegistry::uninstallFileExists( char* regPackageName, char* vrName )
{
  char* temp = convertPackageName(regPackageName);
  regPackageName = temp;
  PRInt32 ret_val = uninstallFileExistsInList(regPackageName, vrName);
  XP_FREEIF(temp);
  return ret_val;
}

PRInt32 nsVersionRegistry::uninstallFileExistsInList( char* regPackageName, char* vrName )
{
  if ( regPackageName == NULL ) {
    return REGERR_FAIL;
  }
  
  if ( vrName == NULL ) {
    return REGERR_FAIL;
  }
  
  return VR_UninstallFileExistsInList( regPackageName, vrName );
}

char* nsVersionRegistry::getUninstallUserName( char* regName )
{
  REGERR  status = REGERR_FAIL;
  char    buf[MAXREGNAMELEN];
  char*   ret_val = NULL;
  
  if ( regName != NULL ) {
    status = VR_GetUninstallUserName( regName, buf, sizeof(buf) );
    PR_ASSERT(PR_FALSE);
    
    if ( status == REGERR_OK )
      ret_val = XP_STRDUP(buf);
  }
  
  return ret_val;
}


/* PRIVATE METHODS */

/**
 * This class is simply static function wrappers; don't "new" one
 */
nsVersionRegistry::nsVersionRegistry()
{
}

/**
 * Replaces all '/' with '_',in the given char*.If an '_' already exists in the
 * given char*, it is escaped by adding another '_' to it.
 * @param   regPackageName  Registry name of the package we are installing
 * @return  modified char*
 */
char* nsVersionRegistry::convertPackageName( char* regPackageName )
{
  if (regPackageName == NULL)
    return regPackageName;
  char* convertedPackageName;
  PRBool bSharedUninstall = PR_FALSE;
  PRUint32 i=0;
  PRUint32 j= 0;
  PRUint32 len = XP_STRLEN(regPackageName);
  PRUint32 new_size;
  
  if (regPackageName[0] == '/')
    bSharedUninstall = PR_TRUE;
  
  /* Count the number of '_' and then allocate that many extra characters */
  for (i=0; i < len; i++) {
    if (regPackageName[i] == '_') 
      j++;
  }

  new_size = len+j+1;
  convertedPackageName = (char*)XP_CALLOC(new_size, sizeof(char));
  i = j = 0;
  for (i=0; i < len; i++) {
    PR_ASSERT(j < new_size);
    char c = regPackageName[i];
    if (c == '/') 
      convertedPackageName[j] = '_';
    else 
      convertedPackageName[j] = c;
    j++;
    if (regPackageName[i] == '_') {
      PR_ASSERT(j < new_size);
      convertedPackageName[j++] = '_';
    }
  }
  PR_ASSERT(j == new_size);

  if (bSharedUninstall) {
    convertedPackageName[0] = '/';
  }
  return convertedPackageName;
}


PR_END_EXTERN_C
