/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "prmem.h"
#include "prmon.h"
#include "prlog.h"
#include "xp.h"
#include "prefapi.h"
#include "prprf.h"
#include "nsVersionInfo.h"
#include "nsString.h"

PR_BEGIN_EXTERN_C

/* Public Methods */

nsVersionInfo::nsVersionInfo(PRInt32 maj, PRInt32 min, PRInt32 rel, PRInt32 bld)
{
  major   = maj;
  minor   = min;
  release = rel;
  build   = bld;
}

nsVersionInfo::nsVersionInfo(char* versionArg)
{
  PRInt32 errorCode;

  major = minor = release = build = 0;

  if (versionArg == NULL) {
    versionArg = "0";
  }
  nsString version(versionArg);
  int dot = version.Find('.', 0);
  
  if ( dot == -1 ) {
    major = version.ToInteger(&errorCode);
  }
  else  {
    nsString majorStr;
    version.Mid(majorStr, 0, dot);
    major = majorStr.ToInteger(&errorCode);
    
    int prev = dot+1;
    dot = version.Find('.',prev);
    if ( dot == -1 ) {
      nsString minorStr;
      version.Mid(minorStr, prev, version.Length() - prev);
      minor = minorStr.ToInteger(&errorCode);
    }
    else {
      nsString minorStr;
      version.Mid(minorStr, prev, dot - prev);
      minor = minorStr.ToInteger(&errorCode);
      
      prev = dot+1;
      dot = version.Find('.',prev);
      if ( dot == -1 ) {
        nsString releaseStr;
        version.Mid(releaseStr, prev, version.Length() - prev);
        release = releaseStr.ToInteger(&errorCode);
      }
      else {
        nsString releaseStr;
        version.Mid(releaseStr, prev, dot - prev);
        release = releaseStr.ToInteger(&errorCode);
        
        prev = dot+1;
        if ( version.Length() > dot ) {
          nsString buildStr;
          version.Mid(buildStr, prev, version.Length() - prev);
          build = buildStr.ToInteger(&errorCode);
        }
      }
    }
  }
}

nsVersionInfo::~nsVersionInfo()
{
}

/* Text representation of the version info */
char* nsVersionInfo::toString()
{
  char *result=NULL;
  result = PR_sprintf_append(result, "%d.%d.%d.%d", major, minor, release, build);
  return result;
}

/*
 * compareTo() -- Compares version info.
 * Returns -n, 0, n, where n = {1-4}
 */
nsVersionEnum nsVersionInfo::compareTo(nsVersionInfo* vi)
{
  nsVersionEnum diff;

  if ( vi == NULL ) {
    diff = nsVersionEnum_MAJOR_DIFF;
  }
  else if ( major == vi->major ) {
    if ( minor == vi->minor ) {
      if ( release == vi->release ) {
        if ( build == vi->build )
          diff = nsVersionEnum_EQUAL;
        else if ( build > vi->build )
          diff = nsVersionEnum_BLD_DIFF;
        else
          diff = nsVersionEnum_BLD_DIFF_MINUS;
      }
      else if ( release > vi->release )
        diff = nsVersionEnum_REL_DIFF;
      else
        diff = nsVersionEnum_REL_DIFF_MINUS;
    }
    else if (  minor > vi->minor )
      diff = nsVersionEnum_MINOR_DIFF;
    else
      diff = nsVersionEnum_MINOR_DIFF_MINUS;
  }
  else if ( major > vi->major )
    diff = nsVersionEnum_MAJOR_DIFF;
  else
    diff = nsVersionEnum_MAJOR_DIFF_MINUS;
  
  return diff;
}

nsVersionEnum nsVersionInfo::compareTo(char* version)
{
  nsVersionInfo* versionInfo = new nsVersionInfo(version);
  nsVersionEnum ret_val = compareTo(versionInfo);
  delete versionInfo;
  return ret_val;
}

nsVersionEnum nsVersionInfo::compareTo(PRInt32 maj, PRInt32 min, PRInt32 rel, PRInt32 bld)
{
  nsVersionInfo* versionInfo = new nsVersionInfo(maj, min, rel, bld);
  nsVersionEnum ret_val = compareTo(versionInfo);
  delete versionInfo;
  return ret_val;
}

PR_END_EXTERN_C
