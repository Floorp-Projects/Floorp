
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
#include "nsSpecialSystemDirectory.h"
#include "nsFileStream.h"
#include "prio.h"
#include "prerror.h"
#include "prmem.h"
#include "prefapi.h"
#include "plstr.h"
#include "prprf.h"

/* Network */

#include "net.h"

#include "nsPrefMigration.h"
#include "nsPMProgressDlg.h"

/* who's going to win the file name battle? */
#if defined(XP_UNIX)
#define PREF_FILE_NAME_IN_4x "preferences.js"
#elif defined(XP_MAC)
#define PREF_FILE_NAME_IN_4x "Netscape Preferences"
#elif defined(XP_PC)
#define PREF_FILE_NAME_IN_4x "prefs.js"
#else
/* this will cause a failure at run time, as it should, since we don't know
   how to migrate platforms other than Mac, Windows and UNIX */
#define PREF_FILE_NAME_IN_4x ""
#endif

/* and the winner is:  Windows */
#define PREF_FILE_NAME_IN_5x "prefs.js"

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

  
  char *oldPOPMailPathStr, *oldIMAPMailPathStr, *oldIMAPLocalMailPathStr, *oldNewsPathStr;
  char *newPOPMailPathStr, *newIMAPMailPathStr, *newIMAPLocalMailPathStr, *newNewsPathStr;
  char *popServerName;

  nsFileSpec oldPOPMailPath, oldIMAPMailPath, oldIMAPLocalMailPath, oldNewsPath;
  nsFileSpec newPOPMailPath, newIMAPMailPath, newIMAPLocalMailPath, newNewsPath;

  PRUint32 totalMailSize = 0, 
           totalNewsSize = 0, 
           totalProfileSize = 0,
           totalRequired = 0;

  PRInt32 serverType = 0, 
    nameLength = MAXPATHLEN;
  
  PRBool hasIMAP = PR_FALSE;

#if defined(NS_DEBUG)
  printf("*Entered Actual Migration routine*\n");
#endif


  if((newPOPMailPathStr = (char*) PR_MALLOC(MAXPATHLEN)) == NULL)
  {
    PR_Free(newPOPMailPathStr);
    *aResult = NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
  }
  
  if((newIMAPMailPathStr = (char*) PR_MALLOC(MAXPATHLEN)) == NULL)
  {
    PR_Free(newIMAPMailPathStr);
    *aResult = NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
  }

  if((newIMAPLocalMailPathStr = (char*) PR_MALLOC(MAXPATHLEN)) == NULL)
  {
    PR_Free(newIMAPLocalMailPathStr);
    *aResult = NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
  }

  if((newNewsPathStr = (char*) PR_MALLOC(MAXPATHLEN)) == NULL)
  {
    PR_Free(newNewsPathStr);
    *aResult = NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
  }

  if((oldPOPMailPathStr = (char*) PR_MALLOC(MAXPATHLEN)) == NULL)
  {
    PR_Free(oldPOPMailPathStr);
    *aResult = NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
  }

  if((oldIMAPMailPathStr = (char*) PR_MALLOC(MAXPATHLEN)) == NULL)
  {
    PR_Free(oldIMAPMailPathStr);
    *aResult = NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
  }
  
  if((oldIMAPLocalMailPathStr = (char*) PR_MALLOC(MAXPATHLEN)) == NULL)
  {
    PR_Free(oldIMAPLocalMailPathStr);
    *aResult = NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
  }

  if((oldNewsPathStr = (char*) PR_MALLOC(MAXPATHLEN)) == NULL)
  {
    PR_Free(oldNewsPathStr);
    *aResult = NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
  }

  if((popServerName = (char*) PR_MALLOC(MAXPATHLEN)) == NULL)
  {
    PR_Free(popServerName);
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
  PREF_GetIntPref("mail.server.type", &serverType);
  if (serverType == 0) /* User had POP */
  {
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
    PR_MkDir(nsNSPRPath(newPOPMailPath), PR_RDWR);
    PREF_GetCharPref("network.hosts.pop_server", popServerName, &nameLength);
    newPOPMailPath += popServerName;
    PR_MkDir(nsNSPRPath(newPOPMailPath), PR_RDWR);
  }
  else /* User had IMAP */
  {
    hasIMAP = PR_TRUE;
	/* First get the actual local mail files location */
    if(GetDirFromPref(newProfilePathStr, "mail.directory", newIMAPLocalMailPathStr, oldIMAPLocalMailPathStr) == NS_OK)
    {
      /* convert back to nsFileSpec */
      oldIMAPLocalMailPath = oldIMAPLocalMailPathStr;
      newIMAPLocalMailPath = newIMAPLocalMailPathStr;
    }
	else  /* default paths */
	{ 
      oldIMAPLocalMailPath = oldProfilePathStr;
      oldIMAPLocalMailPath += "Mail";
      newIMAPLocalMailPath = newProfilePathStr;
      newIMAPLocalMailPath += "Mail";
	}
    /* Now create the new directories */
    PR_MkDir(nsNSPRPath(newIMAPLocalMailPath), PR_RDWR);
    newIMAPLocalMailPath += "Local Mail";
    PR_MkDir(nsNSPRPath(newIMAPLocalMailPath), PR_RDWR);

	/* Next get IMAP mail summary files location */
    if(GetDirFromPref(newProfilePathStr, "mail.imap.root_dir", newIMAPMailPathStr, oldIMAPMailPathStr) == NS_OK)
    {
      /* convert back to nsFileSpec */
      oldIMAPMailPath = oldIMAPMailPathStr;
      newIMAPMailPath = newIMAPMailPathStr;
    }
	else  /* default paths */
	{ 
      oldIMAPMailPath = oldProfilePathStr;
      oldIMAPMailPath += "ImapMail";
      newIMAPMailPath = newProfilePathStr;
      newIMAPMailPath += "ImapMail";
	}
	PR_MkDir(nsNSPRPath(newIMAPMailPath), PR_RDWR);
  }


  /* Create the new News directory from the setting in prefs.js or a default */
  if(GetDirFromPref(newProfilePathStr, "news.directory", newNewsPathStr, oldNewsPathStr) == NS_OK)
  {
    oldNewsPath = oldNewsPathStr;
    newNewsPath = newNewsPathStr;
  }
  else /* default paths */
  {
    oldNewsPath = oldProfilePathStr;
    oldNewsPath += "News";
    newNewsPath = newProfilePathStr;
    newNewsPath += "News";
  }
  PR_MkDir(nsNSPRPath(newNewsPath), PR_RDWR);


  nsFileSpec oldProfilePath(oldProfilePathStr); /* nsFileSpec version of the profile's 4.x root dir */
  nsFileSpec newProfilePath(newProfilePathStr); /* Ditto for the profile's new 5.x root dir         */
  
  success = GetSizes(oldProfilePath, PR_FALSE, &totalProfileSize);
  success = GetSizes(oldNewsPath, PR_TRUE, &totalNewsSize);
  if(hasIMAP)
  {
    success = GetSizes(oldIMAPLocalMailPath, PR_TRUE, &totalMailSize);
  }
  else
  {
    success = GetSizes(oldPOPMailPath, PR_TRUE, &totalMailSize);
  }

 
  /* Get the drive name or letter for the profile tree */
  char *profile_hd_name;
  if ((profile_hd_name = (char*) PR_MALLOC(MAXPATHLEN)) == NULL)
    return NS_ERROR_OUT_OF_MEMORY;
  GetDriveName(oldProfilePath, profile_hd_name);
  
  /* Get the drive name or letter for the mail (IMAP-local or POP) tree */
  char *mail_hd_name;
  if ((mail_hd_name = (char*) PR_MALLOC(MAXPATHLEN)) == NULL)
    return NS_ERROR_OUT_OF_MEMORY;
  if (hasIMAP)
	GetDriveName(oldIMAPLocalMailPath, mail_hd_name);
  else
    GetDriveName(oldPOPMailPath, mail_hd_name);

  /* Get the drive name or letter for the news tree */
  char *news_hd_name;
  if ((news_hd_name = (char*) PR_MALLOC(MAXPATHLEN)) == NULL)
    return NS_ERROR_OUT_OF_MEMORY;
  GetDriveName(oldNewsPath, news_hd_name);

  
  /* Check to see if all the dirs are on the same drive (the default case) */
  if((PL_strcmp(profile_hd_name, mail_hd_name) == 0) &&
     (PL_strcmp(profile_hd_name, news_hd_name) == 0)) /* All dirs are on the same drive */
  {
    totalRequired = totalProfileSize + totalMailSize + totalNewsSize;
    if(CheckForSpace(newProfilePath, totalRequired) != NS_OK)
    {
      *aResult = NS_ERROR_ABORT;
	    return NS_ERROR_ABORT;  /* Need error code for not enough space */
    }
  }
  else
  {
    if(PL_strcmp(mail_hd_name, news_hd_name) == 0) /* Mail and news on same drive */
    {
      totalRequired = totalMailSize + totalNewsSize;
	  if (hasIMAP)
	  {
        if(CheckForSpace(newIMAPLocalMailPath, totalRequired) != NS_OK)
		{
          *aResult = NS_ERROR_ABORT;
	      return NS_OK;  /* Need error code for not enough space */
		}
	  }
	  else
	  {
        if(CheckForSpace(newPOPMailPath, totalRequired) != NS_OK)
		{
          *aResult = NS_ERROR_ABORT;
	      return NS_OK;  /* Need error code for not enough space */
		}
	  }
      if(CheckForSpace(newProfilePath, totalProfileSize) != NS_OK)
      {
        *aResult = NS_ERROR_ABORT;
        return NS_OK;
      }
    }
    else
    {
      if(PL_strcmp(profile_hd_name, mail_hd_name) == 0) /* Mail and profile on same drive */
      {
        totalRequired = totalProfileSize + totalMailSize;
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
		  if (hasIMAP)
		  {
            if(CheckForSpace(newIMAPMailPath, totalMailSize) != NS_OK)
			{
              *aResult = NS_ERROR_ABORT;
              return NS_ERROR_ABORT;  /* Need error code for not enough space */
			}
		  }
		  else
		  {
            if(CheckForSpace(newPOPMailPath, totalMailSize) != NS_OK)
			{
              *aResult = NS_ERROR_ABORT;
              return NS_ERROR_ABORT;  /* Need error code for not enough space */
			}
		  }
        }
        else
        {
          /* All the trees are on different drives */
		  if (hasIMAP)
		  {
            if(CheckForSpace(newIMAPMailPath, totalMailSize) != NS_OK)
			{
              *aResult = NS_ERROR_ABORT;
              return NS_ERROR_ABORT;  /* Need error code for not enough space */
			}
		  }
		  else
		  {
            if(CheckForSpace(newPOPMailPath, totalMailSize) != NS_OK)
			{
              *aResult = NS_ERROR_ABORT;
              return NS_ERROR_ABORT;  /* Need error code for not enough space */
			}
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
  PR_Free(mail_hd_name);
  PR_Free(news_hd_name);
  
  success = DoTheCopy(oldProfilePath, newProfilePath, PR_FALSE);
  success = DoTheCopy(oldNewsPath, newNewsPath, PR_TRUE);
  if(hasIMAP)
  {
    success = DoTheCopy(oldIMAPMailPath, newIMAPMailPath, PR_TRUE);
	success = DoTheCopy(oldIMAPLocalMailPath, newIMAPLocalMailPath, PR_TRUE);
  }
  else
    success = DoTheCopy(oldPOPMailPath, newPOPMailPath, PR_TRUE);
  
  success = DoSpecialUpdates(newProfilePath);

  PREF_SavePrefFile();

  PR_Free(oldPOPMailPathStr);
  PR_Free(oldIMAPMailPathStr);
  PR_Free(oldIMAPLocalMailPathStr);
  PR_Free(oldNewsPathStr);
  PR_Free(newPOPMailPathStr);
  PR_Free(newIMAPMailPathStr);
  PR_Free(newIMAPLocalMailPathStr);
  PR_Free(newNewsPathStr);
  PR_Free(popServerName);

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
  
  NS_ASSERTION((PL_strlen(PREF_FILE_NAME_IN_4x) > 0), "don't know how to migrate your platform");
  if (PL_strlen(PREF_FILE_NAME_IN_4x) > 0) {
    return NS_ERROR_UNEXPECTED;
  }
      
  /* Copy the old prefs.js file to the new profile directory for modification and reading */
  if ((prefsFile = (char*) PR_MALLOC(PL_strlen(oldProfilePath) + 32)) == NULL)
  {
    PR_Free(prefsFile);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsFileSpec oldPrefsFile(oldProfilePath);
  oldPrefsFile += PREF_FILE_NAME_IN_4x;

  nsFileSpec newPrefsFile(newProfilePath);
      
  if (!newPrefsFile.Exists())
  {
	  newPrefsFile.CreateDirectory();
  }

  oldPrefsFile.Copy(newPrefsFile);

  newPrefsFile += PREF_FILE_NAME_IN_4x;
  newPrefsFile.Rename(PREF_FILE_NAME_IN_5x);

  PREF_Init(newPrefsFile);

  PR_Free(prefsFile);

  return NS_OK;
}


/*---------------------------------------------------------------------------------
 * ComputeMailPath finds out if the user was using POP or IMAP and creates a new
 * target (migration) path based on that information.
 *
 * INPUT:oldPath -The path to the user's old Mail directory
 *
 * OUTPUT: newPath - The old path with the appropriate new directories appended 
 *
 * RETURNS: 
 *--------------------------------------------------------------------------------*/
nsresult
nsPrefMigration::ComputeMailPath(nsFileSpec oldPath, nsFileSpec *newPath)
{
  const char *localMailString="Local Mail"; /* string used for new IMAP dirs */ 
  const char *mail = "Mail";

  int  nameLength = MAXPATHLEN;
  char *serverType, *popServerName, *mailPath;

  PREF_GetCharPref("mail.server.type", serverType, &nameLength);
  if(*serverType == 0) /* User had POP */
  {
    PREF_GetCharPref("network.hosts.pop_server", popServerName, &nameLength);
    PREF_GetCharPref("mail.directory", mailPath, &nameLength);
    if (mailPath != NULL)
    {
      (newPath += *mailPath) += *popServerName;
    }
    else
    {
      newPath += *mail;
      newPath += *popServerName;
    }
    return NS_OK;
  }
  else  /* User had IMAP */
  {
    newPath += *localMailString;
    return NS_OK;
  }

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
  int oldDirLength = MAXPATHLEN;
  PRInt32 foundPref;


  foundPref = PREF_GetCharPref(pref, oldPath, &oldDirLength);
  if((foundPref == 0) && (*oldPath != 0))
  {
    PL_strcpy(newPath, oldPath);
    PL_strcat(newPath, "5");

    // save off the old pref, prefixed with "premigration"
    // for example, we need the old "mail.directory" pref whe migrating
    // the copies and folder prefs.
    char *premigration_pref = nsnull;
    premigration_pref = PR_smprintf("premigration.%s", pref);
    if (!premigration_pref) return NS_ERROR_FAILURE;
    PREF_SetCharPref (premigration_pref, oldPath);
    PR_FREEIF(premigration_pref);

    PREF_SetCharPref (pref, newPath); 
    return NS_OK;
  }
  else
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
 * Todo: you may want to change this proto from a float to a int64.
 *--------------------------------------------------------------------------*/
nsresult
nsPrefMigration::CheckForSpace(nsFileSpec newProfilePath, PRFloat64 requiredSpace)
{
  nsFileSpec drive(newProfilePath);
  PRInt64 availSpace = newProfilePath.GetDiskSpaceAvailable();
  PRInt64 require64;
  
  LL_F2L(require64, requiredSpace);
  if (LL_CMP(availSpace, <, require64))
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
  /* Need to add a string to the top of the prefs.js file to prevent it
   * from being loaded as a standard javascript file which would be a
   * security hole.
   */
  const char *headerString="# Mozilla User Preferences    "; 
  
  nsFileSpec fs(profilePath);
  fs += PREF_FILE_NAME_IN_5x;
  
  nsOutputFileStream fsStream(fs, (PR_WRONLY | PR_CREATE_FILE | PR_APPEND));
  
  if (!fsStream.is_open())
  {
    return NS_ERROR_FAILURE;
  }

  fsStream << headerString << nsEndl ;

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
  nsRepository::RegisterComponent(kPrefMigration_CID, NULL, NULL, path, PR_TRUE, PR_TRUE);
 
  return NS_OK;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* aServiceMgr, const char *path)
{
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





