
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "pratom.h"
#include "nsRepository.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "VerReg.h"
#include "nsSpecialSystemDirectory.h"
#include "prio.h"
#include "prerror.h"
#include "prmem.h"
#include "prefapi.h"
#include "plstr.h"

/* Network */

#include "net.h"

#include "nsPrefMigration.h"
#include "nsPMProgressDlg.h"

/*-----------------------------------------------------------------
 * Globals
 *-----------------------------------------------------------------*/
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

static NS_DEFINE_IID(kIPrefMigration_IID, NS_IPrefMigration_IID);
static NS_DEFINE_IID(kPrefMigration_CID,  NS_PrefMigration_CID);

static PRInt32 gInstanceCnt = 0;
static PRInt32 gLockCnt     = 0;

const char separator = '\\';  /* directory separator charater */
const char eolnChar  = '\0';  /* end of line charater         */

nsPrefMigration::nsPrefMigration()
{
  NS_INIT_REFCNT();
}

nsPrefMigration::~nsPrefMigration()
{
}

NS_IMETHODIMP 
nsPrefMigration::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL)
  {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if ( aIID.Equals(kIPrefMigration_IID) )
  {
    *aInstancePtr = (void*)(nsIPrefMigration*)this;
    AddRef();
    return NS_OK;
  }
  else if ( aIID.Equals(kISupportsIID) )
  {
    *aInstancePtr = (void*)(nsISupports*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsPrefMigration)
NS_IMPL_RELEASE(nsPrefMigration)

/*--------------------------------------------------------------------------
 * Process Prefs is the primary funtion for the class nsPrefMigration.
 *
 * Called by: The Profile Manager
 * INPUT: The specific profile path (prefPath) and the 5.0 installed path
 * OUTPUT: The modified 5.0 prefs files
 * RETURN: Success or a failure code
 *
 *-------------------------------------------------------------------------*/
NS_IMETHODIMP
nsPrefMigration::ProcessPrefs(char* oldProfilePathStr, char* newProfilePathStr, nsresult *aResult)
{

  
  char *oldPOPMailPathStr, *oldNewsPathStr;
  char *newPOPMailPathStr, *newNewsPathStr;

  nsFileSpec oldPOPMailPath, oldIMAPMailPath, oldNewsPath;
  nsFileSpec newPOPMailPath, newIMAPMailPath, newNewsPath;

  PRUint32 totalPOPMailSize = 0, 
           totalIMAPMailSize = 0, 
           totalNewsSize = 0, 
           totalProfileSize = 0,
           totalRequired = 0,
           totalSize = 0,
           numberOfMailFiles = 0, 
           numberOfNewsFiles = 0, 
           numberOfProfileFiles = 0;

  PRInt32 oldDirLength = 0;
  
  PRBool hasIMAP = PR_FALSE;

#if defined(NS_DEBUG)
  printf("*Entered Actual Migration routine*\n");
#endif


  if((newPOPMailPathStr = (char*) PR_MALLOC(_MAX_PATH)) == NULL)
  {
    PR_Free(newPOPMailPathStr);
    *aResult = NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
  }

  if((newNewsPathStr = (char*) PR_MALLOC(_MAX_PATH)) == NULL)
  {
    PR_Free(newNewsPathStr);
    *aResult = NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
  }

  if((oldPOPMailPathStr = (char*) PR_MALLOC(_MAX_PATH)) == NULL)
  {
    PR_Free(oldPOPMailPathStr);
    *aResult = NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
  }

  if((oldNewsPathStr = (char*) PR_MALLOC(_MAX_PATH)) == NULL)
  {
    PR_Free(oldNewsPathStr);
    *aResult = NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
  }


  /* Create the new profile tree for 5.x */
  nsresult success = CreateNewUser5Tree(oldProfilePathStr, newProfilePathStr);
  if (success != NS_OK)
  {
    *aResult = success;
    return NS_OK;
  }

  /* Create the new mail directory from the setting in prefs.js or a default */
  if(GetDirFromPref(newProfilePathStr, "mail.directory", newPOPMailPathStr, oldPOPMailPathStr) == NS_OK)
  {
    /* convert back to nsFileSpec */
    oldPOPMailPath = oldPOPMailPathStr;
    newPOPMailPath = newPOPMailPathStr;
  }
  else
  {
    /* use the default locations */
    oldPOPMailPath = oldProfilePathStr;
    oldPOPMailPath += "Mail";
    newPOPMailPath = newProfilePathStr;
    newPOPMailPath += "Mail";
  }

  /* Create the new news directory from the setting in prefs.js or a default */
  if(GetDirFromPref(newProfilePathStr, "news.directory", newNewsPathStr, oldNewsPathStr) == NS_OK)
  {
    oldNewsPath = oldNewsPathStr;
    newNewsPath = newNewsPathStr;
  }
  else
  {
    oldNewsPath = oldProfilePathStr;
    oldNewsPath += "News";
    newNewsPath = newProfilePathStr;
    newNewsPath += "News";
  }

  //IMAP mail doesn't have a pref for its location, so just set it.
  oldIMAPMailPath = oldProfilePathStr;
  oldIMAPMailPath += "imapmail";
  if(oldIMAPMailPath.IsDirectory())
  {
    newIMAPMailPath = newProfilePathStr;
    newIMAPMailPath += "imapmail";
    hasIMAP = PR_TRUE;
  }

  nsFileSpec oldProfilePath(oldProfilePathStr); /* nsFileSpec version of the profile's 4.x root dir */
  nsFileSpec newProfilePath(newProfilePathStr); /* Ditto for the profile's new 5.x root dir         */
  
  success = GetSizes(oldProfilePath, PR_FALSE, &totalProfileSize);
  success = GetSizes(oldPOPMailPath, PR_TRUE, &totalPOPMailSize);
  success = GetSizes(oldNewsPath, PR_TRUE, &totalNewsSize);
  if(hasIMAP)
  {
    success = GetSizes(oldIMAPMailPath, PR_TRUE, &totalIMAPMailSize);
    totalProfileSize += totalIMAPMailSize; /* IMAP tree is ALWAYS off the profile dir */
  }

  
  /* Get the drive name or letter for the profile tree */
  char *profile_hd_name;
  if ((profile_hd_name = (char*) PR_MALLOC(MAXPATHLEN)) == NULL)
    return NS_ERROR_OUT_OF_MEMORY;
  GetDriveName(oldProfilePath, profile_hd_name);
  
  /* Get the drive name or letter for the pop mail tree */
  char *POP_hd_name;
  if ((POP_hd_name = (char*) PR_MALLOC(MAXPATHLEN)) == NULL)
    return NS_ERROR_OUT_OF_MEMORY;
  GetDriveName(oldPOPMailPath, POP_hd_name);

  /* Get the drive name or letter for the news tree */
  char *news_hd_name;
  if ((news_hd_name = (char*) PR_MALLOC(MAXPATHLEN)) == NULL)
    return NS_ERROR_OUT_OF_MEMORY;
  GetDriveName(oldNewsPath, news_hd_name);

  
  /* Check to see if all the dirs are on the same drive (the default case) */
  if((PL_strcmp(profile_hd_name, POP_hd_name) == 0) &&
     (PL_strcmp(profile_hd_name, news_hd_name) == 0)) /* All dirs are on the same drive */
  {
    totalRequired = totalProfileSize + totalPOPMailSize + totalNewsSize + totalIMAPMailSize;
    if(CheckForSpace(newProfilePath, totalRequired) != NS_OK)
    {
      *aResult = NS_ERROR_ABORT;
	    return NS_ERROR_ABORT;  /* Need error code for not enough space */
    }
  }
  else
  {
    if(PL_strcmp(POP_hd_name, news_hd_name) == 0) /* Mail and news on same drive */
    {
      totalRequired = totalPOPMailSize + totalNewsSize;
      if(CheckForSpace(newPOPMailPath, totalRequired) != NS_OK)
      {
        *aResult = NS_ERROR_ABORT;
	      return NS_OK;  /* Need error code for not enough space */
      }
      if(CheckForSpace(newProfilePath, totalProfileSize) != NS_OK)
      {
        *aResult = NS_ERROR_ABORT;
        return NS_OK;
      }
    }
    else
    {
      if(PL_strcmp(profile_hd_name, POP_hd_name) == 0) /* Mail and profile on same drive */
      {
        totalRequired = totalProfileSize + totalPOPMailSize;
        if(CheckForSpace(newProfilePath, totalRequired) != NS_OK)
        {
          *aResult = NS_ERROR_ABORT;
          return NS_ERROR_ABORT;  /* Need error code for not enough space */
        }
        if(CheckForSpace(newNewsPath, totalNewsSize) != NS_OK)
        {
          *aResult = NS_ERROR_ABORT;
          return NS_OK;  /* Need error code for not enough space */
        }
      }
      else
      {
        if(PL_strcmp(profile_hd_name, news_hd_name) == 0) /* News and profile on same drive */
        {
          totalRequired = totalProfileSize + totalNewsSize;
          if(CheckForSpace(newProfilePath, totalRequired) != NS_OK)
          {
            *aResult = NS_ERROR_ABORT;
            return NS_ERROR_ABORT;  /* Need error code for not enough space */
          }
          if(CheckForSpace(newPOPMailPath, totalPOPMailSize) != NS_OK)
          {
            *aResult = NS_ERROR_ABORT;
            return NS_ERROR_ABORT;  /* Need error code for not enough space */
          }
        }
        else
        {
          /* All the trees are on different drives */
          if(CheckForSpace(newPOPMailPath, totalPOPMailSize) != NS_OK)
          {
            *aResult = NS_ERROR_ABORT;
            return NS_ERROR_ABORT;  /* Need error code for not enough space */
          }
          if(CheckForSpace(newNewsPath, totalNewsSize) != NS_OK)
          {
            *aResult = NS_ERROR_ABORT;
            return NS_ERROR_ABORT;  /* Need error code for not enough space */
          }
          if(CheckForSpace(newProfilePath, totalProfileSize) != NS_OK)
          {
            *aResult = NS_ERROR_ABORT;
            return NS_ERROR_ABORT;  /* Need error code for not enough space */
          }
        }
      } /* else */
    } /* else */
  } /* else */

  PR_Free(profile_hd_name);
  PR_Free(POP_hd_name);
  PR_Free(news_hd_name);
  
  PR_MkDir(nsNSPRPath(newPOPMailPath), PR_RDWR);
  PR_MkDir(nsNSPRPath(newNewsPath), PR_RDWR);
  if(hasIMAP)
    PR_MkDir(newIMAPMailPath, PR_RDWR);


  success = DoTheCopy(oldProfilePath, newProfilePath, PR_FALSE);
  success = DoTheCopy(oldPOPMailPath, newPOPMailPath, PR_TRUE);
  success = DoTheCopy(oldNewsPath, newNewsPath, PR_TRUE);
  if(hasIMAP)
    success = DoTheCopy(oldIMAPMailPath, newIMAPMailPath, PR_TRUE);
  
  success = DoSpecialUpdates(newProfilePath);


  PR_Free(oldPOPMailPathStr);
  PR_Free(oldNewsPathStr);
  PR_Free(newPOPMailPathStr);
  PR_Free(newNewsPathStr);

  *aResult = NS_OK;

  return NS_OK;
}


/*----------------------------------------------------------------------------
 * CreateNewUsers5Tree creates the directory called users5 (parent of the
 * of the profile directories) and the profile directory itself
 *---------------------------------------------------------------------------*/

nsresult
nsPrefMigration::CreateNewUser5Tree(char* oldProfilePath, char* newProfilePath)
{

  char* prefsFile;

  /* Copy the old prefs.js file to the new profile directory for modification and reading */
  if ((prefsFile = (char*) PR_MALLOC(PL_strlen(oldProfilePath) + 32)) == NULL)
  {
    PR_Free(prefsFile);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsFileSpec oldPrefsFile(oldProfilePath);
#ifdef XP_PC
  oldPrefsFile += "prefs.js";
#else
  oldPrefsFile += "Netscape Preferences";
#endif

  nsFileSpec newPrefsFile(newProfilePath);
      
  if (!newPrefsFile.Exists())
  {
	  newPrefsFile.CreateDirectory();
  }

  oldPrefsFile.Copy(newPrefsFile);
#ifdef XP_MAC
  newPrefsFile += "Netscape Preferences";
  newPrefsFile.Rename("prefs.js"); /* Rename the file for the Mac only */
#else
  newPrefsFile += "prefs.js";
#endif

  PREF_Init(newPrefsFile);

  PR_Free(prefsFile);

  return NS_OK;
}

/*---------------------------------------------------------------------------------
 * GetDirFromPref gets a directory based on a preference set in the 4.x
 * preferences file, adds a 5 and resets the preference.
 *
 * INPUT: pref - the pref in the "dot" format (e.g. mail.directory)
 *
 * OUTPUT: newPath - The old path with a 5 added 
 *         oldPath - The old path from the pref (if any)
 *
 * RETURNS: NS_OK if the pref was successfully pulled from the prefs file
 *--------------------------------------------------------------------------------*/
nsresult
nsPrefMigration::GetDirFromPref(char* newProfilePath, char* pref, char* newPath, char* oldPath)
{
  int oldDirLength = _MAX_PATH;
  char *prefs_jsPath;

  /* Get location of directory from prefs.js */
  if((prefs_jsPath = (char*) PR_MALLOC(PL_strlen(newProfilePath) + 32)) == NULL)
  {
    PR_Free(prefs_jsPath);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  //PL_strcpy(prefs_jsPath, newProfilePath);
  //PL_strcat(prefs_jsPath, "\\prefs50.js\0");

  //if(PREF_Init(prefs_jsPath))
  //{
    PREF_GetCharPref(pref, oldPath, &oldDirLength);
    if(*oldPath != '\0')
    {
      PL_strcpy(newPath, oldPath);
      PL_strcat(newPath, "5");
      PREF_SetCharPref (pref, newPath); 
      PREF_SavePrefFile();
      PR_Free(prefs_jsPath);
      return NS_OK;
    }
    else
      return NS_ERROR_ABORT;
  //}
  
  PR_Free(prefs_jsPath);
  return NS_ERROR_ABORT;
}


/*---------------------------------------------------------------------------------
 * GetSizes reads the 4.x files in the profile tree and accumulates their sizes
 *
 * INPUT:
 *
 * OUPUT:
 *
 * RETURNS:
 *
 *--------------------------------------------------------------------------------*/
nsresult
nsPrefMigration::GetSizes(nsFileSpec inputPath, PRBool readSubdirs, PRUint32 *sizeTotal)
{
  int i = 0;
  char* folderName;
  nsAutoString fileOrDirNameStr;
  PRInt32 len;

  for (nsDirectoryIterator dir(inputPath, PR_FALSE); dir.Exists(); dir++)
  {
    nsFileSpec fileOrDirName = (nsFileSpec&)dir;
    folderName = fileOrDirName.GetLeafName();
    fileOrDirNameStr = folderName;
    len = fileOrDirNameStr.Length();
    if (fileOrDirNameStr.Find(".snm", PR_TRUE) != -1)  /* Don't copy the summary files */
      continue;
    else
    {
      if (fileOrDirName.IsDirectory())
      {
        if(readSubdirs)
        {
          GetSizes(fileOrDirName, PR_TRUE, sizeTotal); /* re-enter the GetSizes function */
        }
        else
          continue;
      }
      else
        *sizeTotal += fileOrDirName.GetFileSize();
    }
  }

  return NS_OK;
}


/*--------------------------------------------------------------------------
 * GetDriveName gets the drive letter (on Windows) or the volume name (on Mac)
 *
 * INPUT: an nsFileSpec path
 * 
 * OUTPUT: the drive letter or volume name
 * 
 * RETURNS: NS_OK if found
 *          NS_ERROR_FAILURE if some error occurs while iterating through the
 *                           parent nodes.
 *
 *--------------------------------------------------------------------------*/
nsresult
nsPrefMigration::GetDriveName(nsFileSpec inputPath, char* driveName)
{
  nsFileSpec oldParent, newParent;
  PRBool foundIt = PR_FALSE;

  inputPath.GetParent(oldParent); /* do initial pass to optimize one directory case */
  while (!foundIt)
  {
    PL_strcpy(driveName, oldParent.GetCString());
    oldParent.GetParent(newParent);
    /* Since GetParent doesn't return an error if there's no more parents
     * I have to compare the results of the parent value (string) to see 
     * if they've changed. */
    if (PL_strcmp(driveName, newParent.GetCString()) == 0)
      foundIt = PR_TRUE;
    else
      oldParent = newParent;
  }
  return NS_OK;
}

/*--------------------------------------------------------------------------
 * CheckForSpace checks the target drive for enough space
 *
 * INPUT: newProfilePath - The path to the new profile
 *        requiredSpace - The space needed on the new profile drive
 *
 * RETURNS: NS_OK if enough space is available
 *          NS_ERROR_FAILURE if there is not enough space
 *
 *--------------------------------------------------------------------------*/
nsresult
nsPrefMigration::CheckForSpace(nsFileSpec newProfilePath, PRFloat64 requiredSpace)
{
//  nsFileSpec drive(newProfilePath);

  if (newProfilePath.GetDiskSpaceAvailable() < requiredSpace)
    return NS_ERROR_FAILURE;
  return NS_OK;
}

/*-------------------------------------------------------------------------
 * DoTheCopy copies the files listed in oldPath to newPath
 * 
 * INPUT: oldPath - The old profile path plus the specific data type 
 *                  (e.g. mail or news)
 *        newPath - The new profile path plus the specific data type
 *
 * RETURNS: NS_OK if successful
 *          NS_ERROR_FAILURE if failed
 *
 *--------------------------------------------------------------------------*/
nsresult
nsPrefMigration::DoTheCopy(nsFileSpec oldPath, nsFileSpec newPath, PRBool readSubdirs)
{
  int i = 0;
  char* folderName;
  nsAutoString fileOrDirNameStr;
 
  for (nsDirectoryIterator dir(oldPath, PR_FALSE); dir.Exists(); dir++)
  {
    nsFileSpec fileOrDirName = (nsFileSpec&)dir; //set first file or dir to a nsFileSpec
    folderName = fileOrDirName.GetLeafName();    //get the filename without the full path
    fileOrDirNameStr = folderName;

    if (fileOrDirNameStr.Find(".snm", PR_TRUE) != -1)  /* Don't copy the summary files */
      continue;
    else
    {
      if (fileOrDirName.IsDirectory())
      {
        if(readSubdirs)
        {
          nsFileSpec newPathExtended = newPath;
          newPathExtended += folderName;
          newPathExtended.CreateDirectory();
          DoTheCopy(fileOrDirName, newPathExtended, PR_TRUE); /* re-enter the DoTheCopy function */
        }
        else
          continue;
      }
      else
        fileOrDirName.Copy(newPath);
    }
  }  
  
  return NS_OK;
}


/*----------------------------------------------------------------------------
 * DoSpecialUpdates updates is a routine that does some miscellaneous updates 
 * like renaming certain files, etc.
 *
 *--------------------------------------------------------------------------*/
nsresult
nsPrefMigration::DoSpecialUpdates(nsFileSpec profilePath)
{
  /*
  nsFileSpec bookmarkFile = profilePath;
  bookmarkFile += "bookmark.htm";
  if (bookmarkFile.Exists())
    bookmarkFile.Rename("bookmark.html");
  */
  /* This was originally designed for the bookmark renaming.  Since that's now
     invalid I'm leaving this in place for future miscellaneous migrations */
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// DLL Entry Points:
////////////////////////////////////////////////////////////////////////////////

extern "C" NS_EXPORT PRBool
NSCanUnload(nsISupports* serviceMgr)
{
  return PRBool (gInstanceCnt == 0 && gLockCnt == 0);
}

extern "C" NS_EXPORT nsresult
NSRegisterSelf(nsISupports* aServiceMgr, const char *path)
{
  nsresult result;
  nsRepository::RegisterComponent(kPrefMigration_CID, NULL, NULL, path, PR_TRUE, PR_TRUE);
 
  return NS_OK;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* aServiceMgr, const char *path)
{
  nsresult result;
  //nsRepository::UnregisterFactory(kPrefMigration_CID, path);
  return NS_OK;
}


extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
  if (aFactory == NULL)
  {
    return NS_ERROR_NULL_POINTER;
  }

  *aFactory = NULL;
  nsISupports *inst;

  if ( aClass.Equals(kPrefMigration_CID) )
  {
    inst = new nsPrefMigrationFactory();        
  }
  else
  {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (inst == NULL)
  {   
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult res = inst->QueryInterface(kIFactoryIID, (void**) aFactory);

  if (NS_FAILED(res))
  {   
    delete inst;
  }

  return res;
}





