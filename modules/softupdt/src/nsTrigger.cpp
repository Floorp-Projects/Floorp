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

#include "prmem.h"
#include "prmon.h"
#include "prlog.h"
#include "xp.h"

#include "NSReg.h"
#include "softupdt.h"
#include "su_folderspec.h"
#define NEW_FE_CONTEXT_FUNCS
#include "net.h"
#include "libevent.h"
#include "prefapi.h"

#include "nsTrigger.h"
#include "nsVersionRegistry.h"

PR_BEGIN_EXTERN_C

/* PUBLIC METHODS */

/**
 * @return true if SmartUpdate is enabled
 */
PRBool nsTrigger::UpdateEnabled(void)
{
  XP_Bool enabled;
  PREF_GetBoolPref( AUTOUPDATE_ENABLE_PREF, &enabled);

  if (enabled)
    return PR_TRUE;
  else
    return PR_FALSE;
}

/**
 * @param componentName     version registry name of the component
 * @return  version of the package. null if not installed, or 
 *          SmartUpdate disabled
 */
nsVersionInfo* nsTrigger::GetVersionInfo( char* componentName )
{
  if (UpdateEnabled())
    return nsVersionRegistry::componentVersion( componentName );
  else
    return NULL;
}

/**
 * Unconditionally starts the software update
 * @param url   URL of the JAR file
 * @param flags SmartUpdate modifiers (SILENT_INSTALL, FORCE_INSTALL)
 * @return false if SmartUpdate did not trigger
 */
PRBool nsTrigger::StartSoftwareUpdate( char* url, PRInt32 flags )
{
  if (url != NULL) {
    /* This is a potential problem */
    /* We are grabbing any context, but we really need ours */
    /* The problem is that Java does not have access to MWContext */
    MWContext * cx;
    cx = XP_FindSomeContext();
    if (cx)
      return SU_StartSoftwareUpdate(cx, url, NULL, NULL, NULL, flags);
    else
      return PR_FALSE;
  }
  return PR_FALSE;
}

PRBool nsTrigger::StartSoftwareUpdate( char* url )
{
  return StartSoftwareUpdate( url, DEFAULT_MODE );
}


/**
 * Conditionally starts the software update
 * @param url           URL of JAR file
 * @param componentName name of component in the registry to compare
 *                      the version against.  This doesn't have to be the
 *                      registry name of the installed package but could
 *                      instead be a sub-component -- useful because if a
 *                      file doesn't exist then it triggers automatically.
 * @param diffLevel     Specifies which component of the version must be
 *                      different.  If not supplied BLD_DIFF is assumed.
 *                      If the diffLevel is positive the install is triggered
 *                      if the specified version is NEWER (higher) than the
 *                      registered version.  If the diffLevel is negative then
 *                      the install is triggered if the specified version is
 *                      OLDER than the registered version.  Obviously this
 *                      will only have an effect if FORCE_MODE is also used.
 * @param version       The version that must be newer (can be a VersionInfo
 *                      object or a char* version
 * @param flags         Same flags as StartSoftwareUpdate (force, silent)
 */
PRBool
nsTrigger::ConditionalSoftwareUpdate(char* url,
                                     char* componentName,
                                     PRInt32 diffLevel,
                                     nsVersionInfo* versionInfo,
                                     PRInt32 flags)
{
  PRBool needJar = PR_FALSE;

  if ((versionInfo == NULL) || (componentName == NULL))
    needJar = PR_TRUE;
  else {
    int stat = nsVersionRegistry::validateComponent( componentName );
    if ( stat == REGERR_NOFIND ||
         stat == REGERR_NOFILE ) {
      // either component is not in the registry or it's a file
      // node and the physical file is missing
      needJar = PR_TRUE;
    } else {
      nsVersionInfo* oldVer = 
        nsVersionRegistry::componentVersion( componentName );

      if ( oldVer == NULL )
        needJar = PR_TRUE;
      else if ( diffLevel < 0 )
        needJar = (versionInfo->compareTo( oldVer ) <= diffLevel);
      else
        needJar = (versionInfo->compareTo( oldVer ) >= diffLevel);
      delete oldVer;
    }
  }

  if (needJar)
    return StartSoftwareUpdate( url, flags );
  else
    return PR_FALSE;
}

PRBool
nsTrigger::ConditionalSoftwareUpdate(char* url,
                                     char* componentName,
                                     char* version)
{
  nsVersionInfo* versionInfo = new nsVersionInfo(version);
  PRBool ret_val = ConditionalSoftwareUpdate( url, 
                                              componentName,
                                              BLD_DIFF, 
                                              versionInfo, 
                                              DEFAULT_MODE );
  delete versionInfo;
  return ret_val;
}

/**
 * Validates existence and compares versions
 *
 * @param regName       name of component in the registry to compare
 *                      the version against.  This doesn't have to be the
 *                      registry name of an installed package but could
 *                      instead be a sub-component.  If the named item
 *                      has a Path property the file must exist or a 
 *                      null version is used in the comparison.
 * @param version       The version to compare against
 */
PRInt32 nsTrigger::CompareVersion( char* regName, nsVersionInfo* version )
{
  PRInt32 ret_val;
  if (!UpdateEnabled())
    return EQUAL;

  nsVersionInfo* regVersion = GetVersionInfo( regName );

  if ( regVersion == NULL ||
       nsVersionRegistry::validateComponent( regName ) == REGERR_NOFILE ) {

    if (regVersion) delete regVersion;
    regVersion = new nsVersionInfo(0,0,0,0,0);
  }

  PR_ASSERT(regVersion != NULL);

  ret_val = regVersion->compareTo( version );
  delete regVersion;
  return ret_val;
}

PRInt32 nsTrigger::CompareVersion( char* regName, char* version )
{
  nsVersionInfo* versionInfo = new nsVersionInfo( version );
  PRInt32 ret_val = CompareVersion( regName, versionInfo );
  delete versionInfo;
  return ret_val;
}

PRInt32 
nsTrigger::CompareVersion( char* regName, PRInt32 maj, 
                           PRInt32 min, PRInt32 rel, 
                           PRInt32 bld )
{
  nsVersionInfo* versionInfo = new nsVersionInfo(maj, min, rel, bld, 0);
  PRInt32 ret_val = CompareVersion( regName, versionInfo );
  delete versionInfo;
  return ret_val;
}

PR_END_EXTERN_C
