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

#ifndef nsSoftwareUpdate_h__
#define nsSoftwareUpdate_h__

#include "prtypes.h"
#include "jsapi.h"
#include "nsHashtable.h"
#include "nsVector.h"

#include "nsSoftUpdateEnums.h"
#include "nsFolderSpec.h"
#include "nsVersionInfo.h"
#include "nsInstallObject.h"

#include "nsPrincipal.h"

PR_BEGIN_EXTERN_C

#define IMPERSONATOR "Impersonator"
#define INSTALL_PRIV "SoftwareInstall"
#define SILENT_PRIV "SilentInstall"
#define FOLDER_FILE_URL "file:///"

struct nsProgressDetails;

struct nsSoftwareUpdate {

public:

  /* Public Fields */
  char* packageName;               /* Name of the package we are installing */
  char* userPackageName;           /* User-readable package name */
  nsVector* installedFiles;        /* List of files/processes to be installed */
  nsHashtable* patchList;          /* files that have been patched (orig name is key) */
  nsVersionInfo* versionInfo;      /* Component version info */


  /* Public Methods */

  /**
   * @param env   JavaScript environment (this inside the installer). Contains installer directives
   * @param inUserPackageName Name of tha package installed. This name is displayed to the user
   */
  nsSoftwareUpdate(void* env, char* inUserPackageName);

  virtual ~nsSoftwareUpdate();

  nsPrincipal* GetPrincipal();

  char* GetUserPackageName();

  char* GetRegPackageName();

  PRBool GetSilent();

  /**
   * @return a vector of InstallObjects
   */
  nsVector* GetInstallQueue();

  /**
   * @return  the most recent non-zero error code
   * @see ResetError
   */
  PRInt32 GetLastError();

  /**
   * resets remembered error code to zero
   * @see GetLastError
   */
  void ResetError();

  /**
   * @return the folder object suitable to be passed into AddSubcomponent
   * @param folderID One of the predefined folder names
   * @see AddSubcomponent
   */
  nsFolderSpec* GetFolder(char* folderID, char* *errorMsg);

  /**
   * @return the full path to a component as it is known to the
   *          Version Registry
   * @param  component Version Registry component name
   */
  nsFolderSpec* GetComponentFolder(char* component);

  nsFolderSpec* GetComponentFolder(char* component, char* subdir, char* *errorMsg);

  /**
   * sets the default folder for the package
   * @param folder a folder object obtained through GetFolder()
   */
  void SetPackageFolder(nsFolderSpec* folder);

  /**
   * Returns a Windows Profile object if you're on windows,
   * null if you're not or if there's a security error
   */
  void* GetWinProfile(nsFolderSpec* folder, char* file, char* *errorMsg);

  /**
   * @return an object for manipulating the Windows Registry.
   *          Null if you're not on windows
   */
  void* GetWinRegistry(char* *errorMsg);


  /**
   * extract the file out of the JAR directory and places it into temporary
   * directory.
   * two security checks:
   * - the certificate of the extracted file must match the installer certificate
   * - must have privileges to extract the jar file
   * @param inJarLocation  file name inside the JAR file
   */
  char* ExtractJARFile(char* inJarLocation, char* finalFile, char* *errorMsg);


  /**
   * Call this to initialize the update
   * Opens the jar file and gets the certificate of the installer
   * Opens up the gui, and asks for proper security privileges
   * @param vrPackageName     Full qualified  version registry name of the package
   *                          (ex: "/Plugins/Adobe/Acrobat")
   *                          null or empty package names are errors
   * @param inVInfo           version of the package installed.
   *                          Can be null, in which case package is installed
   *                          without a version. Having a null version, this package is
   *                          automatically updated in the future (ie. no version check is performed).
   * @param flags             Once was securityLevel(LIMITED_INSTALL or FULL_INSTALL).  Now
   *                          can be either NO_STATUS_DLG or NO_FINALIZE_DLG
   */
  PRInt32 StartInstall(char* vrPackageName, nsVersionInfo* inVInfo, PRInt32 flags, char* *errorMsg);


  /**
   * An new form that doesn't require the security level
   */
  PRInt32 StartInstall(char* vrPackageName, char* inVer, PRInt32 flags, char* *errorMsg);

  /**
   * another StartInstall() simplification -- version as char*
   */
  PRInt32 StartInstall(char* vrPackageName, char* inVer, char* *errorMsg);


  /*
   * UI feedback
   */
  void UserCancelled();

  void UserApproved();

  /**
   * Proper completion of the install
   * Copies all the files to the right place
   * returns 0 on success, <0 error code on failure
   */
  PRInt32 FinalizeInstall(char* *errorMsg);

  /**
   * Aborts the install :), cleans up all the installed files
   * XXX: This is a synchronized method. FIX it.
   */
  void AbortInstall();


  /**
   * ScheduleForInstall
   * call this to put an InstallObject on the install queue
   * Do not call installedFiles.addElement directly, because this routine also handles
   * progress messages
   */
  char* ScheduleForInstall(nsInstallObject* ob);


  /**
   * Extract  a file from JAR archive to the disk, and update the
   * version registry. Actually, keep a record of elements to be installed. FinalizeInstall()
   * does the real installation. Install elements are accepted if they meet one of the
   * following criteria:
   *  1) There is no entry for this subcomponnet in the registry
   *  2) The subcomponent version info is newer than the one installed
   *  3) The subcomponent version info is null
   *
   * @param name      path of the package in the registry. Can be:
   *                    absolute: "/Plugins/Adobe/Acrobat/Drawer.exe"
   *                    relative: "Drawer.exe". Relative paths are relative to main package name
   *                    null:   if null jarLocation is assumed to be the relative path
   * @param version   version of the subcomponent. Can be null
   * @param jarSource location of the file to be installed inside JAR
   * @param folderSpec one of the predefined folder   locations
   * @see GetFolder
   * @param relativePath  where the file should be copied to on the local disk.
   *                      Relative to folder
   *                      if null, use the path to the JAR source.
   * @param forceInstall  if true, file is always replaced
   */
  PRInt32 AddSubcomponent(char* name, 
                          nsVersionInfo* version, 
                          char* jarSource,
                          nsFolderSpec* folderSpec, 
                          char* relativePath,
                          PRBool forceInstall, 
                          char* *errorMsg);
  
  /**
   * executes the file
   * @param jarSource     name of the file to execute inside JAR archive
   * @param args          command-line argument string (Win/Unix only)
   */
  PRInt32 Execute(char* jarSource, char* *errorMsg, char* args);


  /**
   * Mac-only, simulates Mac toolbox Gestalt function
   * OSErr Gestalt(char* selector, long * response)
   * @param selector      4-character string, 
   * @return      2 item array. 1st item corresponds to OSErr, 
   *                       2nd item corresponds to response
   */
  PRInt32 Gestalt(char* selectorStr, int* os_err, char* *errorMsg);



  /**
   * Patch
   *
   */
  PRInt32 Patch(char* regName, nsVersionInfo* version, char* patchname, char* *errorMsg);

  PRInt32 Patch(char* regName, nsVersionInfo* version, char* patchname,
                nsFolderSpec* folder, char* filename, char* *errorMsg);


  /**
   * This method deletes the specified file from the disk. It does not look
   * for the file in the registry and is guaranteed to muddle any uninstall 
   * reference counting.  Its main purpose is to delete files installed or 
   * created outside of SmartUpdate.
   */
  PRInt32 DeleteFile(nsFolderSpec* folder, char* relativeFileName, char* *errorMsg);


  /**
   * This method finds named registry component and deletes both the file and the 
   * entry in the Client VR. registryName is the name of the component in the registry.
   * Returns usual errors codes + code to indicate item doesn't exist in registry, registry 
   * item wasn't a file item, or the related file doesn't exist. If the file is in use we will
   * store the name and to try to delete it on subsequent start-ups until we're successful.
   */
  PRInt32 DeleteComponent(char* registryName, char* *errorMsg);

  nsFolderSpec* GetFolder(char* targetFolder, char* subdirectory, char* *errorMsg);

  nsFolderSpec* GetFolder(nsFolderSpec* folder, char* subdir, char* *errorMsg);

  /**
   * This method returns true if there is enough free diskspace, false if there isn't.
   * The drive containg the folder is checked for # of free bytes.
   */
  long DiskSpaceAvailable(nsFolderSpec* folder);


    /**
     * This method is used to install an entire subdirectory of the JAR. 
     * Any files so installed are individually entered into the registry so they
     * can be uninstalled later. Like AddSubcomponent the directory is installed
     * only if the version specified is greater than that already registered,
     * or if the force parameter is true.The final version presented is the complete
     * form, the others are for convenience and assume values for the missing
     * arguments.
     */
  PRInt32 AddDirectory(char* name, 
                       nsVersionInfo* version, 
                       char* jarSource,   
                       nsFolderSpec* folderSpec, 
                       char* subdir, 
                       PRBool forceInstall,
                       char* *errorMsg);


  
  JSObject*  LoadStringObject(const char* filename);


  /* Uninstall */
  PRInt32 Uninstall(char* packageName, char* *errorMsg);
  
  
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
  void OpenProgressDialog(void);
    
  void CloseProgressDialog(void);

  void SetProgressDialogItem(char* message);

  void SetProgressDialogRange(PRInt32 max);

  void SetProgressDialogThermo(PRInt32 value);

    
private:

  /* Private Fields */
  nsFolderSpec* packageFolder;     /* default folder for package */

  nsProgressDetails* confdlg;      /* Detail confirmation dialog */

  PRInt32 userChoice;              /* User feedback: -1 unknown, 0 is cancel, 1 is OK */
  void* progwin;                   /* pointer to native progress window */
  PRBool silent;                   /* Silent upgrade? */
  PRBool force;                    /* Force install? */
  PRInt32 lastError;               /* the most recent non-zero error */
  char* filesep;                   /* the platform-specific file separator */
  PRBool bShowProgress;            /* true if we should show the inital progress dialog    */
  PRBool bShowFinalize;            /* true if we should show the finalize progress dialog. */

  char* installerJarName;          /* Name of the installer file */
  unsigned long installerJarNameLength;    /* Length of Name of the installer file */
  char* jarName;                   /* Name of the JAR file */
  char* jarURL;                    /* Name of the JAR file */
  char* jarCharset;                /* Charset for filenames in JAR */
  void* zigPtr;                    /* Stores the pointer to ZIG * */
  nsPrincipal* installPrincipal;   /* principal with the signature from the JAR file */

  PRBool bUninstallPackage;        /* Create an uninstall node in registry? */
  PRBool bRegisterPackage;         /* Create package node in registry? */
  PRBool bUserCancelled;           /* User cancels the install prg dialog -true else false */
    

  /* Private Field Accessors */

  /* Private Methods */
  
  int SanityCheck(char**errorMsg);
  PRBool BadRegName(char* regName);
  
  /*
   * Parses the StartInstall flags and set class varibles.
   */   
  void ParseFlags(int flags);
   
  /*
   * Reads in the installer certificate, and creates its principal
   */
  PRInt32 InitializeInstallerCertificate(char* *errorMsg);

  /*
   * checks if our principal has privileges for silent install
   */
  PRBool CheckSilentPrivileges();


  /* Request the security privileges, so that the security dialogs
   * pop up
   */
  PRInt32 RequestSecurityPrivileges(char* *errorMsg);


  /**
   * saves non-zero error codes so they can be seen by GetLastError()
   */
  PRInt32 saveError(PRInt32 errcode);


  /*
   * CleanUp
   * call	it when	done with the install
   *
   * XXX: This is a synchronized method. FIX it.
   */
  void CleanUp();


  /**
   * GetQualifiedRegName
   *
   * This routine converts a package-relative component registry name
   * into a full name that can be used in calls to the version registry.
   */
  char* GetQualifiedRegName(char* name, char** errMsg);
  
  /**
   * GetQualifiedPackageName
   *
   * This routine converts a package-relative component registry name
   * into a full name that can be used in calls to the version registry.
   */
   
  char* GetQualifiedPackageName( char* name );
  char* CurrentUserNode();
  char* NativeProfileName();

  /* Private Native methods */

  /* VerifyJSObject
   * Make sure that JSObject is of type SoftUpdate.
   * Since SoftUpdate JSObject can only be created by C code
   * we cannot be spoofed
   */
  char* VerifyJSObject(void* jsObj);

  /* Open/close the jar file
   */
  PRInt32 OpenJARFile(char* *errorMsg);
  void CloseJARFile();

  /* getCertificates
   * native encapsulation that calls AppletClassLoader.getCertificates
   * we cannot call this method from Java because it is private.
   * The method cannot be made public because it is a security risk
   */
  void* getCertificates(void* zigPtr, char* inJarLocation);

  void freeIfCertificates(void* prins);

  char* NativeExtractJARFile(char* inJarLocation, char* finalFile, char* *errorMsg);

  PRInt32 NativeMakeDirectory(char* path);;

  long NativeDiskSpaceAvailable(char* path);

  char* NativeFileURLToNative(char* Dir, char* path);

  char** ExtractDirEntries(char* Dir, int *length);

  void*    NativeOpenProgDlg(char* packageName);
  void     NativeCloseProgDlg(void* progptr);
  void     NativeSetProgDlgItem(void* progptr, char* message);
  void     NativeSetProgDlgRange(void* progptr, PRInt32 max);
  void     NativeSetProgDlgThermo(void* progptr, PRInt32 value);
  PRBool   UserWantsConfirm();
  
  
  
  
  
  
  
  

};

PR_END_EXTERN_C

#endif /* nsSoftwareUpdate_h__ */
