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
#include "fe_proto.h"
#include "xp.h"
#include "xp_str.h"
#include "prprf.h"
#include "nsString.h"
#include "nsSoftwareUpdate.h"
#include "nsSoftUpdateEnums.h"
#include "nsInstallObject.h"
#include "nsInstallFile.h"
#include "nsInstallDelete.h"
#include "nsInstallExecute.h"
#include "nsVersionRegistry.h"
#include "nsSUError.h"
#include "nsWinReg.h"
#include "nsPrivilegeManager.h"
#include "nsTarget.h"
#include "nsPrincipal.h"

extern int SU_ERROR_BAD_PACKAGE_NAME;
extern int SU_ERROR_WIN_PROFILE_MUST_CALL_START;
extern int SU_ERROR_BAD_PACKAGE_NAME_AS;
extern int SU_ERROR_EXTRACT_FAILED;

PR_BEGIN_EXTERN_C

/* Public Methods */

/**
 * @param env   JavaScript environment (this inside the installer). Contains installer directives
 * @param inUserPackageName Name of tha package installed. This name is displayed to the user
 */
nsSoftwareUpdate::nsSoftwareUpdate(void* env, char* inUserPackageName)
{
  userPackageName = PR_sprintf_append(userPackageName, "%s", inUserPackageName);
  installPrincipal = NULL;
  packageName = NULL;
  packageFolder = NULL;
  installedFiles = NULL;
  confdlg = NULL;
  progwin = 0;
  patchList = new nsHashtable();
  zigPtr = NULL;
  userChoice = -1;
  lastError = 0;
  silent = PR_FALSE;
  force = PR_FALSE;

  /* Need to verify that this is a SoftUpdate JavaScript object */
  VerifyJSObject(env);

  /* XXX: FIX IT. How do we get data from env 
  jarName = (String) env.getMember("src");
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
  CleanUp();
}

nsPrincipal* nsSoftwareUpdate::GetPrincipal()
{
  return installPrincipal;
}

char* nsSoftwareUpdate::GetUserPackageName()
{
  return userPackageName;
}

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
      char * ignore = spec->GetDirectoryPath(errorMsg);
      if (*errorMsg != NULL)
        return NULL;
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
  char* allocatedStr=NULL;
  char* dir;
  nsFolderSpec* spec = NULL;

  dir = nsVersionRegistry::getDefaultDirectory( component );

  if ( dir == NULL ) {
    dir = nsVersionRegistry::componentPath( component );
    if ( dir != NULL ) {
      int i;

      nsString dirStr(dir);
      if ((i = dirStr.RFind(filesep)) > 0) {
        allocatedStr = new char[i];
        allocatedStr = dirStr.ToCString(allocatedStr, i);
        dir = allocatedStr;
      }
    }
  }

  if ( dir != NULL ) {
    /* We have a directory */
    spec = new nsFolderSpec("Installed", dir, userPackageName);
  }
  if (allocatedStr != NULL)
    delete allocatedStr;
  return spec;
}

nsFolderSpec* nsSoftwareUpdate::GetComponentFolder(char* component, char* subdir, char* *errorMsg)
{
  return GetFolder( GetComponentFolder( component ), subdir, errorMsg );
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
void* nsSoftwareUpdate::GetWinProfile(nsFolderSpec* folder, char* file, char* *errorMsg)
{
#ifdef XP_PC
  nsWinProfile* profile = NULL;
  *errorMsg = NULL;

  if ( packageName == NULL ) {
    *errorMsg = SU_GetErrorMsg4(SU_ERROR_WIN_PROFILE_MUST_CALL_START, nsSoftUpdateError_INSTALL_NOT_STARTED );
    return NULL;
  }
  profile = new nsWinProfile(this, folder, file, errorMsg);

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

  if ( packageName == NULL ) {
    *errorMsg = SU_GetErrorMsg4(SU_ERROR_WIN_PROFILE_MUST_CALL_START, nsSoftUpdateError_INSTALL_NOT_STARTED );
    return NULL;
  }
  registry = new nsWinReg(this, errorMsg);
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
 * @param inJarLocation  file name inside the JAR file
 */
char* nsSoftwareUpdate::ExtractJARFile(char* inJarLocation, char* finalFile, char* *errorMsg)
{
  if (zigPtr == NULL) {
    *errorMsg = SU_GetErrorMsg3("JAR file has not been opened", nsSoftUpdateError_UNKNOWN_JAR_FILE );
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
  {
    PRUint32 i;
    PRBool haveMatch = PR_FALSE;
    nsPrincipalArray* prinArray = (nsPrincipalArray*)getCertificates( zigPtr, inJarLocation );
    if ((prinArray == NULL) || (prinArray->GetSize() == 0)) {
      char *msg = PR_sprintf_append("Missing certificate for %s", inJarLocation);
      *errorMsg = SU_GetErrorMsg3(msg, nsSoftUpdateError_NO_CERTIFICATE);
      PR_FREEIF(msg);
      return NULL;
    }

    PRUint32 noOfPrins = prinArray->GetSize();
    for (i=0; i < noOfPrins; i++) {
      nsPrincipal* prin = (nsPrincipal*)prinArray->Get(i);
      if (installPrincipal->equals( prin )) {
        haveMatch = true;
        break;
      }
    }
    if (haveMatch == false) {
      char *msg = NULL;
      msg = PR_sprintf_append(msg, "Missing certificate for %s", inJarLocation);
      *errorMsg = SU_GetErrorMsg3(msg, nsSoftUpdateError_NO_MATCHING_CERTIFICATE);
      PR_FREEIF(msg);
    }

    /* Free all the objects */
    for (i=0; i < noOfPrins; i++) {
      nsPrincipal* prin = (nsPrincipal*)prinArray->Get(i);
      delete prin;
      prinArray->Set(i, NULL);
    }
    delete prinArray;

    if (*errorMsg != NULL) 
      return NULL;
  }

  /* Extract the file */
  char* outExtractLocation = NativeExtractJARFile(inJarLocation, finalFile, errorMsg);
  return outExtractLocation;
}

/**
 * Call this to initialize the update
 * Opens the jar file and gets the certificate of the installer
 * Opens up the gui, and asks for proper security privileges
 * @param vrPackageName     Full qualified  version registry name of the package
 *                          (ex: "/Plugins/Adobe/Acrobat")
 *                          NULL or empty package names are errors
 * @param inVInfo           version of the package installed.
 *                          Can be NULL, in which case package is installed
 *                          without a version. Having a NULL version, this package is
 *                          automatically updated in the future (ie. no version check is performed).
 * @param securityLevel     ignored (was LIMITED_INSTALL or FULL_INSTALL)
 */
PRInt32 nsSoftwareUpdate::StartInstall(char* vrPackageName, nsVersionInfo* inVInfo, PRInt32 securityLevel, char* *errorMsg)
{
  // ignore securityLevel
  return StartInstall( vrPackageName, inVInfo, errorMsg);
}

/**
 * An new form that doesn't require the security level
 */
PRInt32 nsSoftwareUpdate::StartInstall(char* vrPackageName, nsVersionInfo* inVInfo, char* *errorMsg)
{
  int errcode= nsSoftwareUpdate_SUCCESS;
  ResetError();
  

  if ( (vrPackageName	== NULL) ) {
    *errorMsg = SU_GetErrorMsg4(SU_ERROR_BAD_PACKAGE_NAME, nsSoftUpdateError_INVALID_ARGUMENTS );
    return nsSoftUpdateError_INVALID_ARGUMENTS;
  }
      
  packageName = PR_sprintf_append(packageName, "%s", vrPackageName);
  
  int len = XP_STRLEN(vrPackageName);
  int last_pos = len-1;
  char* packageName = new char[len+1];
  XP_STRCPY(packageName, vrPackageName);
  while ((last_pos >= 0) && (packageName[last_pos] == '/')) {
    // Make sure that package name does not end with '/'
    char* ptr = new char[last_pos+1];
    memcpy(packageName, ptr, last_pos);
    packageName[last_pos] = '\0';
    delete packageName;
    packageName	= ptr;
    last_pos = last_pos - 1;
  }
  versionInfo	= inVInfo;
  installedFiles = new nsVector();
      
  /* JAR initalization */
  /* open the file, create a principal out of installer file certificate */
  OpenJARFile(errorMsg);
  InitializeInstallerCertificate(errorMsg);
  CheckSilentPrivileges(errorMsg);
  RequestSecurityPrivileges(errorMsg);
      
  OpenProgressDialog(errorMsg);
  if (*errorMsg == NULL) 
    return nsSoftUpdateError_UNEXPECTED_ERROR;
      
  // set up default package folder, if any
  char* path = nsVersionRegistry::getDefaultDirectory( packageName );
  if ( path !=  NULL ) {
    packageFolder = new nsFolderSpec("Installed", path, userPackageName);
  }
  
  saveError( errcode );
  return errcode;
}

/**
 * another StartInstall() simplification -- version as char*
 */
PRInt32 nsSoftwareUpdate::StartInstall(char* vrPackageName, char* inVer, char* *errorMsg)
{
  return StartInstall( vrPackageName, new nsVersionInfo( inVer ), errorMsg );
}

/*
 * UI feedback
 */
void nsSoftwareUpdate::UserCancelled()
{
  userChoice = 0;
  AbortInstall();
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
  PRBool rebootNeeded = false;
  int result = nsSoftwareUpdate_SUCCESS;
  
  SetProgressDialogItem(""); // blank the "current item" line
      
  if (packageName == NULL) {
    //	probably didn't	call StartInstall()          
    *errorMsg = SU_GetErrorMsg4(SU_ERROR_WIN_PROFILE_MUST_CALL_START, nsSoftUpdateError_INSTALL_NOT_STARTED);
    return nsSoftUpdateError_UNEXPECTED_ERROR;
  }
      
  if ( installedFiles == NULL || installedFiles->GetSize() == 0 ) {
    // no actions queued: don't register the package version
    // and no need for user confirmation
    
    CleanUp();
    return result; // XXX: a different status code here?
  }
      
  // Wait for user approval
#ifdef XXX /* FIX IT */
  if ( !silent && UserWantsConfirm() ) {
    confdlg = new nsProgressDetails(this);

    /* XXX What is this? 
    while ( userChoice == -1 )
      Thread.sleep(10);
    */
          
    confdlg = NULL;
    if (userChoice != 1) {
      AbortInstall();
      return saveError(nsSoftUpdateError_USER_CANCELLED);
    }
  }
      
  SetProgressDialogRange( installedFiles->GetSize() );
#endif /* XXX */
      
      
  // Register default package folder if set
  if ( packageFolder != NULL ) {
    nsVersionRegistry::setDefaultDirectory( packageName, packageFolder->toString() );
  }
      
  /* call Complete() on all the elements */
  /* If an error occurs in the middle, call Abort() on the rest */
  nsVector* ve = GetInstallQueue();
  nsInstallObject* ie = NULL;
      
  // Main loop
  int count = 0;
  nsVersionRegistry::uninstallCreate(packageName, userPackageName);
  PRUint32 i=0;
  for (i=0; i < ve->GetSize(); i++) {
    ie = (nsInstallObject*)ve->Get(i);
    if (ie == NULL)
      continue;
    ie->Complete(errorMsg);
    if (errorMsg != NULL) {
      ie->Abort();
      return nsSoftUpdateError_UNEXPECTED_ERROR;
    }
    SetProgressDialogThermo(++count);
  }
  // add overall version for package
  if ( versionInfo != NULL) {
    result = nsVersionRegistry::installComponent(packageName, NULL, versionInfo);
  }
  CleanUp();
  
  if (result == nsSoftwareUpdate_SUCCESS && rebootNeeded)
    result = nsSoftwareUpdate_REBOOT_NEEDED;
  
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
 * Do not call installedFiles.addElement directly, because this routine also handles
 * progress messages
 */
char* nsSoftwareUpdate::ScheduleForInstall(nsInstallObject* ob)
{
  char *errorMsg = NULL;
  // flash current item
  SetProgressDialogItem( ob->toString() );

  // do any unpacking or other set-up
  errorMsg = ob->Prepare();
  if (errorMsg != NULL) {
    return errorMsg;
  }

  // Add to installation list if we haven't thrown out
  installedFiles->Add( ob );
  // if (confdlg != NULL)
  //    confdlg.ScheduleForInstall( ob );
  return NULL;
}

/**
 * Extract  a file from JAR archive to the disk, and update the
 * version registry. Actually, keep a record of elements to be installed. FinalizeInstall()
 * does the real installation. Install elements are accepted if they meet one of the
 * following criteria:
 *  1) There is no entry for this subcomponnet in the registry
 *  2) The subcomponent version info is newer than the one installed
 *  3) The subcomponent version info is NULL
 *
 * @param name      path of the package in the registry. Can be:
 *                    absolute: "/Plugins/Adobe/Acrobat/Drawer.exe"
 *                    relative: "Drawer.exe". Relative paths are relative to main package name
 *                    NULL:   if NULL jarLocation is assumed to be the relative path
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
  int result = nsSoftwareUpdate_SUCCESS;
  *errorMsg = NULL;
  if ( jarSource == NULL || (XP_STRLEN(jarSource) == 0) ) {
    *errorMsg = SU_GetErrorMsg3("No Jar Source", nsSoftUpdateError_INVALID_ARGUMENTS );
    return nsSoftUpdateError_INVALID_ARGUMENTS;
  }
      
  if ( folderSpec == NULL ) {
    *errorMsg = SU_GetErrorMsg3("folderSpec is NULL ", nsSoftUpdateError_INVALID_ARGUMENTS );
    return nsSoftUpdateError_INVALID_ARGUMENTS;
  }
      
  if (packageName == NULL) {
    // probably didn't call StartInstall()
    *errorMsg = SU_GetErrorMsg4(SU_ERROR_BAD_PACKAGE_NAME_AS, nsSoftUpdateError_BAD_PACKAGE_NAME );
    return nsSoftUpdateError_BAD_PACKAGE_NAME;
  }
      
  if ((name == NULL) || (XP_STRLEN(name) == 0)) {
    // Default subName = location in jar file
    name = GetQualifiedRegName( jarSource );
  } else {
    name = GetQualifiedRegName( name );
  }
      
  if ( (relativePath == NULL) || (XP_STRLEN(relativePath) == 0) ) {
    relativePath = jarSource;
  }
      
  /* Check for existence of the newer	version	*/
      
  PRBool versionNewer = PR_FALSE;
  if ( (forceInstall == PR_FALSE ) &&
       (version !=  NULL) &&
       ( nsVersionRegistry::validateComponent( name ) == 0 ) ) {
    nsVersionInfo* oldVersion = nsVersionRegistry::componentVersion(name);
    if ( version->compareTo( oldVersion ) != nsVersionEnum_EQUAL )
      versionNewer = PR_TRUE;
  } else {
    versionNewer = PR_TRUE;
  }
      
  if (versionNewer) {
    ie = new nsInstallFile( this, name, version, jarSource,
                            folderSpec, relativePath, forceInstall, 
                            errorMsg );
    if (errorMsg == NULL) {
      *errorMsg = ScheduleForInstall( ie );
    }
  }
  if (*errorMsg != NULL) {
    result = nsSoftUpdateError_UNEXPECTED_ERROR;
  }
  
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
  int errcode = nsSoftwareUpdate_SUCCESS;
  
  nsInstallExecute* ie = new nsInstallExecute(this, jarSource, errorMsg, args);
  if (*errorMsg != NULL) {
    errcode = nsSoftUpdateError_ACCESS_DENIED;
  } else {
    *errorMsg = ScheduleForInstall( ie );
    if (*errorMsg != NULL) {
      errcode = nsSoftUpdateError_UNEXPECTED_ERROR;
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
PRInt32 nsSoftwareUpdate::Gestalt(char* selectorStr, int* os_err, char* *errorMsg)
{
  *errorMsg = NULL;

#ifdef XP_MAC
  *os_err = noErr;
  long response = 0;
  OSType selector;
  
  if ((selectorStr == NULL) ||
      (XP_STRLEN(selectorStr) != 4))
    {
      *os_err = nsSoftUpdateError_GESTALT_UNKNOWN_ERR; /* gestaltUnknownErr */
      goto fail;
    }
  XP_MEMCPY(&selector, selectorStr, 4);
  *os_err = (int)Gestalt(selector, response);
  if (*os_err == noErr) {
    return response;
  }
  goto fail;	/* Drop through to failure */
#else
  *os_err = nsSoftUpdateError_GESTALT_INVALID_ARGUMENT;
  goto fail;
#endif

fail:
  *errorMsg = SU_GetErrorMsg4(SU_ERROR_EXTRACT_FAILED, *os_err );
  return 0;
}

/**
 * Patch
 *
 */
PRInt32 nsSoftwareUpdate::Patch(char* regName, nsVersionInfo* version, char* patchname, char* *errorMsg)
{
  int errcode = nsSoftwareUpdate_SUCCESS;
  
  if ( regName == NULL || patchname == NULL ) {
    *errorMsg = SU_GetErrorMsg3("regName or patchName is NULL ", nsSoftUpdateError_INVALID_ARGUMENTS );
    return saveError( nsSoftUpdateError_INVALID_ARGUMENTS );
  }
  
  char* rname = GetQualifiedRegName( regName );
  
#ifdef XXX /* FIX IT    */
  nsInstallPatch* ip = new nsInstallPatch(this, rname, version, patchname, errorMsg);
  if (*errorMsg != NULL) {
    errcode = nsSoftUpdateError_ACCESS_DENIED;
  } else {
    *errorMsg = ScheduleForInstall( ip );
    if (*errorMsg != NULL) {
      errcode = nsSoftUpdateError_UNEXPECTED_ERROR;
    }
  }
#endif /* XXX */
  saveError( errcode );
  return errcode;
}

PRInt32 nsSoftwareUpdate::Patch(char* regName, nsVersionInfo* version, char* patchname,
              nsFolderSpec* folder, char* filename, char* *errorMsg)
{
  if ( folder == NULL || regName == NULL || XP_STRLEN(regName) == 0 || patchname == NULL ) {
    *errorMsg = SU_GetErrorMsg3("folder or regName or patchName is NULL ", nsSoftUpdateError_INVALID_ARGUMENTS );
    return saveError( nsSoftUpdateError_INVALID_ARGUMENTS );
  }
  
  int errcode = nsSoftwareUpdate_SUCCESS;
  
  char* rname = GetQualifiedRegName( regName );

#ifdef XXX /* FIX IT    */
  nsInstallPatch* ip = new nsInstallPatch( this, rname, version,
                                           patchname, folder, filename, errorMsg );
  if (*errorMsg != NULL) {
    errcode = nsSoftUpdateError_ACCESS_DENIED;
  } else {
    *errorMsg = ScheduleForInstall( ip );
    if (*errorMsg != NULL) {
      errcode = nsSoftUpdateError_UNEXPECTED_ERROR;
    }
  }
#endif /* XXX */
  saveError( errcode );
  return errcode;
}

/**
 * This method deletes the specified file from the disk. It does not look
 * for the file in the registry and is guaranteed to muddle any uninstall 
 * reference counting.  Its main purpose is to delete files installed or 
 * created outside of SmartUpdate.
 */
PRInt32 nsSoftwareUpdate::DeleteFile(nsFolderSpec* folder, char* relativeFileName, char* *errorMsg)
{
  int errcode = nsSoftwareUpdate_SUCCESS;
  
  nsInstallDelete* id = new nsInstallDelete(this, folder, relativeFileName, errorMsg);
  if (*errorMsg != NULL) {
    errcode = nsSoftUpdateError_ACCESS_DENIED;
  } else {
    *errorMsg = ScheduleForInstall( id );
    if (*errorMsg != NULL) {
      errcode = nsSoftUpdateError_UNEXPECTED_ERROR;
    }
  }
  /* XXX: who set FILE_DOES_NOT_EXIST errcode ?? */
  if (errcode == nsSoftUpdateError_FILE_DOES_NOT_EXIST) {
    errcode = nsSoftwareUpdate_SUCCESS;
  }
  saveError( errcode );
  return errcode;
}

/**
 * This method finds named registry component and deletes both the file and the 
 * entry in the Client VR. registryName is the name of the component in the registry.
 * Returns usual errors codes + code to indicate item doesn't exist in registry, registry 
 * item wasn't a file item, or the related file doesn't exist. If the file is in use we will
 * store the name and to try to delete it on subsequent start-ups until we're successful.
 */
PRInt32 nsSoftwareUpdate::DeleteComponent(char* registryName, char* *errorMsg)
{
  int errcode = nsSoftwareUpdate_SUCCESS;
  
  nsInstallDelete* id = new nsInstallDelete(this, NULL, registryName, errorMsg);
  if (*errorMsg != NULL) {
    errcode = nsSoftUpdateError_ACCESS_DENIED;
  } else {
    *errorMsg = ScheduleForInstall( id );
    if (*errorMsg != NULL) {
      errcode = nsSoftUpdateError_UNEXPECTED_ERROR;
    }
  }
  /* XXX: who set FILE_DOES_NOT_EXIST errcode ?? */
  if (errcode == nsSoftUpdateError_FILE_DOES_NOT_EXIST) {
    errcode = nsSoftwareUpdate_SUCCESS;
  }
  saveError( errcode );
  return errcode;
}

nsFolderSpec* nsSoftwareUpdate::GetFolder(char* targetFolder, char* subdirectory, char* *errorMsg)
{
  if (XP_STRCMP(targetFolder, FOLDER_FILE_URL) == 0) {
    char* newPath = NULL;
    char* path = NativeFileURLToNative("/", subdirectory );
    if ( path != NULL ) {
      PRInt32 filesep_len = XP_STRLEN(filesep);
      PRInt32 path_len = XP_STRLEN(path);
      PRBool ends_with_filesep = PR_FALSE;
      if (path_len >= filesep_len) {
        ends_with_filesep = (XP_STRSTR(&path[path_len - filesep_len], filesep) ? PR_TRUE : PR_FALSE);
      }
      if (ends_with_filesep) {
        return new nsFolderSpec("Installed", path, userPackageName);
      } else {
        newPath = PR_sprintf_append(newPath, "%s%s", path, filesep);
        return new nsFolderSpec("Installed", newPath, userPackageName);
      }
    } else {
      return NULL;
    }
  } else {
    return GetFolder( GetFolder(targetFolder, NULL, errorMsg), subdirectory, errorMsg );
  }
}

nsFolderSpec* nsSoftwareUpdate::GetFolder(nsFolderSpec* folder, char* subdir, char* *errorMsg)
{
  /* XXX: FIX IT */
  return NULL;
}

/**
 * This method returns true if there is enough free diskspace, false if there isn't.
 * The drive containg the folder is checked for # of free bytes.
 */
long nsSoftwareUpdate::DiskSpaceAvailable(nsFolderSpec* folder)
{
  /* XXX: Fix it */
  return 0;
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
                     PRBool forceInstall)
{
  /* XXX: Fix it */
  return 0;
}

/* Uninstall */
PRInt32 nsSoftwareUpdate::Uninstall(char* packageName)
{
  /* XXX: Fix it */
  return 0;
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
void nsSoftwareUpdate::OpenProgressDialog()
{
}

void nsSoftwareUpdate::CloseProgressDialog()
{
}

void nsSoftwareUpdate::SetProgressDialogItem(char* message)
{
}

void nsSoftwareUpdate::SetProgressDialogRange(PRInt32 max)
{
}

void nsSoftwareUpdate::SetProgressDialogThermo(PRInt32 value)
{
}

/* Private Methods */

/*
 * Reads in the installer certificate, and creates its principal
 */
char* nsSoftwareUpdate::InitializeInstallerCertificate()
{
  /* XXX: Fix it */
  return NULL;
}

/*
 * checks if our principal has privileges for silent install
 */
PRBool nsSoftwareUpdate::CheckSilentPrivileges()
{
  /* XXX: Fix it */
  return PR_FALSE;
}

/* Request the security privileges, so that the security dialogs
 * pop up
 */
void nsSoftwareUpdate::RequestSecurityPrivileges()
{
}

/**
 * saves non-zero error codes so they can be seen by GetLastError()
 */
PRInt32 nsSoftwareUpdate::saveError(PRInt32 errcode)
{
  /* XXX: Fix it */
  return 0;
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
  if ( installedFiles != NULL ) {
    PRUint32 i=0;
    for (; i < installedFiles->GetSize(); i++) {
      ie = (nsInstallObject*)installedFiles->Get(i);
      delete ie;
    }
    installedFiles->RemoveAll();
  }
}

/**
 * GetQualifiedRegName
 *
 * This routine converts a package-relative component registry name
 * into a full name that can be used in calls to the version registry.
 */
char* nsSoftwareUpdate::GetQualifiedRegName(char* name)
{
  /* XXX: Fix it */
  return NULL;
}

/* Private Native methods */

/* VerifyJSObject
 * Make sure that JSObject is of type SoftUpdate.
 * Since SoftUpdate JSObject can only be created by C code
 * we cannot be spoofed
 */
void nsSoftwareUpdate::VerifyJSObject(void* jsObj)
{
}

/* Open/close the jar file
 */
char* nsSoftwareUpdate::OpenJARFile()
{
  /* XXX: Fix it */
  return NULL;
}

void nsSoftwareUpdate::CloseJARFile()
{
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

char* nsSoftwareUpdate::NativeExtractJARFile(char* inJarLocation, char* finalFile, char* *errorMsg)
{
  /* XXX: Fix it */
  return NULL;
}

PRInt32 nsSoftwareUpdate::NativeMakeDirectory(char* path)
{
  /* XXX: Fix it */
  return 0;
}

long nsSoftwareUpdate::NativeDiskSpaceAvailable(char* path)
{
  /* XXX: Fix it */
  return 0;
}

char* nsSoftwareUpdate::NativeFileURLToNative(char* dir, char* path)
{
  /* XXX: Fix it */
  return NULL;
}

char** nsSoftwareUpdate::ExtractDirEntries(char* Dir)
{
  /* XXX: Fix it */
  return NULL;
}

PRInt32  nsSoftwareUpdate::NativeOpenProgDlg(char* packageName)
{
  /* XXX: Fix it */
  return 0;
}

void nsSoftwareUpdate::NativeCloseProgDlg(PRInt32 progptr)
{
}

void nsSoftwareUpdate::NativeSetProgDlgItem(PRInt32 progptr, char* message)
{
}

void nsSoftwareUpdate::NativeSetProgDlgRange(PRInt32 progptr, PRInt32 max)
{
}

void nsSoftwareUpdate::NativeSetProgDlgThermo(PRInt32 progptr, PRInt32 value)
{
}

PRBool nsSoftwareUpdate::UserWantsConfirm()
{
  /* XXX: Fix it */
  return PR_FALSE;
}


PR_END_EXTERN_C

