
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
#include "nsFileSpec.h"
#include "nsSpecialSystemDirectory.h"
#include "prio.h"
#include "prerror.h"
#include "prmem.h"
#include "prefapi.h"
#include "plstr.h"

/* Network */

#include "net.h"

#include "nsPrefMigration.h"

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
nsPrefMigration::ProcessPrefs(char* profilePath, char* installPath50, nsresult *aResult)
{
/*
  PRFileDesc *reg4file, *prefs4file;   
    *reg5file,
    *prefs5file,
    *cfg5file,
    *cfg4file, 
    *cssfile;
*/
  char *mailFileArray[200], *newsFileArray[200];
  char *oldMailPath, *oldNewsPath;
  char *newMailPath, *newNewsPath;
  char *newProfilePath;
  PRUint32 totalMailSize = 0, totalNewsSize = 0;
  PRInt32 oldDirLength = 0;

  if((newProfilePath = (char*) PR_MALLOC(PL_strlen(profilePath) + 2)) == NULL)
  {
    PR_Free(newProfilePath);
    *aResult = NS_ERROR_FAILURE;
    return 0;
  }

  if((newMailPath = (char*) PR_MALLOC(_MAX_PATH)) == NULL)
  {
    PR_Free(newMailPath);
    *aResult = NS_ERROR_FAILURE;
    return 0;
  }

  if((newNewsPath = (char*) PR_MALLOC(_MAX_PATH)) == NULL)
  {
    PR_Free(newNewsPath);
    *aResult = NS_ERROR_FAILURE;
    return 0;
  }

  if((oldMailPath = (char*) PR_MALLOC(_MAX_PATH)) == NULL)
  {
    PR_Free(oldMailPath);
    *aResult = NS_ERROR_FAILURE;
    return 0;
  }

  if((oldNewsPath = (char*) PR_MALLOC(_MAX_PATH)) == NULL)
  {
    PR_Free(oldNewsPath);
    *aResult = NS_ERROR_FAILURE;
    return 0;
  }

  /* Create the new profile tree for 5.x */
  nsresult success = CreateNewUser5Tree(profilePath, newProfilePath);
  if (success != NS_OK)
  {
    *aResult = success;
    return 0;
  }

  /* Create the new mail directory from the setting in prefs.js or a default */
  if(GetDirFromPref(newProfilePath, "mail.directory", newMailPath, oldMailPath) != NS_OK)
  {
    /* PREF_GetDefaultCharPref("mail.directory", oldMailPath, &oldDirLength); */
    PL_strcpy(oldMailPath, profilePath);
    PL_strcat(oldMailPath, "\\Mail\0");
    PL_strcpy(newMailPath, newProfilePath);
    PL_strcat(newMailPath, "\\Mail\0");
  }

  /* Create the new news directory from the setting in prefs.js or a default */
  if(GetDirFromPref(newProfilePath, "news.directory", newNewsPath, oldNewsPath) != NS_OK)
  {
    /* PREF_GetDefaultCharPref("news.directory", oldNewsPath, &oldDirLength); */
    PL_strcpy(oldNewsPath, profilePath);
    PL_strcat(oldNewsPath, "\\News\0");
    PL_strcpy(newNewsPath, newProfilePath);
    PL_strcat(newNewsPath, "\\News\0");
  }

  /* Read the user's 4.x files from their profile */
  success = Read4xFiles(oldMailPath, mailFileArray, &totalMailSize);
  success = Read4xFiles(oldNewsPath, newsFileArray, &totalNewsSize);

  if(CheckForSpace(newMailPath, totalMailSize) != NS_OK)
	  return -1;  /* Need error code for not enough space */

  if(CheckForSpace(newNewsPath, totalNewsSize) != NS_OK)
    return -1;

  PR_MkDir(newMailPath, PR_RDWR);
  PR_MkDir(newNewsPath, PR_RDWR);

  success = DoTheCopy(oldMailPath, newMailPath, mailFileArray);
  success = DoTheCopy(oldNewsPath, newNewsPath, newsFileArray);

  PR_Free(newProfilePath);
  PR_Free(oldMailPath);
  PR_Free(oldNewsPath);
  PR_Free(newMailPath);
  PR_Free(newNewsPath);
  //PR_Free(mailFileArray);
  //PR_Free(newsFileArray);

  return NS_OK;
}


/*----------------------------------------------------------------------------
 * CreateNewUsers5Tree creates the directory called users5 (parent of the
 * of the profile directories) and the profile directory itself
 *---------------------------------------------------------------------------*/

nsresult
nsPrefMigration::CreateNewUser5Tree(char* oldProfilePath, char* newProfilePath)
{
  char* users5Dir;
  char* prefsFile;
  int newDirLength = _MAX_PATH, i=0, j=0, len=0;
  PRInt32 idx = 0, sbidx = 0;

  if ((users5Dir = (char*) PR_MALLOC(PL_strlen(oldProfilePath) + 32)) == NULL)
  {
    PR_Free(users5Dir);
    return -1;
  }

#ifdef XP_PC
  PL_strcpy(users5Dir, oldProfilePath);
  len = strlen(users5Dir);
  i = len;

  while (users5Dir[i] != separator)
    i--;

  j = i;
  users5Dir[i] = '5';
  i++; 
  users5Dir[i] = eolnChar;
#endif /* XP_PC */

#ifdef DKB
#ifdef XP_MAC
  nsSpecialSystemDirectory docDir = nsSpecialSystemDirectory::Mac_DocumentsDirectory;
  docDir += ":netscape:users5";
  users5Dir = docDir.mPath;
#endif /* XP_MAC */
#endif /* DKB */

  /* Create 5.x profile directory tree */
  if (!PR_OpenDir(users5Dir))     /* users5 directory doesn't exist */
    PR_MkDir(users5Dir, PR_RDWR);

  while ((oldProfilePath[j] != eolnChar) && (j <= len)) /* reassemble the user5 path */
  {
    users5Dir[i] = oldProfilePath[j];
    i++;
    j++;
  }
  users5Dir[i] = '\0';
  PR_MkDir(users5Dir, PR_RDWR);         /* Create the actual profile dir */
  PL_strcpy(newProfilePath, users5Dir); /* set newProfilePath for reference parameter */

  /* Copy the old prefs.js file to the new profile directory for modification and reading */
  if ((prefsFile = (char*) PR_MALLOC(PL_strlen(oldProfilePath) + 32)) == NULL)
  {
    PR_Free(prefsFile);
    return -1;
  }
  PL_strcpy(prefsFile, oldProfilePath);
  PL_strcat(prefsFile, "\\prefs.js\0");

  nsFileSpec oldPrefsFile(prefsFile);
  nsFileSpec newPrefsFile(newProfilePath);

  oldPrefsFile.Copy(newPrefsFile);

  PR_Free(users5Dir);
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
    return NS_ERROR_FAILURE;
  }

  PL_strcpy(prefs_jsPath, newProfilePath);
  PL_strcat(prefs_jsPath, "\\prefs.js\0");

  if(PREF_Init(prefs_jsPath))
  {
    if(PREF_GetCharPref(pref, oldPath, &oldDirLength) > -1) /* found the pref */
    {
      PL_strcpy(newPath, oldPath);
      PL_strcat(newPath, "5");
      PREF_SetCharPref (pref, newPath); 
      PREF_SavePrefFile();
      PR_Free(prefs_jsPath);

      return NS_OK;
    }
    PR_Free(prefs_jsPath);
    return NS_ERROR_ABORT;
  }
  return NS_ERROR_FAILURE;   /* need error code for pref file not found */
}

/*---------------------------------------------------------------------------------
 * Read4xFiles reads the files contained in the profile path and loads the 
 *             names into an array
 *
 * INPUT: ProfilePath - The 4.x profile path plus the data dir (e.g. mail or news
 *        fileArray - A blank array to be filled by this function
 *        sizeTotal - A total byte accumulator
 *
 * OUTPUT: fileArray - The input fileArray filled with filenames
 *         sizeTotal - The total (in bytes) of the file sizes in fileArray
 *
 * RETURNS: NS_OK if successful
 *--------------------------------------------------------------------------------*/
nsresult
nsPrefMigration::Read4xFiles(char* ProfilePath, char* fileArray[], PRUint32* sizeTotal)
{
  char* fullFileName;
  int i = 0;

  PRUint32 fileSize = 0, length = 0;
  PRDir *ProfileDir;
  PRDirEntry *filename;
  PRFileInfo fileInfo;
  bool pr_succeeded;

  if((fullFileName = (char*) PR_MALLOC(PL_strlen(ProfilePath) + 64)) == NULL)
  {
    PR_Free(fullFileName);
    return -1;
  }

  /* Load files into an array while adding up their size */
  ProfileDir = PR_OpenDir(ProfilePath);
  while ((filename = PR_ReadDir(ProfileDir, PR_SKIP_BOTH)) != NULL)
  {
    length = PL_strlen(filename->name);
	  if(PL_strcmp(&filename->name[length -3], "snm") != 0) /* Don't copy the summary files */
    {
      fileArray[i] = (char*) PR_MALLOC(length);
	    PL_strcpy(fileArray[i], (char*)filename->name);
      PL_strcpy(fullFileName, ProfilePath);
      PL_strcat(fullFileName, "\\");
      PL_strcat(fullFileName, (char*) filename->name);
      PL_strcat(fullFileName, "\0");

	    i++;
      if (pr_succeeded = PR_GetFileInfo(fullFileName, &fileInfo) == PR_SUCCESS)
      {
        *sizeTotal = *sizeTotal + fileInfo.size;
      }
      fileSize = 0;	
    }
  }

  fileArray[i] = NULL; /* Put a Null in the array after the last file name */
  PR_Free(fullFileName);

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
nsPrefMigration::CheckForSpace(char* newProfilePath, PRFloat64 requiredSpace)
{
  nsFileSpec drive(newProfilePath);

  if (drive.GetDiskSpaceAvailable() < requiredSpace)
    return NS_ERROR_FAILURE;
  return NS_OK;
}

/*-------------------------------------------------------------------------
 * DoTheCopy copies the files listed in fileArray from oldPath to newPath
 * 
 * INPUT: oldPath - The old profile path plus the specific data type 
 *                  (e.g. mail or news)
 *        newPath - The new profile path plus the specific data type
 *        fileArray - The list of filenames read from the old profile path
 *
 * RETURNS: NS_OK if successful
 *          NS_ERROR_FAILURE if failed
 *
 *--------------------------------------------------------------------------*/
nsresult
nsPrefMigration::DoTheCopy(char* oldPath, char* newPath, char* fileArray[])
{
  PRInt32 i = 0, succeeded = 0;

  while ((fileArray[i] != NULL) && (i < 200))
  {
	  nsFileSpec sourceFile(oldPath);
	  sourceFile += fileArray[i];
	  nsFileSpec targetFile(newPath);

  	//nsFileSpec parentofTargetFile;
  	//targetFile.GetParent(parentofTargetFile);

	  succeeded = sourceFile.Copy(targetFile);

    i++;
  }
  return NS_OK;
}


/*

NS_IMETHODIMP
nsPrefMigration::Startup()
{
  VR_SetRegDirectory("C:\\temp\\");
  NR_StartupRegistry();   

  return NS_OK;
}

NS_IMETHODIMP
nsPrefMigration::Shutdown()
{
  NR_ShutdownRegistry();
  return NS_OK;
}

*/


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





