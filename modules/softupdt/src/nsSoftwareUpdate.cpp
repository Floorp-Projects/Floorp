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
#include "fe_proto.h"
#include "xp.h"
#include "xp_str.h"
#include "prprf.h"
#include "softupdt.h"
#include "nsString.h"
#include "nsSoftwareUpdate.h"
#include "nsSoftUpdateEnums.h"
#include "nsInstallObject.h"
#include "nsInstallFile.h"
#include "nsInstallDelete.h"
#include "nsInstallExecute.h"
#include "nsInstallPatch.h"
#include "nsUninstallObject.h"
#include "nsSUError.h"
#include "nsWinProfile.h"
#include "nsWinReg.h"
#include "nsPrivilegeManager.h"
#include "nsTarget.h"
#include "nsPrincipal.h"
#include "zig.h"
#include "prefapi.h"
#include "proto.h"
#include "jsapi.h"
#include "xp_error.h"
#include "jsjava.h"
#include "xpgetstr.h"
#include "pw_public.h"

#include "VerReg.h"

#ifdef XP_MAC
#include "su_aplsn.h"
#endif

#ifdef XP_MAC
#pragma export on
#endif

extern int MK_OUT_OF_MEMORY;

extern int SU_ERROR_BAD_PACKAGE_NAME;
extern int SU_ERROR_WIN_PROFILE_MUST_CALL_START;
extern int SU_ERROR_BAD_PACKAGE_NAME_AS;
extern int SU_ERROR_EXTRACT_FAILED;
extern int SU_ERROR_NO_CERTIFICATE;
extern int SU_ERROR_TOO_MANY_CERTIFICATES;
extern int SU_ERROR_BAD_JS_ARGUMENT;
extern int SU_ERROR_SMART_UPDATE_DISABLED;
extern int SU_ERROR_UNEXPECTED;
extern int SU_ERROR_VERIFICATION_FAILED;
extern int SU_ERROR_MISSING_INSTALLER;
extern int SU_ERROR_OUT_OF_MEMORY;
extern int SU_ERROR_EXTRACT_FAILED;

/*****************************************
 * SoftwareUpdate progress dialog methods
 *****************************************/
#define TITLESIZE 256
extern int SU_INSTALLWIN_TITLE;
extern int SU_INSTALLWIN_UNPACKING;
extern int SU_INSTALLWIN_INSTALLING;

#ifdef XP_PC
extern char * WH_TempFileName(int type, const char * prefix, 
                              const char * extension); 
#endif

PR_BEGIN_EXTERN_C

static PRBool su_PathEndsWithSeparator(char* path, char* sep);


extern uint32 FE_DiskSpaceAvailable (MWContext *context, const char *lpszPath );


/* Public Methods */

/**
 * @param env                JavaScript environment (this inside the installer).
 *                           Contains installer directives
 * @param inUserPackageName  Name of tha package installed. 
 *                           This name is displayed to the user
 */
nsSoftwareUpdate::nsSoftwareUpdate(void* env, char* inUserPackageName)
{
  userPackageName = XP_STRDUP(inUserPackageName);
  installPrincipal = NULL;
  packageName = NULL;
  packageFolder = NULL;
  installedFiles = NULL;
  confdlg = NULL;
  progwin = NULL;
  patchList = new nsHashtable();
  zigPtr = NULL;
  versionInfo = NULL;
  userChoice = -1;
  lastError = 0;
  silent = PR_FALSE;
  force = PR_FALSE;
  jarName = NULL;
  jarURL = NULL;
  
  bUninstallPackage = PR_FALSE;
  bRegisterPackage	= PR_FALSE;
  bShowProgress		= PR_TRUE;
  bShowFinalize		= PR_TRUE;
  bUserCancelled	= PR_FALSE;
  
  char *errorMsg;
        
  /* Need to verify that this is a SoftUpdate JavaScript object */
  errorMsg = VerifyJSObject(env);

  /* XXX: FIX IT. How do we get data from env 
  jarName = (String) env.getMember("src");
  jarCharset = (String) env.getMember("jarCharset");
  jarURL = (String) env.getMember("srcURL");
  silent = ((PRBool) env.getMember("silent")).booleanValue();
  force  = ((PRBool) env.getMember("force")).booleanValue();
  */

#ifdef XP_PC
  filesep = "\\";
#elif defined(XP_MAC)
  filesep = ":";
#else
  filesep = "/";
#endif 
}

nsSoftwareUpdate::~nsSoftwareUpdate()
{
  if(patchList) 
	  delete patchList;
  if(packageFolder)
	delete packageFolder;
  if(versionInfo)
	delete versionInfo;

  CleanUp();
}

/* Gives its internal copy. Don't free the returned value */
nsPrincipal* nsSoftwareUpdate::GetPrincipal()
{
  return installPrincipal;
}

/* Gives its internal copy. Don't free the returned value */
char* nsSoftwareUpdate::GetUserPackageName()
{
  return userPackageName;
}

/* Gives its internal copy. Don't free the returned value */
char* nsSoftwareUpdate::GetRegPackageName()
{
  return packageName;
}

PRBool nsSoftwareUpdate::GetSilent()
{
  return silent;
}

/**
 * @return a vector of InstallObjects
 */
nsVector* nsSoftwareUpdate::GetInstallQueue()
{
  return installedFiles;
}

/**
 * @return  the most recent non-zero error code
 * @see ResetError
 */
PRInt32 nsSoftwareUpdate::GetLastError()
{
  return lastError;
}

/**
 * resets remembered error code to zero
 * @see GetLastError
 */
void nsSoftwareUpdate::ResetError()
{
  lastError = 0;
}

/**
 * @return the folder object suitable to be passed into AddSubcomponent
 * @param folderID One of the predefined folder names
 * @see AddSubcomponent
 */
nsFolderSpec* nsSoftwareUpdate::GetFolder(char* folderID, char* *errorMsg)
{
  nsFolderSpec* spec = NULL;
  errorMsg = NULL;

  if ((XP_STRCMP(folderID, "Installed") != 0) && 
      (XP_STRCMP(folderID, FOLDER_FILE_URL) != 0)) {
    spec = new nsFolderSpec(folderID, packageName, userPackageName);
    if (XP_STRCMP(folderID, "User Pick") == 0) {
      // Force the prompt
      char * ignore = spec->GetDirectoryPath();
      if (ignore == NULL) 
      {
        if(spec)
          delete spec;

        return NULL;
      }
    }
  }
  return spec;
}

/**
 * @return the full path to a component as it is known to the
 *          Version Registry
 * @param  component Version Registry component name
 */
nsFolderSpec* nsSoftwareUpdate::GetComponentFolder(char* component)
{
  int err;
  char* dir;
  char* qualifiedComponent;
  
  nsFolderSpec* spec = NULL;
  
  qualifiedComponent = GetQualifiedPackageName( component );
  
  if (qualifiedComponent == NULL)
  {
        return NULL;
  }
  
  dir = (char*)XP_CALLOC(MAXREGPATHLEN, sizeof(char));
  err = VR_GetDefaultDirectory( qualifiedComponent, MAXREGPATHLEN, dir );
  if (err != REGERR_OK)
  {
     XP_FREEIF(dir);
     dir = NULL;
  }

  if ( dir == NULL ) 
  {
    dir = (char*)XP_CALLOC(MAXREGPATHLEN, sizeof(char));
    err = VR_GetPath( qualifiedComponent, MAXREGPATHLEN, dir );
    if (err != REGERR_OK)
    {
        XP_FREEIF(dir);
        dir = NULL;
    }
    

    if ( dir != NULL ) {
      int i;

      nsString dirStr(dir);
      if ((i = dirStr.RFind(filesep)) > 0) {
        XP_FREEIF(dir);  
        dir = (char*)XP_ALLOC(i);
        dir = dirStr.ToCString(dir, i);
      }
    }
  }

  if ( dir != NULL ) 
  {
    /* We have a directory */
   
        if (su_PathEndsWithSeparator(dir, filesep)) 
        {
            spec = new nsFolderSpec("Installed", dir, userPackageName);
        } 
        else 
        {
           dir = XP_AppendStr(dir, filesep);
           spec = new nsFolderSpec("Installed", dir, userPackageName);
        }
    
        XP_FREEIF(dir);
  }
  return spec;
}

nsFolderSpec* nsSoftwareUpdate::GetComponentFolder(char* component, 
                                                   char* subdir, 
                                                   char* *errorMsg)
{
  nsFolderSpec* spec = GetComponentFolder( component );
  nsFolderSpec* ret_val = GetFolder( spec, subdir, errorMsg );
  
  if(spec);
	delete spec;

  return ret_val;
}

/**
 * sets the default folder for the package
 * @param folder a folder object obtained through GetFolder()
 */
void nsSoftwareUpdate::SetPackageFolder(nsFolderSpec* folder)
{
  packageFolder = folder;
}

/**
 * Returns a Windows Profile object if you're on windows,
 * null if you're not or if there's a security error
 */
void* nsSoftwareUpdate::GetWinProfile(nsFolderSpec* folder, 
                                      char* file, 
                                      char* *errorMsg)
{
#ifdef XP_PC
  nsWinProfile* profile = NULL;
  *errorMsg = NULL;

  SanityCheck(errorMsg);
  
  if (*errorMsg == NULL)
  {
      profile = new nsWinProfile(this, folder, file);
  }
  
  return profile;
#else
  return NULL;
#endif
}

/**
 * @return an object for manipulating the Windows Registry.
 *          Null if you're not on windows
 */
void* nsSoftwareUpdate::GetWinRegistry(char* *errorMsg)
{
#ifdef XP_PC
  nsWinReg* registry = NULL;

  SanityCheck(errorMsg);
  
  if (*errorMsg == NULL)
  {
      registry = new nsWinReg(this);
  }
  
  return registry;
#else
  return NULL;
#endif /* XP_PC */
}

/**
 * extract the file out of the JAR directory and places it into temporary
 * directory.
 * two security checks:
 * - the certificate of the extracted file must match the installer certificate
 * - must have privileges to extract the jar file
 *
 * Caller should free the returned string 
 *
 * @param inJarLocation  file name inside the JAR file
 */
char* nsSoftwareUpdate::ExtractJARFile(char* inJarLocation, 
                                       char* finalFile, 
                                       char* *errorMsg)
{
  if (zigPtr == NULL) {
    *errorMsg = SU_GetErrorMsg3("JAR file has not been opened", 
                                SUERR_UNKNOWN_JAR_FILE );
    return NULL;
  }

  /* Security checks */
  nsTarget* target;
  nsPrivilegeManager* privMgr = nsPrivilegeManager::getPrivilegeManager();
  target = nsTarget::findTarget( INSTALL_PRIV );
  if (target != NULL) {
    if (!privMgr->isPrivilegeEnabled(target, 1)) {
      return NULL;
    }
  }

  /* Make sure that the certificate of the extracted file matches
     the installer certificate */
  /* XXX this needs to be optimized, so that we do not create a principal
     for every certificate */

  /* XXX: Should we put up a dialog for unsigned JAR file support? */
  XP_Bool unsigned_jar_enabled;
  PREF_GetBoolPref("autoupdate.unsigned_jar_support", &unsigned_jar_enabled);

  {
    PRUint32 i;
    PRBool haveMatch = PR_FALSE;
    nsPrincipalArray* prinArray = 
      (nsPrincipalArray*)getCertificates(zigPtr, inJarLocation);

    PR_ASSERT(installPrincipal != NULL);
    if ((prinArray != NULL) && (prinArray->GetSize() > 0)) {
      PRUint32 noOfPrins = prinArray->GetSize();
      for (i=0; i < noOfPrins; i++) {
        nsPrincipal* prin = (nsPrincipal*)prinArray->Get(i);
        if (installPrincipal->equals( prin )) {
          haveMatch = PR_TRUE;
          break;
        }
      }
    }

    if ((haveMatch == PR_FALSE)) {
      char *msg = NULL;
      if (prinArray == NULL) {
        if (!unsigned_jar_enabled) {
          msg = PR_sprintf_append(msg, "Missing certificate for %s", 
                                  inJarLocation);
          *errorMsg = SU_GetErrorMsg3(msg, SUERR_NO_CERTIFICATE);
        }
      } else {
        msg = PR_sprintf_append(msg, "Missing certificate for %s", inJarLocation);
        *errorMsg = SU_GetErrorMsg3(msg, 
                                    SUERR_NO_MATCHING_CERTIFICATE);
      }
      PR_FREEIF(msg);
    }

    freeIfCertificates(prinArray);

    if (*errorMsg != NULL) 
      return NULL;
  }

  /* Extract the file */
  char* outExtractLocation = NativeExtractJARFile(inJarLocation, finalFile, 
                                                  errorMsg);
  return outExtractLocation;
}


void
nsSoftwareUpdate::ParseFlags(int flags)
{
 
    if ((flags & SU_NO_STATUS_DLG) == SU_NO_STATUS_DLG)
    {
        bShowProgress = PR_FALSE;
    }
    if ((flags & SU_NO_FINALIZE_DLG) == SU_NO_FINALIZE_DLG)
    {
        bShowFinalize = PR_FALSE;
    }
}

/**
 * Call this to initialize the update
 * Opens the jar file and gets the certificate of the installer
 * Opens up the gui, and asks for proper security privileges
 *
 * @param vrPackageName     Full qualified  version registry name of the package
 *                          (ex: "/Plugins/Adobe/Acrobat")
 *                          NULL or empty package names are errors
 *
 * @param inVInfo           version of the package installed.
 *                          Can be NULL, in which case package is installed
 *                          without a version. Having a NULL version, this 
 *                          package is automatically updated in the future 
 *                          (ie. no version check is performed).
 *
 * @param flags             Once was securityLevel(LIMITED_INSTALL or FULL_INSTALL).  Now
 *                          can be either NO_STATUS_DLG or NO_FINALIZE_DLG
 */
     
PRInt32 nsSoftwareUpdate::StartInstall(char* vrPackageName, 
                                       nsVersionInfo* inVInfo, 
                                       PRInt32 flags, 
                                       char* *errorMsg)
{
  int errcode= SU_SUCCESS;
  *errorMsg = NULL;
  ResetError();
  ParseFlags(flags);
  bUserCancelled = PR_FALSE; 
  packageName = NULL;

  if ( vrPackageName == NULL ) 
  {
    *errorMsg = SU_GetErrorMsg4(SU_ERROR_BAD_PACKAGE_NAME, 
                                SUERR_INVALID_ARGUMENTS );
    return SUERR_INVALID_ARGUMENTS;
  }
  
  packageName	= GetQualifiedPackageName( vrPackageName );
      
  int len = XP_STRLEN(packageName);
  int last_pos = len-1;
  char* tmpPackageName = new char[len+1];
  XP_STRCPY(tmpPackageName, packageName);
  
  
  while ((last_pos >= 0) && (tmpPackageName[last_pos] == '/')) 
  {
    // Make sure that package name does not end with '/'
    char* ptr = new char[last_pos+1];
    memcpy(tmpPackageName, ptr, last_pos);
    ptr[last_pos] = '\0';
    
	if(tmpPackageName);
		delete tmpPackageName;

    tmpPackageName = ptr;
    last_pos = last_pos - 1;
  }
  packageName = XP_STRDUP(tmpPackageName);
  if(tmpPackageName);
		delete tmpPackageName;
  
  /* delete the old nsVersionInfo object. */
  if(versionInfo); 
	delete versionInfo;

  versionInfo	= inVInfo;
  installedFiles = new nsVector();
      
  /* JAR initalization */
  /* open the file, create a principal out of installer file certificate */
  errcode = OpenJARFile(errorMsg);
  if (*errorMsg != NULL)
    return errcode;
  errcode = InitializeInstallerCertificate(errorMsg);
  if (*errorMsg != NULL)
    return errcode;
  CheckSilentPrivileges();
  errcode = RequestSecurityPrivileges(errorMsg);
  if (*errorMsg != NULL) 
    return errcode;

    if (bShowProgress)
    {
	    OpenProgressDialog();
	}
      
  // set up default package folder, if any
  int err;

  char*  path = (char*)XP_CALLOC(MAXREGPATHLEN, sizeof(char));
  err = VR_GetDefaultDirectory( packageName, MAXREGPATHLEN, path );
  if (err != REGERR_OK)
  {
     XP_FREEIF(path);
     path = NULL;
  }

  if ( path !=  NULL ) {
    packageFolder = new nsFolderSpec("Installed", path, userPackageName);
    XP_FREEIF(path); 
  }
  
  saveError( errcode );
  
  if (errcode != 0)
  {
      packageName = NULL; // Reset!
  }
  
  return errcode;
}

/*
 * another forms of StartInstall()
 */

PRInt32 nsSoftwareUpdate::StartInstall(char* vrPackageName, 
                                       char* inVer,
                                       PRInt32 flags, 
                                       char* *errorMsg)
{
 	return StartInstall(vrPackageName,  new nsVersionInfo( inVer ), flags, errorMsg);
}

/**
 * another StartInstall() simplification -- version as char*
 */
PRInt32 nsSoftwareUpdate::StartInstall(char* vrPackageName, char* inVer, 
                                       char* *errorMsg)
{
  return StartInstall( vrPackageName, new nsVersionInfo( inVer ), 0, errorMsg );
}

/*
 * UI feedback
 */
void nsSoftwareUpdate::UserCancelled()
{
  userChoice = 0;
}

void nsSoftwareUpdate::UserApproved()
{
  userChoice = 1;
}

/**
 * Proper completion of the install
 * Copies all the files to the right place
 * returns 0 on success, <0 error code on failure
 */
PRInt32 nsSoftwareUpdate::FinalizeInstall(char* *errorMsg)
{
  PRBool rebootNeeded = PR_FALSE;
  int result = SU_SUCCESS;
  *errorMsg = NULL;
  
  
  result = SanityCheck(errorMsg);
  
  if (*errorMsg != NULL)
  {
	saveError( result );
	return result;
  }
      
  if ( installedFiles == NULL || installedFiles->GetSize() == 0 ) {
    // no actions queued: don't register the package version
    // and no need for user confirmation
    
    CleanUp();
    return result; // XXX: a different status code here?
  }
      
  // Wait for user approval
  if ( !silent && UserWantsConfirm() ) {
#ifdef XXX /* XXX: RAMAN FIX IT */
    confdlg = new nsProgressDetails(this);

    /* XXX What is this? 
    while ( userChoice == -1 )
      Thread.sleep(10);
    */
#endif /* XXX */
          
    confdlg = NULL;
    if (userChoice != 1) {
      AbortInstall();
      return saveError(SUERR_USER_CANCELLED);
    }
  }
  
  // If the user passed NO_FINALIZE_DLG, we should close the progress dialog here.
  // If the user passed !NO_FINALIZE_DLG, we should open the progress dialog here.
	
  if ( bShowFinalize )
  {
    OpenProgressDialog();
  }
  else
  {
    CloseProgressDialog();
  }
	
  SetProgressDialogItem(""); // blank the "current item" line    
  SetProgressDialogRange( installedFiles->GetSize() );
  
      
  /* call Complete() on all the elements */
  /* If an error occurs in the middle, call Abort() on the rest */
  nsVector* ve = GetInstallQueue();
  nsInstallObject* ie = NULL;
      
  // Main loop
  int count = 0;
  
  if ( bUninstallPackage )
  {
	  VR_UninstallCreateNode( packageName, userPackageName );
  }

  PRUint32 i=0;
  for (i=0; i < ve->GetSize(); i++) {
    ie = (nsInstallObject*)ve->Get(i);
    if (ie == NULL)
      continue;
    *errorMsg = ie->Complete();
    if (*errorMsg != NULL) {
      ie->Abort();
      return SUERR_UNEXPECTED_ERROR;
    }
    SetProgressDialogThermo(++count);
  }
  // add overall version for package
  if ( (versionInfo != NULL) && (bRegisterPackage)) 
  {
    result = VR_Install( packageName, NULL, versionInfo->toString(), PR_FALSE );
    // Register default package folder if set
	if ( packageFolder != NULL )
	{
		VR_SetDefaultDirectory( packageName, packageFolder->toString() );
	}
  
  }
  CleanUp();
  
  if (result == SU_SUCCESS && rebootNeeded)
    result = SU_REBOOT_NEEDED;
  
  saveError( result );
  return result;
}

/**
 * Aborts the install :), cleans up all the installed files
 * XXX: This is a synchronized method. FIX it.
 */
void nsSoftwareUpdate::AbortInstall()
{
  nsInstallObject* ie;
  if (installedFiles != NULL) {
    nsVector* ve = GetInstallQueue();
    PRUint32 i=0;
    for (i=0; i < ve->GetSize(); i++) {
      ie = (nsInstallObject *)ve->Get(i);
      if (ie == NULL)
        continue;
      ie->Abort();
    }
  }
  CloseJARFile();
  CleanUp();
}

/**
 * ScheduleForInstall
 * call this to put an InstallObject on the install queue
 * Do not call installedFiles.addElement directly, because this routine also 
 * handles progress messages
 */
char* nsSoftwareUpdate::ScheduleForInstall(nsInstallObject* ob)
{
  char *errorMsg = NULL;
  char *objString = ob->toString();

  // flash current item
  SetProgressDialogItem( objString );

  XP_FREEIF(objString);

  // do any unpacking or other set-up
  errorMsg = ob->Prepare();
  if (errorMsg != NULL) {
    return errorMsg;
  }

  // Add to installation list if we haven't thrown out
  installedFiles->Add( ob );
  // if (confdlg != NULL)
  //    confdlg.ScheduleForInstall( ob );
  
  
	
  // turn on flags for creating the uninstall node and
  // the package node for each InstallObject
  if (ob->CanUninstall())
    bUninstallPackage = PR_TRUE;
	
  if (ob->RegisterPackageNode())
    bRegisterPackage = PR_TRUE;
  
  return NULL;
}

/**
 * Extract  a file from JAR archive to the disk, and update the
 * version registry. Actually, keep a record of elements to be installed. 
 * FinalizeInstall() does the real installation. Install elements are accepted 
 * if they meet one of the following criteria:
 *  1) There is no entry for this subcomponnet in the registry
 *  2) The subcomponent version info is newer than the one installed
 *  3) The subcomponent version info is NULL
 *
 * @param name      path of the package in the registry. Can be:
 *                    absolute: "/Plugins/Adobe/Acrobat/Drawer.exe"
 *                    relative: "Drawer.exe". Relative paths are relative to 
 *                    main package name NULL:  if NULL jarLocation is assumed 
 *                    to be the relative path
 * @param version   version of the subcomponent. Can be NULL
 * @param jarSource location of the file to be installed inside JAR
 * @param folderSpec one of the predefined folder   locations
 * @see GetFolder
 * @param relativePath  where the file should be copied to on the local disk.
 *                      Relative to folder
 *                      if NULL, use the path to the JAR source.
 * @param forceInstall  if true, file is always replaced
 */
PRInt32 nsSoftwareUpdate::AddSubcomponent(char* name, 
                                          nsVersionInfo* version, 
                                          char* jarSource,
                                          nsFolderSpec* folderSpec, 
                                          char* relativePath,
                                          PRBool forceInstall,
                                          char* *errorMsg)
{
  nsInstallFile* ie;
  int result = SU_SUCCESS;
  int err;  /* does not get saved */
  *errorMsg = NULL;
  char *new_name;

  if ( jarSource == NULL || (XP_STRLEN(jarSource) == 0) ) {
    *errorMsg = SU_GetErrorMsg3("No Jar Source", 
                                SUERR_INVALID_ARGUMENTS );
    return SUERR_INVALID_ARGUMENTS;
  }
      
  if ( folderSpec == NULL ) {
    *errorMsg = SU_GetErrorMsg3("folderSpec is NULL ", 
                                SUERR_INVALID_ARGUMENTS );
    return SUERR_INVALID_ARGUMENTS;
  }
   
   
  result = SanityCheck(errorMsg);
  
  if (*errorMsg == NULL)
  {
      saveError( result );
      return result;
  }    
  
      
  if ((name == NULL) || (XP_STRLEN(name) == 0)) {
    // Default subName = location in jar file
    new_name = GetQualifiedRegName( jarSource, errorMsg );
  } else {
    new_name = GetQualifiedRegName( name, errorMsg );
  }
  
  if (*errorMsg != NULL)
  {
  	return SUERR_BAD_PACKAGE_NAME;
  }
      
  if ( (relativePath == NULL) || (XP_STRLEN(relativePath) == 0) ) {
    relativePath = jarSource;
  }
      
  /* Check for existence of the newer	version	*/
      
  PRBool versionNewer = PR_FALSE;
  if ( (forceInstall == PR_FALSE ) &&
       (version !=  NULL) &&
       ( VR_ValidateComponent( new_name ) == 0 ) ) 
  {

    VERSION versionStruct;

    err = VR_GetVersion( new_name, &versionStruct );
        
    nsVersionInfo* oldVersion = new nsVersionInfo(versionStruct.major,
                                                  versionStruct.minor,
                                                  versionStruct.release,
                                                  versionStruct.build);
    
	if ( version->compareTo( oldVersion ) != nsVersionEnum_EQUAL )
      versionNewer = PR_TRUE;
      
	if( oldVersion )
		  delete oldVersion;

  } 
  else 
  {
    versionNewer = PR_TRUE;
  }
      
  if (versionNewer) {
    ie = new nsInstallFile( this, new_name, version, jarSource,
                            folderSpec, relativePath, forceInstall, 
                            errorMsg );
    if (errorMsg == NULL) {
      *errorMsg = ScheduleForInstall( ie );
    }
  }
  if (*errorMsg != NULL) {
    result = SUERR_UNEXPECTED_ERROR;
  }

  XP_FREEIF (new_name);
  saveError( result );
  return result;
}

/**
 * executes the file
 * @param jarSource     name of the file to execute inside JAR archive
 * @param args          command-line argument string (Win/Unix only)
 */
PRInt32 nsSoftwareUpdate::Execute(char* jarSource, char* *errorMsg, char* args)
{
  int errcode = SU_SUCCESS;
  
  errcode = SanityCheck(errorMsg);
  
  if (*errorMsg != NULL)
  {
    saveError( errcode );
    return errcode;
  }
  
  
  nsInstallExecute* ie = new nsInstallExecute(this, jarSource, errorMsg, args);
  if (*errorMsg != NULL) {
    errcode = SUERR_ACCESS_DENIED;
  } else {
    *errorMsg = ScheduleForInstall( ie );
    if (*errorMsg != NULL) {
      errcode = SUERR_UNEXPECTED_ERROR;
    }
  }
  
  saveError( errcode );
  return errcode;
}

#ifdef XP_MAC
#include <Gestalt.h>
#endif


/**
 * Mac-only, simulates Mac toolbox Gestalt function
 * OSErr Gestalt(char* selector, long * response)
 * @param selector      4-character string, 
 * @param os_err        corresponds to OSErr
 * @param errorMsg      Error message
 * @return              an integer corresponding to response from Gestalt
 */
PRInt32 nsSoftwareUpdate::Gestalt(char* selectorStr, int* os_err, 
                                  char* *errorMsg)
{
  *errorMsg = NULL;

#ifdef XP_MAC
  *os_err = noErr;
  long response = 0;
  OSType selector;
  
  if ((selectorStr == NULL) ||
      (XP_STRLEN(selectorStr) != 4))
    {
      *os_err = SUERR_GESTALT_UNKNOWN_ERR; /* gestaltUnknownErr */
      goto fail;
    }
  XP_MEMCPY(&selector, selectorStr, 4);
  *os_err = (int)Gestalt(selector, response);
  if (*os_err == noErr) {
    return response;
  }
  goto fail;	/* Drop through to failure */
#else
  *os_err = SUERR_GESTALT_INVALID_ARGUMENT;
  goto fail;
#endif

fail:
  *errorMsg = SU_GetErrorMsg4(SU_ERROR_EXTRACT_FAILED, *os_err );
  return 0;
}

/**
 * Patch
 *
 * nsVersionInfo object shouldn't be free'ed. nsSoftwareUpdate object 
 * deletes it.
 *
 */
PRInt32 nsSoftwareUpdate::Patch(char* regName, 
                                nsVersionInfo* version, 
                                char* patchname, 
                                char* *errorMsg)
{
  int errcode = SU_SUCCESS;
  
  if ( regName == NULL || patchname == NULL ) {
    *errorMsg = SU_GetErrorMsg3("regName or patchName is NULL ", 
                                SUERR_INVALID_ARGUMENTS );
    return saveError( SUERR_INVALID_ARGUMENTS );
  }
  
  errcode = SanityCheck(errorMsg);
  
  if (*errorMsg != NULL)
  {
  		saveError( errcode );
    	return errcode;
  }
    
  char* rname = GetQualifiedRegName( regName, errorMsg);
  if (*errorMsg != NULL) 
  {
  	errcode = SUERR_BAD_PACKAGE_NAME;
  	saveError( errcode );
    return errcode;
  }
  
  nsInstallPatch* ip = new nsInstallPatch(this, rname, version, patchname, 
                                          errorMsg);
  if (*errorMsg != NULL) {
    errcode = SUERR_ACCESS_DENIED;
  } else {
    *errorMsg = ScheduleForInstall( ip );
    if (*errorMsg != NULL) {
      errcode = SUERR_UNEXPECTED_ERROR;
    }
  }
  XP_FREEIF( rname );
  saveError( errcode );
  return errcode;
}

/* Patch
 *
 * nsVersionInfo object shouldn't be free'ed. nsSoftwareUpdate object 
 * deletes it.
 *
 */
PRInt32 nsSoftwareUpdate::Patch(char* regName, 
                                nsVersionInfo* version, 
                                char* patchname,
                                nsFolderSpec* folder, 
                                char* filename, 
                                char* *errorMsg)
{
  if ( folder == NULL || regName == NULL || 
       XP_STRLEN(regName) == 0 || patchname == NULL ) {
    *errorMsg = SU_GetErrorMsg3("folder or regName or patchName is NULL ", 
                                SUERR_INVALID_ARGUMENTS );
    return saveError( SUERR_INVALID_ARGUMENTS );
  }
  
  int errcode = SU_SUCCESS;
  
  errcode = SanityCheck(errorMsg);
  
  if (*errorMsg != NULL)
  {
  		saveError( errcode );
    	return errcode;
  }
  
  char* rname = GetQualifiedRegName( regName, errorMsg );
  if (*errorMsg != NULL) 
  {
    errcode = SUERR_BAD_PACKAGE_NAME;
    saveError( errcode );
  	return errcode;
  }
  
  
  nsInstallPatch* ip = new nsInstallPatch( this, rname, version,
                                           patchname, folder, filename, 
                                           errorMsg );
  if (*errorMsg != NULL) {
    errcode = SUERR_ACCESS_DENIED;
  } else {
    *errorMsg = ScheduleForInstall( ip );
    if (*errorMsg != NULL) {
      errcode = SUERR_UNEXPECTED_ERROR;
    }
  }
  XP_FREEIF ( rname );
  saveError( errcode );
  return errcode;
}

/**
 * This method deletes the specified file from the disk. It does not look
 * for the file in the registry and is guaranteed to muddle any uninstall 
 * reference counting.  Its main purpose is to delete files installed or 
 * created outside of SmartUpdate.
 */
PRInt32 nsSoftwareUpdate::DeleteFile(nsFolderSpec* folder, 
                                     char* relativeFileName, 
                                     char* *errorMsg)
{
  int errcode = SU_SUCCESS;
  
  errcode = SanityCheck(errorMsg);
  
  if (*errorMsg != NULL)
  {
  		saveError( errcode );
    	return errcode;
  }
  
  nsInstallDelete* id = new nsInstallDelete(this, folder, relativeFileName, 
                                            errorMsg);
  if (*errorMsg != NULL) {
    errcode = SUERR_ACCESS_DENIED;
  } else {
    *errorMsg = ScheduleForInstall( id );
    if (*errorMsg != NULL) {
      errcode = SUERR_UNEXPECTED_ERROR;
    }
  }
  /* XXX: who set FILE_DOES_NOT_EXIST errcode ?? */
  if (errcode == SUERR_FILE_DOES_NOT_EXIST) {
    errcode = SU_SUCCESS;
  }
  saveError( errcode );
  return errcode;
}

/**
 * This method finds named registry component and deletes both the file and the 
 * entry in the Client VR. registryName is the name of the component in the 
 * registry. Returns usual errors codes + code to indicate item doesn't exist 
 * in registry, registry item wasn't a file item, or the related file doesn't 
 * exist. If the file is in use we will store the name and to try to delete it 
 * on subsequent start-ups until we're successful.
 */
PRInt32 nsSoftwareUpdate::DeleteComponent(char* registryName, char* *errorMsg)
{
  int errcode = SU_SUCCESS;
  
  errcode = SanityCheck(errorMsg);
  
  if (*errorMsg != NULL)
  {
  		saveError( errcode );
    	return errcode;
  }
  
  char* rname = GetQualifiedRegName( registryName, errorMsg );
  if (*errorMsg != NULL) 
  {
	 errcode = SUERR_UNEXPECTED_ERROR;
	 saveError( errcode );
	return errcode;
  }

  nsInstallDelete* id = new nsInstallDelete(this, NULL, rname, errorMsg);
  if (*errorMsg != NULL) {
    errcode = SUERR_ACCESS_DENIED;
  } else {
    *errorMsg = ScheduleForInstall( id );
    if (*errorMsg != NULL) {
      errcode = SUERR_UNEXPECTED_ERROR;
    }
  }
  /* XXX: who set FILE_DOES_NOT_EXIST errcode ?? */
  if (errcode == SUERR_FILE_DOES_NOT_EXIST) {
    errcode = SU_SUCCESS;
  }
  
  XP_FREEIF (rname );
  saveError( errcode );
  return errcode;
}


static PRBool su_PathEndsWithSeparator(char* path, char* sep)
{
  PRBool ends_with_filesep = PR_FALSE;
  if (path != NULL) {
    PRInt32 filesep_len = XP_STRLEN(sep);
    PRInt32 path_len = XP_STRLEN(path);
    if (path_len >= filesep_len) {
      ends_with_filesep = (XP_STRSTR(&path[path_len - filesep_len], sep) 
                           ? PR_TRUE : PR_FALSE);
    }
  }
  return ends_with_filesep;
}


nsFolderSpec* nsSoftwareUpdate::GetFolder(char* targetFolder, 
                                          char* subdirectory, 
                                          char* *errorMsg)
{
  if (XP_STRCMP(targetFolder, FOLDER_FILE_URL) == 0) {
    char* newPath = NULL;
    char* path = NativeFileURLToNative("/", subdirectory );
    if ( path != NULL ) {
      nsFolderSpec *spec;
      if (su_PathEndsWithSeparator(path, filesep)) {
        spec = new nsFolderSpec("Installed", path, userPackageName);
      } else {
        path = XP_AppendStr(path, filesep);
        spec = new nsFolderSpec("Installed", path, userPackageName);
      }
      XP_FREEIF(path);
      return spec;
    } else {
      return NULL;
    }
  } else {
    nsFolderSpec* spec = GetFolder(targetFolder, errorMsg);
    nsFolderSpec* ret_val = GetFolder( spec, subdirectory, errorMsg );
    if (spec) 
	{
      delete spec;
    }
    return ret_val;
  }
}

nsFolderSpec* nsSoftwareUpdate::GetFolder(nsFolderSpec* folder, 
                                          char* subdir, 
                                          char* *errorMsg)
{
  nsFolderSpec* spec = NULL;
  char*         path = NULL;
  char*         newPath = NULL;
  
  if ( subdir == NULL || (XP_STRLEN(subdir) == 0 )) 
  {
    // no subdir, return what we were passed
    return folder;
  } 
  else if ( folder != NULL ) 
  {
    path = folder->MakeFullPath( subdir, errorMsg );
    if (path != NULL) {
      if (su_PathEndsWithSeparator(path, filesep)) {
        spec = new nsFolderSpec("Installed", path, userPackageName);
      } else {
        path = XP_AppendStr(path, filesep);
        spec = new nsFolderSpec("Installed", path, userPackageName);
      }
      XP_FREEIF(path);
    }
  }
  return spec;
}

/**
 * This method returns true if there is enough free diskspace, false if there 
 * isn't. The drive containg the folder is checked for # of free bytes.
 */
long nsSoftwareUpdate::DiskSpaceAvailable(nsFolderSpec* folder)
{
  char *errorMsg = NULL;
  char* path = folder->GetDirectoryPath();
  if (path == NULL)
    return 0;

  return NativeDiskSpaceAvailable(path);
}

/**
 * This method is used to install an entire subdirectory of the JAR. 
 * Any files so installed are individually entered into the registry so they
 * can be uninstalled later. Like AddSubcomponent the directory is installed
 * only if the version specified is greater than that already registered,
 * or if the force parameter is true.The final version presented is the complete
 * form, the others are for convenience and assume values for the missing
 * arguments.
 */
PRInt32 nsSoftwareUpdate::AddDirectory(char* name, 
                                       nsVersionInfo* version, 
                                       char* jarSource,   
                                       nsFolderSpec* folderSpec, 
                                       char* subdir, 
                                       PRBool forceInstall,
                                       char* *errorMsg)
{
  nsInstallFile* ie = NULL;
  int result = SU_SUCCESS;
  char *qualified_name = NULL;
  *errorMsg = NULL;

  if ( jarSource == NULL || XP_STRLEN(jarSource) == 0 || folderSpec == NULL ) {
    *errorMsg = SU_GetErrorMsg3("folder or Jarsource is NULL ", 
                                SUERR_INVALID_ARGUMENTS );
    return saveError(SUERR_INVALID_ARGUMENTS);
  }
      
  if (packageName == NULL) {
    // probably didn't call StartInstall()
    *errorMsg = SU_GetErrorMsg4(SU_ERROR_BAD_PACKAGE_NAME_AS, 
                                SUERR_BAD_PACKAGE_NAME );
    return saveError(SUERR_BAD_PACKAGE_NAME);
  }
      
  if ((name == NULL) || (XP_STRLEN(name) == 0)) {
    // Default subName = location in jar file
    qualified_name = GetQualifiedRegName( jarSource, errorMsg );
  } else {
    qualified_name = GetQualifiedRegName( name, errorMsg );
  }
  
  if (*errorMsg != NULL)
  {
  	return SUERR_BAD_PACKAGE_NAME;
  }
  
      
  if ( subdir == NULL ) {
    subdir = NULL;
  } else if ( XP_STRLEN(subdir) != 0 ) {
    subdir = XP_Cat(subdir, "/");
  }
      
      
  /* XXX: May be we should print the following to JS console
  System.out.println("AddDirectory " + qualified_name 
                     + " from " + jarSource + " into " + 
                     folderSpec.MakeFullPath(subdir));
  */
      
  
  int length;
  char** matchingFiles = ExtractDirEntries( jarSource, &length );
  int i;
  PRBool bInstall;
      
  for (i=0; i < length; i++) {
    /* XP_Cat allocates a new string and returns it */
    char* fullRegName = XP_Cat(qualified_name, "/", matchingFiles[i]);
          
    if ( (forceInstall == PR_FALSE) && (version != NULL) &&
         (VR_ValidateComponent(fullRegName) == 0) ) 
    {
      // Only install if newer
      VERSION versionStruct;
      VR_GetVersion( fullRegName, &versionStruct );

      
      nsVersionInfo* oldVer = new nsVersionInfo(versionStruct.major,
                                                versionStruct.minor,
                                                versionStruct.release,
                                                versionStruct.build);
         
      bInstall = ( version->compareTo(oldVer) > 0 );
      
	  if (oldVer)
		  delete oldVer;

    } else {
      // file doesn't exist or "forced" install
      bInstall = PR_TRUE;
    }
          
    if ( bInstall ) {
      char *newJarSource = XP_Cat(jarSource, "/", matchingFiles[i]);
      char *newSubDir;
      if (subdir) {
        newSubDir = XP_Cat(subdir,  matchingFiles[i]);
      } else {
        newSubDir = XP_STRDUP(matchingFiles[i]);
      }
      ie = new nsInstallFile( this, 
                              fullRegName, 
                              version,
                              newJarSource,
                              (nsFolderSpec*) folderSpec, 
                              newSubDir, 
                              forceInstall,
                              errorMsg);
      if (*errorMsg == NULL) {
        ScheduleForInstall( ie );
      } else {
        /* We have an error and we haven't scheduled, 
         * thus we can delete it 
         */
        if (ie)
			delete ie;
      }
      XP_FREEIF(newJarSource);
      XP_FREEIF(newSubDir);
    }
    XP_FREEIF(fullRegName);
  }
  XP_FREEIF(subdir);
  XP_FREEIF(qualified_name);
  if (errorMsg != NULL) {
    result = SUERR_UNEXPECTED_ERROR;
  }
  
  saveError( result );
  return (result);
}

/* Uninstall */
PRInt32 nsSoftwareUpdate::Uninstall(char* packageName, char* *errorMsg)
{
  int errcode = SU_SUCCESS;
  
  errcode = SanityCheck(errorMsg);
  
  if (*errorMsg != NULL)
  {
	 saveError( errcode );
	 return errcode;
  }
  
  char* rname = GetQualifiedPackageName( packageName );
   
  nsUninstallObject* u = new nsUninstallObject( this, rname, errorMsg );
  if (*errorMsg != NULL) {
    errcode = SUERR_UNEXPECTED_ERROR;
  } else {
    ScheduleForInstall( u );
  }
  
  XP_FREEIF (rname);
  
  saveError( errcode );
  return errcode;
}

/*******************************
 *
 * progress window
 *
 * functions for dealing with the progress window.
 * normally I'd make this an object, but since we're implementing
 * it with native routines and will soon be getting rid of Java
 * altogether this makes more sense for now. Creating a new object
 * would only lead to more JRI hell, especially on the Mac
 *******************************/
void nsSoftwareUpdate::OpenProgressDialog(void)
{
  if ( !silent && progwin == 0) {
    progwin = NativeOpenProgDlg( GetUserPackageName() );
  }
}

void nsSoftwareUpdate::CloseProgressDialog(void)
{
  if ( progwin != NULL ) {
    NativeCloseProgDlg( progwin );
    progwin = NULL;
  }
}

void nsSoftwareUpdate::SetProgressDialogItem(char* message)
{
  if ( progwin != NULL ) {
    NativeSetProgDlgItem( progwin, message );
  }
}

void nsSoftwareUpdate::SetProgressDialogRange(PRInt32 max)
{
  if ( progwin != NULL ) {
    NativeSetProgDlgRange( progwin, max );
  }
}

void nsSoftwareUpdate::SetProgressDialogThermo(PRInt32 value)
{
  if ( progwin != NULL ) {
    NativeSetProgDlgThermo( progwin, value );
  }
}

/* Private Methods */

/*
 * Reads in the installer certificate, and creates its principal
 */
PRInt32 nsSoftwareUpdate::InitializeInstallerCertificate(char* *errorMsg)
{
  PRInt32 errcode;
  nsPrincipal *prin = NULL;

  *errorMsg = NULL;

  /* XXX: Should we put up a dialog for unsigned JAR file support? */
  XP_Bool unsigned_jar_enabled;
  PREF_GetBoolPref("autoupdate.unsigned_jar_support", &unsigned_jar_enabled);

  nsPrincipalArray* prinArray = 
    (nsPrincipalArray*)getCertificates(zigPtr, installerJarName);
  if ((prinArray == NULL) || (prinArray->GetSize() == 0)) {
    if (!unsigned_jar_enabled) {
      *errorMsg = SU_GetErrorMsg4(SU_ERROR_NO_CERTIFICATE, 
                                  SUERR_NO_INSTALLER_CERTIFICATE);
      errcode = SUERR_NO_INSTALLER_CERTIFICATE;
    }
  } else if (prinArray->GetSize() > 1) {
    *errorMsg = SU_GetErrorMsg4(SU_ERROR_TOO_MANY_CERTIFICATES, 
                                SUERR_TOO_MANY_CERTIFICATES);
    errcode = SUERR_TOO_MANY_CERTIFICATES;
  } else {
    prin = (nsPrincipal *)prinArray->Get(0);
    if ((prin == NULL) && (!unsigned_jar_enabled)) {
      *errorMsg = SU_GetErrorMsg4(SU_ERROR_NO_CERTIFICATE, 
                                  SUERR_NO_INSTALLER_CERTIFICATE);
      errcode = SUERR_NO_INSTALLER_CERTIFICATE;
    }
  }

  if (prin != NULL) {
    /* XXX: We should have Dup method on nsPrincipal object */
    nsPrincipalType prinType = prin->getType();
    void* key = prin->getKey();
    PRUint32 key_len = prin->getKeyLength();
    installPrincipal = new nsPrincipal(prinType, key, key_len);
  } else {
    /* We should get the URL where we are getting the JarURL from */
    PR_ASSERT(jarURL != NULL);
    /* Create a codebase principal with the JAR URL */
    installPrincipal = new nsPrincipal(nsPrincipalType_CodebaseExact, 
                                       jarURL, XP_STRLEN(jarURL));
  }
  PR_ASSERT(installPrincipal != NULL);
  
  /* Free the allocated principals */
  freeIfCertificates(prinArray);
  return saveError(errcode);
}

/*
 * checks if our principal has privileges for silent install
 */
PRBool nsSoftwareUpdate::CheckSilentPrivileges()
{
  PRBool ret_val = PR_FALSE;

  if (silent == PR_FALSE) {
    return ret_val;
  }
 
  /* Request impersonation privileges */
  nsTarget* impersonation = nsTarget::findTarget(IMPERSONATOR);
  nsPrivilegeManager* privMgr = nsPrivilegeManager::getPrivilegeManager();
  if ((privMgr != NULL) && (impersonation != NULL)) {
    privMgr->enablePrivilege(impersonation, 1);
    
    nsTarget* target = nsTarget::findTarget(SILENT_PRIV);
    /* check the security permissions */
    if (target != NULL) {
      ret_val = privMgr->enablePrivilege(target, GetPrincipal(), 1);
    }
  }
  if (!ret_val) {
    silent = PR_FALSE;
  }
  return ret_val;
}

/* Request the security privileges, so that the security dialogs
 * pop up
 */
PRInt32 nsSoftwareUpdate::RequestSecurityPrivileges(char* *errorMsg)
{
  PRBool ret_val = PR_FALSE;
  /* Request impersonation privileges */
  nsTarget* impersonation = nsTarget::findTarget(IMPERSONATOR);
  nsPrivilegeManager* privMgr = nsPrivilegeManager::getPrivilegeManager();
  if ((privMgr != NULL) && (impersonation != NULL)) {
    privMgr->enablePrivilege(impersonation, 1);
    
    nsTarget* target = nsTarget::findTarget(INSTALL_PRIV);
    /* check the security permissions */
    if (target != NULL) {
      ret_val = privMgr->enablePrivilege(target, GetPrincipal(), 1);
    }
  }

  if (!ret_val) {
    *errorMsg = "Permssion was denied";
    return SUERR_ACCESS_DENIED;
  } else {
    *errorMsg = NULL;
  }
  return SU_SUCCESS;
}

/**
 * saves non-zero error codes so they can be seen by GetLastError()
 */
PRInt32 nsSoftwareUpdate::saveError(PRInt32 errcode)
{
  if ( errcode != SU_SUCCESS ) 
    lastError = errcode;
  return errcode;
}

/*
 * CleanUp
 * call	it when	done with the install
 *
 * XXX: This is a synchronized method. FIX it.
 */
void nsSoftwareUpdate::CleanUp()
{
  nsInstallObject* ie;
  CloseJARFile();
  confdlg = NULL;
  zigPtr = NULL;
  if ( installedFiles != NULL ) {
    PRUint32 i=0;
    for (; i < installedFiles->GetSize(); i++) {
      ie = (nsInstallObject*)installedFiles->Get(i);
      XP_FREEIF (ie);
    }
    installedFiles->RemoveAll();
    XP_FREEIF (installedFiles);
    installedFiles = NULL;
  }
  XP_FREEIF(packageName);
  packageName = NULL;  // used to see if StartInstall() has been called

  CloseProgressDialog();
}

/**
 * GetQualifiedRegName
 *
 * Allocates a new string and returns it. Caller is supposed to free it
 *
 * This routine converts a package-relative component registry name
 * into a full name that can be used in calls to the version registry.
 */
char* nsSoftwareUpdate::GetQualifiedRegName(char* name, char**errorMsg)
{

  char *comm = "=COMM=/";
  PRUint32 comm_len = XP_STRLEN(comm);
  char *usr = "=USER=/";
  PRUint32 usr_len = XP_STRLEN(usr);
  
  
  if ((XP_STRLEN(name)) >= comm_len) 
  {
    PRUint32 i;
    for (i=0; i<comm_len; i++) 
    {
      if (XP_TO_UPPER(name[i]) != comm[i])
        break;
    }
    if (i == comm_len) 
    {
      name = XP_STRDUP(&name[comm_len]);
    }
  }
  else if ((XP_STRLEN(name)) >= usr_len) 
  {
    PRUint32 i;
    for (i=0; i<usr_len; i++) 
    {
      if (XP_TO_UPPER(name[i]) != usr[i])
        break;
    }
    if (i == usr_len) 
    {
      name = XP_STRDUP(&name[usr_len]);
    }
  }  
  else if (name[0] != '/') 
  {
    // Relative package path
    char* packagePrefix;
    if (XP_STRLEN(packageName) != 0) {
      packagePrefix = XP_Cat(packageName, "/");
      name = XP_AppendStr(packagePrefix, name);
    } else {
      name = XP_STRDUP(name);
    }
  }
  
  
  if (BadRegName(name)) 
  {
  	*errorMsg = SU_GetErrorMsg4(SUERR_BAD_PACKAGE_NAME, SUERR_BAD_PACKAGE_NAME);
  	name = NULL;
  }
  
  return name;
}

/* Private Native methods */

/* VerifyJSObject
 * Make sure that JSObject is of type SoftUpdate.
 * Since SoftUpdate JSObject can only be created by C code
 * we cannot be spoofed
 */
char* nsSoftwareUpdate::VerifyJSObject(void* jsObj)
{
  char *errorMsg = NULL;
  /* Get the object's class, and verify that it is of class SoftUpdate
   */
#ifdef XXX
  JSObject * jsobj;
  JSClass * jsclass;
  jsobj = (JSObject *) jsObj;
  /*	jsobj = JSJ_ExtractInternalJSObject(env, (HObject*)a);*/
  jsclass = JS_GetClass(jsobj);
  /* XXX Fix it */
  if ( jsclass != &lm_softup_class ) {
    errorMsg = SU_GetErrorMsg4(SU_ERROR_BAD_JS_ARGUMENT, -1);
  }
#endif
  return errorMsg;
}

/* Open/close the jar file
 */
PRInt32 nsSoftwareUpdate::OpenJARFile(char* *errorMsg)
{
  ZIG * jarData;
  char * jarFile;
  PRInt32 err;
  XP_Bool enabled;

  *errorMsg = NULL;
  
  /* XXX: Should we put up a dialog for unsigned JAR file support? */
  XP_Bool unsigned_jar_enabled;
  PREF_GetBoolPref("autoupdate.unsigned_jar_support", &unsigned_jar_enabled);

  PREF_GetBoolPref( AUTOUPDATE_ENABLE_PREF, &enabled);
  
  if (!enabled)	{
    *errorMsg = SU_GetErrorMsg4(SU_ERROR_BAD_JS_ARGUMENT, 
                                SUERR_INVALID_ARGUMENTS);
    return SUERR_INVALID_ARGUMENTS;
  }
  
  jarFile = jarName;
  if (jarFile == NULL) {
    /* error already signaled */
    *errorMsg = SU_GetErrorMsg3("No Jar Source", 
                                SUERR_INVALID_ARGUMENTS);
    return SUERR_INVALID_ARGUMENTS;
  }
  
  /* Open and initialize the JAR archive */
  jarData = SOB_new();
  if ( jarData == NULL ) {
    *errorMsg = SU_GetErrorMsg3("No Jar Source", 
                                SUERR_UNEXPECTED_ERROR);
    return SUERR_UNEXPECTED_ERROR;
  }
  err = SOB_pass_archive( ZIG_F_GUESS, 
                          jarFile, 
                          NULL, /* realStream->fURL->address,  */
                          jarData );
  if ( err != 0 ) {
    if (unsigned_jar_enabled) 
      return SU_SUCCESS;
    *errorMsg = SU_GetErrorMsg4(SU_ERROR_VERIFICATION_FAILED, err);
    return err;
  }
  
  /* Get the installer file name */
  installerJarName = NULL;
  err = SOB_get_metainfo(jarData, NULL, INSTALLER_HEADER, 
                         (void**)&installerJarName, &installerJarNameLength);
  if (err != 0) {
    if (unsigned_jar_enabled) 
      return SU_SUCCESS;
    *errorMsg = SU_GetErrorMsg4(SU_ERROR_MISSING_INSTALLER, err);
    return err;
  }
  zigPtr = jarData;
  return SU_SUCCESS;
}

void nsSoftwareUpdate::CloseJARFile()
{
  ZIG* jarData = (ZIG*)zigPtr;
  if (jarData != NULL)
    SOB_destroy( jarData);
  zigPtr = NULL;
}

/* getCertificates
 * native encapsulation that calls AppletClassLoader.getCertificates
 * we cannot call this method from Java because it is private.
 * The method cannot be made public because it is a security risk
 */
void* nsSoftwareUpdate::getCertificates(void* zigPtr, char* pathname)
{
  return nsPrincipal::getSigners(zigPtr, pathname);
}

void nsSoftwareUpdate::freeIfCertificates(void* prins)
{
  nsPrincipalArray* prinArray = (nsPrincipalArray*)prins;
  if ((prinArray == NULL) || (prinArray->GetSize() == 0))
    return;

  PRUint32 noOfPrins = prinArray->GetSize();
  /* Free all the objects */
  PRUint32 i;
  for (i=0; i < noOfPrins; i++) {
    nsPrincipal* prin = (nsPrincipal*)prinArray->Get(i);
    XP_FREEIF (prin);
    prinArray->Set(i, NULL);
  }
  XP_FREEIF (prinArray);
}

#define APPLESINGLE_MAGIC_HACK 1		/* Hack to automatically detect applesingle files until we get tdell to do the right thing */


/* Caller should free the returned string */
char* nsSoftwareUpdate::NativeExtractJARFile(char* inJarLocation, 
                                             char* finalFile, 
                                             char* *errorMsg)
{
  char * tempName = NULL;
  char * target = NULL;
  char * ret_value = NULL;
  int result;
  ZIG * jar;
  char * jarPath;
  
  /* Extract the file */
  jar = (ZIG *)zigPtr;
  jarPath = (char*)inJarLocation;
  if (jarPath == NULL) {
    /* out-of-memory error already signaled */
    *errorMsg = SU_GetErrorMsg4(SU_ERROR_OUT_OF_MEMORY, 
                                SUERR_UNEXPECTED_ERROR);
    return NULL;
  }
  
  target = (char*)finalFile;
  
  if (target) {
    char *fn;
    char *end;
    char *URLfile = XP_PlatformFileToURL(target);
      
    fn = URLfile+7; /* skip "file://" part */
      
    if ((end = XP_STRRCHR(fn, '/')) != NULL )
      end[1] = 0;
      
    /* Create a temporary location */
    tempName = WH_TempName( xpURL, fn );
    XP_FREEIF(URLfile);
  } else {
#ifdef XP_PC
    char * extension = XP_STRRCHR(jarPath, '.');
    if (extension)
      tempName = WH_TempFileName( xpURL, NULL, extension ); 
    else
      tempName = WH_TempName( xpURL, NULL ); 
#else
    tempName = WH_TempName( xpURL, NULL );
#endif
  }
  if (tempName == NULL)	{
    result = MK_OUT_OF_MEMORY;
    goto done;
  }
  
  
  result = SOB_verified_extract( jar, jarPath, tempName);
    
  if ( result == 0 ) {
    /* If we have an applesingle file
     * decode it to a new file
     */
    char * encodingName;
    unsigned long encodingNameLength;
    XP_Bool isApplesingle = FALSE;
    result = SOB_get_metainfo( jar, NULL, CONTENT_ENCODING_HEADER, 
                               (void**)&encodingName, &encodingNameLength);
        
#ifdef APPLESINGLE_MAGIC_HACK
    if (result != 0) {
      XP_File f;
      uint32 magic;
      f = XP_FileOpen(tempName, xpURL, XP_FILE_READ_BIN);
      XP_FileRead( &magic, sizeof(magic), f);
      XP_FileClose(f);
      isApplesingle = (magic == 0x00051600 );
      result = 0;
    }
#else
    isApplesingle = (( result == 0 ) &&
                     (XP_STRNCMP(APPLESINGLE_MIME_TYPE, encodingName, 
                                 XP_STRLEN( APPLESINGLE_MIME_TYPE ) == 0)));
#endif
    if ( isApplesingle ) {
      /* We have an AppleSingle file */
      /* Extract it to the new AppleFile, and get the URL back */
      char * newTempName = NULL;
#ifdef XP_MAC
      result = SU_DecodeAppleSingle(tempName, &newTempName);
      if ( result == 0 ) {
        XP_FileRemove( tempName, xpURL );
        XP_FREEIF(tempName);
        tempName = newTempName;
      } else {
        XP_FileRemove( tempName, xpURL );
      }
#else
      result = 0;
#endif
    }
  }
  
done:
  XP_Bool unsigned_jar_enabled;
  PREF_GetBoolPref("autoupdate.unsigned_jar_support", &unsigned_jar_enabled);

  /* Create the return Java string if everything went OK */
  if (tempName) {
    if (result == 0) {
      ret_value = tempName;
    } else {
      if (unsigned_jar_enabled) {
        ret_value = tempName;
      }
    }
  } 
  
  if ( (result != 0) && (!unsigned_jar_enabled) ) {
    *errorMsg = SU_GetErrorMsg4(SU_ERROR_EXTRACT_FAILED, result);
    return NULL;
  }
  
  if (ret_value == NULL) {
    *errorMsg = SU_GetErrorMsg4(SU_ERROR_EXTRACT_FAILED, result);
    return NULL;
  }
  
  return ret_value;
}

PRInt32 nsSoftwareUpdate::NativeMakeDirectory(char* dir)
{
  if ( !dir )
    return -1;
  
  return XP_MakeDirectoryR ( dir, xpURL );
}

long nsSoftwareUpdate::NativeDiskSpaceAvailable(char* fileSystem)
{
  PRInt32 val = 0;
  
  if (fileSystem) {
    val = FE_DiskSpaceAvailable(NULL, fileSystem);
  }
  return val;
}

/* Caller should free the returned string */
char* nsSoftwareUpdate::NativeFileURLToNative(char* dir, char* pathNamePlatform)
{
  char * newName = NULL;
  
  if (pathNamePlatform != NULL) {
    newName = WH_FileName(pathNamePlatform, xpURL);
  }
  return newName;
}

char** nsSoftwareUpdate::ExtractDirEntries(char *Directory, int *length)
{
  int         size = 0;
  int         len = 0;
  int         dirlen;
  char        *pattern = NULL;
  ZIG_Context *context;
  SOBITEM     *item;
  char*       *StrArray = NULL;
  char        *buff;
  
  if (!zigPtr)
    goto bail;
  
  if (!Directory)
    goto bail;
  
  dirlen = XP_STRLEN(Directory);
  if ( (pattern = (char *)XP_ALLOC(dirlen + 3)) == NULL)
    goto bail;
  
  XP_STRCPY(pattern, Directory);
  XP_STRCPY(pattern+dirlen, "/*");
  
  
  /* it's either go through the JAR twice (first time just to get a count)
   * or go through once and potentially use lots of memory saving all the
   * strings.  In deference to Win16 and potentially large installs we're
   * going to loop through the JAR twice and take the performance hit;
   * no one installs very often anyway.
   */
  
  if ((context = SOB_find ((ZIG*)zigPtr, pattern, ZIG_MF)) == NULL) 
    goto bail;
  
  while (SOB_find_next (context, &item) >= 0) 
    size++;
  
  SOB_find_end (context);
  
  StrArray = (char **)XP_CALLOC(size, sizeof(char *));
  if (StrArray == NULL)
    goto bail;
  
  if ((context = SOB_find ((ZIG*)zigPtr, pattern, ZIG_MF)) == NULL) 
    goto bail;

  *length = size;
  size = 0;
  while (SOB_find_next (context, &item) >= 0) {
    len = XP_STRLEN(item->pathname);
    /* subtract length of target directory + slash */
    len = len - (dirlen+1);
    if (( buff = (char*)XP_ALLOC (len+1)) != NULL ) {
      /* Don't copy the search directory part */
      XP_STRCPY (buff, (item->pathname)+dirlen+1);
      StrArray[size++] = buff;
    }
  }
  SOB_find_end (context);
  XP_FREEIF(pattern);
  return StrArray;

bail:
  XP_FREEIF(pattern);
  if (StrArray != NULL) {
    size = *length;
    for (int i=0; i<size; i++) {
      XP_FREEIF(StrArray[i]);
    }
  }
  StrArray = NULL;
  *length = 0;
  return StrArray;
}

void* nsSoftwareUpdate::NativeOpenProgDlg(char* name)
{
  pw_ptr prgwin = NULL;
  char buf[TITLESIZE];

  prgwin = PW_Create( XP_FindContextOfType(NULL, MWContextBookmarks), 
                      pwStandard );

  if ( prgwin != NULL ) {
    PR_snprintf(buf, TITLESIZE, XP_GetString(SU_INSTALLWIN_TITLE), name);

    PW_SetWindowTitle( prgwin, buf );
    PW_SetLine2( prgwin, NULL );

    PW_Show( prgwin );

    /* for some reason must be after PW_Show() to work */
    PW_SetLine1( prgwin, XP_GetString(SU_INSTALLWIN_UNPACKING) );
  }

  return (void *)prgwin;
}

void nsSoftwareUpdate::NativeCloseProgDlg(void* progptr)
{
  PW_Destroy( (pw_ptr)progptr );
}

void nsSoftwareUpdate::NativeSetProgDlgItem(void* progptr, char* message)
{
  PW_SetLine2( (pw_ptr)progptr, message );
}

void nsSoftwareUpdate::NativeSetProgDlgRange(void* progptr, PRInt32 max)
{
  PW_SetLine1( (pw_ptr)progptr, XP_GetString(SU_INSTALLWIN_INSTALLING) );
  PW_SetProgressRange( (pw_ptr)progptr, 0, max );
  PW_SetProgressValue( (pw_ptr)progptr, max );
  PW_SetProgressValue( (pw_ptr)progptr, 0 );
}

void nsSoftwareUpdate::NativeSetProgDlgThermo(void* progptr, PRInt32 value)
{
  PW_SetProgressValue( (pw_ptr)progptr, value );
}

PRBool nsSoftwareUpdate::UserWantsConfirm()
{
  XP_Bool bConfirm;
  PREF_GetBoolPref( AUTOUPDATE_CONFIRM_PREF, &bConfirm);

  if (bConfirm)
    return PR_TRUE;
  else
    return PR_FALSE;
}



 /**
 * GetQualifiedPackageName
 *
 * This routine converts a package-relative component registry name
 * into a full name that can be used in calls to the version registry.
 */
char*
nsSoftwareUpdate::GetQualifiedPackageName( char* name )
{
 	char* qualifedName;
 	
 	if (XP_STRNCMP(name, "=USER=/", XP_STRLEN( name )) == 0)
    {
		char* currentUserNode;

    	currentUserNode = CurrentUserNode();
    	
    	qualifedName = XP_STRDUP(currentUserNode);

		/* Append the the the path to the CurrentUserName.
		   name[7], should be after the =USER=/   
		*/

    	qualifedName = XP_AppendStr(qualifedName, &name[7]);
    
    	XP_FREEIF (currentUserNode);
    }
	
	if (BadRegName(qualifedName)) 
	{
		XP_FREEIF (qualifedName);
		qualifedName = NULL;     
	}

    return qualifedName;
}



char*
nsSoftwareUpdate::CurrentUserNode()
{
	char *qualifedName;
	char *profileName;

	qualifedName = XP_STRDUP("/Netscape/Users/");
	profileName  = NativeProfileName();
	
	XP_AppendStr(qualifedName, profileName);
	XP_AppendStr(qualifedName, "/");
    
	XP_FREEIF(profileName);

    return qualifedName;
}

char* 
nsSoftwareUpdate::NativeProfileName( ) 
{
    char *profname;
    int len = MAXREGNAMELEN;
    int err;

	profname = (char*) malloc(len);

    err = PREF_GetCharPref( "profile.name", profname, &len );

    if ( err != PREF_OK )
    {
		XP_FREEIF(profname);
		profname = NULL;
	}

	return profname;
}


/**
 * SanityCheck
 *
 * This routine checks if the packageName is null. It also checks the flag if the user cancels
 * the install progress dialog is set and acccordingly aborts the install.
 */
int
nsSoftwareUpdate::SanityCheck(char**errorMsg)
{
    if ( packageName == NULL ) 
    {
        *errorMsg = SU_GetErrorMsg4(SUERR_INSTALL_NOT_STARTED, SUERR_INSTALL_NOT_STARTED);
        return SUERR_INSTALL_NOT_STARTED;	
    }

    if (bUserCancelled) 
    {
        AbortInstall();
        saveError(SUERR_USER_CANCELLED);
        *errorMsg = SU_GetErrorMsg4(SUERR_USER_CANCELLED, SUERR_USER_CANCELLED);
        return SUERR_USER_CANCELLED;
    }
	
	return 0;
}




// catch obvious registry name errors proactively
// rather than returning some cryptic libreg error
PRBool 
nsSoftwareUpdate::BadRegName(char* regName)
{
    long regNameLen;
    
    if (regName== NULL)
	    return PR_TRUE;
	
	regNameLen = XP_STRLEN(regName);
	
    if ((regName[0] == ' ' ) || (regName[regNameLen] == ' ' ))
        return PR_TRUE;
        
    if ( XP_STRSTR(regName, "//") != NULL )
        return PR_TRUE;
     
    if ( XP_STRSTR(regName, " /") != NULL )
        return PR_TRUE;

    if ( XP_STRSTR(regName, "/ ") != NULL )
        return PR_TRUE;        
    
    if ( XP_STRSTR(regName, "=") != NULL )
        return PR_TRUE;           
  
    
    return PR_FALSE;
}

static JSClass stringObject = {
    "global", 0,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

JSObject*  
nsSoftwareUpdate::LoadStringObject(const char* filename)
{
	int err = 0;
	XP_File fp;
	XP_StatStruct stats;
	long fileLength;
    
    JSObject  *stringObj = NULL;
    MWContext *cx;

    /* XXX FIX: We should use the context passed into nsSoftwareUpdate via the env var! dft */
    cx = XP_FindContextOfType(NULL, MWContextJava);


    stringObj = JS_NewObject(cx->mocha_context, &stringObject, NULL, NULL);
    if (stringObj == NULL) return NULL;

    /* Now lets open of the file and read it into a buffer */

	stats.st_size = 0;
	if ( stat(filename, (struct stat *) &stats) == -1 )
		return NULL;

	fileLength = stats.st_size;
	if (fileLength <= 1)
		return NULL;
	
    fp = XP_FileOpen(filename, xpURL, "r");

	if (fp) 
    {	

		char* readBuf = (char *) XP_ALLOC(fileLength * sizeof(char) + 1);
		char* buffPtr;
        char* buffEnd;
        char* valuePtr;
        char* tempPtr;

        if (readBuf) 
        {
			fileLength = XP_FileRead(readBuf, fileLength, fp);
            readBuf[fileLength+1] = 0;

            buffPtr = readBuf;
            buffEnd = readBuf + fileLength;

            while (buffPtr < buffEnd)
            {
                
                /* Loop until we come across a interesting char */
                if (XP_IS_SPACE(*buffPtr))
                {
                    buffPtr++;
                }
                else
                {
                    /* cool we got something.  lets find its value, and then add it to the js object */
                    valuePtr = XP_STRCHR(buffPtr, '=');
                    if (valuePtr != NULL)
                    {
                        /* lets check to see if we hit a new line prior to this = */
                    	tempPtr = XP_STRCHR(buffPtr, '\n');
                    	
                    	
                   		if (tempPtr == NULL || tempPtr > valuePtr)
                    	{
                            *valuePtr = 0;  /* null out the last char    */
                            valuePtr++;     /* point it pass the = sign  */
                       
                            /* Make sure that the Value is nullified. */
                            tempPtr = XP_STRCHR(valuePtr, CR);
                            if (tempPtr)
                            {
                                tempPtr = 0;
                            }
                            else
                            {
                                /* EOF? */
                            }
                        
                            /* we found both the name and value, lets add it! */

                            JS_DefineProperty(  cx->mocha_context, 
                                                stringObj, 
                                                buffPtr, 
                                                valuePtr?STRING_TO_JSVAL(JS_NewStringCopyZ(cx->mocha_context, valuePtr)):JSVAL_VOID,
		                                        NULL, 
                                                NULL, 
                                                JSPROP_ENUMERATE | JSPROP_READONLY);
                        
                            /* set the buffPtr to the end of the value so that we can restart this loop */
                            if (tempPtr)
                            {
                                buffPtr= ++tempPtr;
                            }
                        }
                        else
                        {
                            /* we hit a return before hitting the =.  Lets adjust the buffPtr, and continue */
	                     	buffPtr = XP_STRCHR(buffPtr, '\n');
	                     	if (buffPtr != NULL)	
	                     	{
	                     		buffPtr++;
	                     	}

                        }

                    }
                    else
                    {
                        /* the resource file does look right - Line without an = */
                        /* what do we do? - lets just break*/
                        break; 
                    }
                }
            }

            XP_FREEIF(readBuf);
		}
		XP_FileClose(fp);
    }
    else
    {
        return NULL;
    }
    
	return stringObj;
}

PR_END_EXTERN_C
