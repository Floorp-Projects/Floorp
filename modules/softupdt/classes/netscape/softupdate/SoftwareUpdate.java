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
 * SoftwareUpdate.java    1.0    96/12/19
 *
 */
package netscape.softupdate;

import java.net.*;
import java.util.Vector;
import java.util.Hashtable;
import java.lang.Thread;

import netscape.util.*;
import netscape.applet.AppletClassLoader;
import netscape.security.Principal;
import netscape.security.Target;
import netscape.security.UserTarget;
import netscape.security.PrivilegeManager;
import netscape.security.AppletSecurity;
import netscape.security.UserDialogHelper;
import netscape.security.ForbiddenTargetException;
import netscape.javascript.JSObject;

final public class SoftwareUpdate {

/* Security strategy
 * Only two main targets, limited and full install
 * Limited install is Java folders only, full install everything else
 * Each routine that does something unsafe checks the permissions there
 */
    static String targetNames[] = {
    "LimitedInstall",       // Install Java classes into a single directory
    "SoftwareInstall",      // Any other install actions
    "SilentInstall"         // Install without any UI
    };
    public static final int LIMITED_INSTALL= 0;
    public static final int FULL_INSTALL = 1;
    public static final int SILENT_INSTALL = 2;

    protected static final String IMPERSONATOR = "Impersonator";

    /* Errors -200 to -300 */
    static final int BAD_PACKAGE_NAME = -200;
    static final int UNEXPECTED_ERROR = -201;
    static final int ACCESS_DENIED = -202;
    static final int TOO_MANY_CERTIFICATES = -203;  // Installer file must have 1 certificate
    static final int NO_INSTALLER_CERTIFICATE = -204; // Installer file must have a certificate
    static final int NO_CERTIFICATE =  -205; // Extracted file is not signed
    static final int NO_MATCHING_CERTIFICATE = -206; // Extracted file does not match installer certificate
    static final int UNKNOWN_JAR_FILE = -207;   // JAR file has not been opened
    static final int INVALID_ARGUMENTS = -208;  // Bad arguments to a function
    static final int ILLEGAL_RELATIVE_PATH = -209;  // Illegal relative path
    static final int USER_CANCELLED = -210;     // User cancelled
    static final int INSTALL_NOT_STARTED = -211;
    static final int SILENT_MODE_DENIED = -212;
    static final int NO_SUCH_COMPONENT = -213;  // no such component in the registry.

    static final int SUCCESS = 0;
    static final int REBOOT_NEEDED = 999;

    static {
        /* Create security targets */
        netscape.security.UserTarget t1, t2, t3;
        netscape.security.Principal sysPrin = PrivilegeManager.getSystemPrincipal();
        // t1 = new netscape.security.Target(targetNames[EXTRACT_JAR_FILE], sysPrin).registerTarget();
        t1 = new netscape.security.UserTarget(targetNames[LIMITED_INSTALL], sysPrin,
                     UserDialogHelper.targetRiskMedium(),
                     UserDialogHelper.targetRiskColorMedium(),
                     Strings.targetDesc_LimitedInstall(),
                     Strings.getString("s37"),
                     Strings.targetUrl_LimitedInstall()
                     );
        t1 = (UserTarget)t1.registerTarget();

        Target[] tarray;
        tarray = new Target[1];
        tarray[0] = t1;
        t2 = new netscape.security.UserTarget(targetNames[FULL_INSTALL], sysPrin,
                     UserDialogHelper.targetRiskHigh(),
                     UserDialogHelper.targetRiskColorHigh(),
                     Strings.targetDesc_FullInstall(),
                     Strings.getString("s38"),
                     Strings.targetUrl_FullInstall(),
                     tarray);
        t2 = (UserTarget)t2.registerTarget();
        tarray[0] = t2;
        t3 = new netscape.security.UserTarget(targetNames[SILENT_INSTALL], sysPrin,
                     UserDialogHelper.targetRiskHigh(),
                     UserDialogHelper.targetRiskColorHigh(),
                     Strings.targetDesc_SilentInstall(),
                     Strings.getString("s39"),
                     Strings.targetUrl_SilentInstall(),
                     tarray);
        t3 = (UserTarget)t3.registerTarget();
    };

    protected String packageName;       // Name of the package we are installing
    protected String userPackageName;   // User-readable package name
    protected Vector installedFiles;    // List of files/processes to be installed
    protected Hashtable patchList;      // files that have been patched (orig name is key)
    protected VersionInfo versionInfo;  // Component version info

    private FolderSpec packageFolder;   // default folder for package
    private ProgressMediator progress;  // Progress feedback
    private int userChoice;             // User feedback: -1 unknown, 0 is cancel, 1 is OK
    private boolean silent;             // Silent upgrade?
    private boolean force;              // Force install?
    private int lastError;              // the most recent non-zero error

    /**
     * @param env   JavaScript environment (this inside the installer). Contains installer directives
     * @param inUserPackageName Name of tha package installed. This name is displayed to the user
     */
    public
    SoftwareUpdate( JSObject env, String inUserPackageName)
    {
        userPackageName = inUserPackageName;
        installPrincipal = null;
        packageName = null;
        packageFolder = null;
        progress = null;
        patchList = new java.util.Hashtable();
        zigPtr = 0;
        userChoice = -1;
        lastError = 0;
        /* Need to verify that this is a SoftUpdate JavaScript object */
        VerifyJSObject(env);
        jarName = (String) env.getMember("src");

        try 
        {
            silent = ((Boolean) env.getMember("silent")).booleanValue();
            force  = ((Boolean) env.getMember("force")).booleanValue();
        }
        catch (Throwable t) 
        {
            System.out.println("Unexpected throw getting silent/force" );
            silent = false;
            force = false;
        }
    }

    protected void
    finalize() throws Throwable
    {
        CleanUp();
        super.finalize();
    }

    /* VerifyJSObject
     * Make sure that JSObject is of type SoftUpdate.
     * Since SoftUpdate JSObject can only be created by C code
     * we cannot be spoofed
     */
    private native void
    VerifyJSObject(JSObject obj);

    /*
     * Reads in the installer certificate, and creates its principal
     */
    private void
    InitializeInstallerCertificate() throws SoftUpdateException
    {
        Object[] certArray = null;

        certArray = getCertificates( zigPtr, installerJarName );

        if ( certArray == null || certArray.length == 0 )
            throw new SoftUpdateException( Strings.error_NoCertificate(), NO_INSTALLER_CERTIFICATE) ;

        if ( certArray.length > 1 )
            throw new SoftUpdateException( Strings.error_TooManyCertificates(), TOO_MANY_CERTIFICATES);

        installPrincipal = new netscape.security.Principal( Principal.CERT_KEY,
                                (byte[])certArray[0] );
    }

    /*
     * checks if our principal has privileges for silent install
     */
    private void
    CheckSilentPrivileges()
    {
        if (silent == false)
            return;
        try
        {
            /* Request impersonation privileges */
            netscape.security.PrivilegeManager privMgr;
            Target impersonation, target;

            privMgr = AppletSecurity.getPrivilegeManager();
            impersonation = Target.findTarget( IMPERSONATOR );
            privMgr.enablePrivilege( impersonation );

            /* check the security permissions */
            target = Target.findTarget( targetNames[SILENT_INSTALL] );

            /* XXX: we need a way to indicate that a dialog box should appear.*/
            privMgr.enablePrivilege( target, GetPrincipal() );
        }
        catch(Throwable t)
        {
            System.out.println(Strings.error_SilentModeDenied());
            silent = false;
        }
    }

    /* Request the security privileges, so that the security dialogs
     * pop up
     */
    private void
    RequestSecurityPrivileges(int securityLevel)
    {
        if ((securityLevel != LIMITED_INSTALL) &&
            (securityLevel != FULL_INSTALL))
            securityLevel = FULL_INSTALL;

        netscape.security.PrivilegeManager privMgr;
        Target impersonation, target;

        privMgr = AppletSecurity.getPrivilegeManager();
        impersonation = Target.findTarget( IMPERSONATOR );
        privMgr.enablePrivilege( impersonation );

        /* check the security permissions */
        target = Target.findTarget( SoftwareUpdate.targetNames[securityLevel] );
//        System.out.println("Asking for privileges ");
        privMgr.enablePrivilege( target, GetPrincipal() );
//       System.out.println("Got privileges ");
    }

    protected final netscape.security.Principal 
    GetPrincipal()  {
        return installPrincipal;
    }

    protected final String
    GetUserPackageName()    {
        return userPackageName;
    }
    
    protected final boolean
    GetSilent() {
        return silent;
    }
    
    /**
     * @return enumeration of InstallObjects
     */
    protected final java.util.Enumeration
    GetInstallQueue()   {
        if (installedFiles != null)
            return installedFiles.elements();
        else
            return null;
    }

    
    /**
     * @return  the most recent non-zero error code
     * @see ResetError
     */
    public int GetLastError()
    {
        return lastError;
    }

    /**
     * resets remembered error code to zero
     * @see GetLastError
     */
    public void ResetError()
    {
        lastError = 0;
    }

    /**
     * saves non-zero error codes so they can be seen by GetLastError()
     */
    private int saveError(int errcode)
    {
        if ( errcode != SUCCESS ) 
            lastError = errcode;
        return errcode;
    }





    /**
     * @return the folder object suitable to be passed into AddSubcomponent
     * @param folderID One of the predefined folder names
     * @see AddSubcomponent
     */
    public FolderSpec
    GetFolder(String folderID)
    {
        FolderSpec spec = null;
        try
        {
            int errCode = 0;

            if (folderID.compareTo("Installed") != 0)
            {
                spec = new FolderSpec(folderID, packageName, userPackageName);
                if (folderID == "User Pick")     // Force the prompt
                {
                    String ignore = spec.GetDirectoryPath();
                }
            }
        }
        catch (Throwable e)
        {
            return null;
        }
        return spec;
    }


	/**
	 * @return the full path to a component as it is known to the
	 *          Version Registry
	 * @param  component Version Registry component name
	 */
	public FolderSpec GetComponentFolder( String component )
	{
        String     dir;
        FolderSpec spec = null;

        dir = VersionRegistry.getDefaultDirectory( component );

        if ( dir == null )
        {
            dir = VersionRegistry.componentPath( component );
            if ( dir != null )
            {
                int i;

                if ((i = dir.lastIndexOf(java.lang.System.getProperty("file.separator"))) > 0)
                {
                        char c[] = new char[i];
                        dir.getChars(0, i, c, 0);
                        dir = new String( c );
                }
            }
        }

        if ( dir != null ) /* We have a directory */
        {
            spec = new FolderSpec("Installed", dir, userPackageName);
        }
        return spec;
	}

	public FolderSpec GetComponentFolder( String component, String subdir )
	{
        return GetFolder( GetComponentFolder( component ), subdir );
    }



    /**
     * sets the default folder for the package
     * @param folder a folder object obtained through GetFolder()
     */
    public void SetPackageFolder( FolderSpec folder )
    {
        packageFolder = folder;
    }


    /**
     * Returns a Windows Profile object if you're on windows,
     * null if you're not or if there's a security error
     */
    public Object
    GetWinProfile(FolderSpec folder, String file)
    {
        WinProfile  profile = null;
        String      osName = System.getProperty("os.name");

        try {
            if ( packageName == null ) {
                throw new SoftUpdateException(
                    Strings.error_WinProfileMustCallStart(), 
                    INSTALL_NOT_STARTED );
            }

            if ( osName.indexOf("Windows") >= 0 || osName.equals("Win32s") || 
                osName.equals("OS/2") ) 
            {
                profile = new WinProfile(this, folder, file);
            }
        }
        catch (Throwable e) {
            profile = null;
            e.printStackTrace();
        }

        return profile;
    }

    /**
     * @return an object for manipulating the Windows Registry.
     *          Null if you're not on windows
     */
    public Object GetWinRegistry()
    {
        WinReg      registry = null;
        String      osName = System.getProperty("os.name");

        try {
            if ( packageName == null )
                throw new SoftUpdateException(
                    Strings.error_WinProfileMustCallStart(),
                    INSTALL_NOT_STARTED);

            if ( osName.indexOf("Windows") >= 0 || osName.equals("Win32s")) {
                registry = new WinReg(this);
            }
        }
        catch (Throwable e) {
            registry = null;
            e.printStackTrace();
        }

        return registry;
    }

    /* JAR file manipulation */

    private String installerJarName;    // Name of the installer file
    private String jarName;             // Name of the JAR file
    private int zigPtr;                 // Stores the pointer to ZIG *
    private netscape.security.Principal installPrincipal; /* principal with the signature from the JAR file */


    /* Open/close the jar file
     */
    private native void OpenJARFile() throws SoftUpdateException;
    private native void CloseJARFile();

    /* getCertificates
     * native encapsulation that calls AppletClassLoader.getCertificates
     * we cannot call this method from Java because it is private.
     * The method cannot be made public because it is a security risk
     */
    private native Object[] getCertificates(int zigPtr, String inJarLocation);

    /**
     * extract the file out of the JAR directory and places it into temporary
     * directory.
     * two security checks:
     * - the certificate of the extracted file must match the installer certificate
     * - must have privileges to extract the jar file
     * @param inJarLocation  file name inside the JAR file
     */
    protected String
    ExtractJARFile( String inJarLocation, String finalFile ) throws SoftUpdateException,
                                                   netscape.security.AppletSecurityException
    {
        if (zigPtr == 0)
            throw new SoftUpdateException("JAR file has not been opened", UNKNOWN_JAR_FILE);

        /* Security checks */
        Target target;
        PrivilegeManager privMgr;
        privMgr = AppletSecurity.getPrivilegeManager();
        target = Target.findTarget( targetNames[LIMITED_INSTALL] );

        privMgr.checkPrivilegeEnabled(target);

        /* Make sure that the certificate of the extracted file matches
            the installer certificate */
        /* XXX this needs to be optimized, so that we do not create a principal
            for every certificate */
        {
            int i;
            boolean haveMatch = false;
            Object[] certArray  = null;
            certArray = getCertificates( zigPtr, inJarLocation );
            if (certArray == null || certArray.length == 0)
            {
                throw new SoftUpdateException("Missing certificate for "
                    + inJarLocation, NO_CERTIFICATE);
            }

            for (i=0; i < certArray.length; i++)
            {
                netscape.security.Principal prin = new netscape.security.Principal( Principal.CERT_KEY,
                                                        (byte[])certArray[i] );
                if (installPrincipal.equals( prin ))
                {
                    haveMatch = true;
                    break;
                }
            }
            if (haveMatch == false)
                throw new SoftUpdateException(Strings.error_MismatchedCertificate() + inJarLocation,
                                            NO_MATCHING_CERTIFICATE);
        }

        /* Extract the file */
//      System.out.println("Extracting " + inJarLocation);
        String outExtractLocation;
        outExtractLocation = NativeExtractJARFile(inJarLocation, finalFile);
//      System.out.println("Extracted to " + outExtractLocation);
        return outExtractLocation;
   }

    private native String
    NativeExtractJARFile( String inJarLocation, String finalFile ) throws   SoftUpdateException;

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
     * @param securityLevel     ignored (was LIMITED_INSTALL or FULL_INSTALL)
     */
    public int
    StartInstall(String vrPackageName,
                 VersionInfo inVInfo,
                 int securityLevel)
    {
        int errcode = SUCCESS;
        ResetError();

        try
        {
            if ( (vrPackageName == null) )
                throw new SoftUpdateException( Strings.error_BadPackageName() , INVALID_ARGUMENTS );

            packageName = vrPackageName;
            while ( packageName.endsWith("/")) // Make sure that package name does not end with '/'
            {
                char c[] = new char[packageName.length() - 1];
                packageName.getChars(0, packageName.length() -1, c, 0);
                packageName = new String( c );
                // System.out.println("Truncated package name is "  + packageName);

            }
            versionInfo = inVInfo;
            installedFiles = new java.util.Vector();

            /* JAR initalization */
            /* open the file, create a principal out of installer file certificate */
            OpenJARFile();
            InitializeInstallerCertificate();
            CheckSilentPrivileges();
            RequestSecurityPrivileges(securityLevel);
            progress = new ProgressMediator(this);
            progress.StartInstall();

            // set up default package folder, if any
            String path = VersionRegistry.getDefaultDirectory( packageName );
            if ( path != null )
            {
                packageFolder = new FolderSpec("Installed", path, userPackageName);
            }
        }

        catch(SoftUpdateException e)
        {
            errcode = e.GetError();
        }
        catch(Exception e)
        {
            e.printStackTrace();
            errcode = UNEXPECTED_ERROR;
        }

        saveError( errcode );
        return errcode;
    }

    /**
     * An alternate form that doesn't require the security level
     */
    public int
    StartInstall(String vrPackageName, VersionInfo inVInfo )
    {
        return StartInstall( vrPackageName, inVInfo, FULL_INSTALL );
    }

    /*
     * UI feedback
     */
    protected void
    UserCancelled()
    {
        userChoice = 0;
        AbortInstall();
    }

    protected void
    UserApproved()
    {
        userChoice = 1;
    }

    /**
     * Proper completion of the install
     * Copies all the files to the right place
     * returns 0 on success, <0 error code on failure
     */
    public int
    FinalizeInstall()
    {
        boolean rebootNeeded = false;
        int result = SUCCESS;
        try
        {

            if (packageName == null) // didn't call StartInstall()
            {
                throw new SoftUpdateException(
                    Strings.error_WinProfileMustCallStart(), 
                    INSTALL_NOT_STARTED );
            }

            // Wait for user approval
            progress.ConfirmWithUser();
            while ( userChoice == -1 )
                Thread.sleep(10);

            if (userChoice != 1)
            {
                AbortInstall();
                lastError = USER_CANCELLED;
                return USER_CANCELLED;
            }

            // Register default package folder if set
            if ( packageFolder != null )
            {
                VersionRegistry.setDefaultDirectory( packageName, 
                    packageFolder.toString() );
            }
            
            /* call Complete() on all the elements */
            /* If an error occurs in the middle, call Abort() on the rest */
            java.util.Enumeration   ve = GetInstallQueue();
            InstallObject   ie = null;
            
            try     // Main loop
            {
                while ( ve.hasMoreElements() )
                {
                    ie = (InstallObject)ve.nextElement();
                    try {
                        ie.Complete();
                    } 
                    catch (SoftUpdateException e) {
                        if (e.GetError() == REBOOT_NEEDED)
                            rebootNeeded = true;
                        else
                            throw e;
                    }
                }
            }
            catch (Throwable e)
            {
                if (ie != null)
                    ie.Abort();
 
                while (ve.hasMoreElements())
                {
                    try 
                    {
                        ie = (InstallObject)ve.nextElement();
                        ie.Abort();
                    }
                    catch(Throwable t)
                    {
                        // t.printStackTrace();
                    }
                }
                throw(e);   // Rethrow out of main loop
            }
            
            // add overall version for package
            if ( versionInfo != null )
            {
                result = VersionRegistry.installComponent(
                                    packageName, null, versionInfo );
            }
        }
        catch (SoftUpdateException e)
        {
            result = e.GetError();
        }
        catch (netscape.security.AppletSecurityException e)
        {
            result = ACCESS_DENIED;
        }
        catch (Throwable e)
        {
            e.printStackTrace();
            System.out.println( Strings.error_Unexpected() + " FinalizeInstall");
            result = UNEXPECTED_ERROR;
        }
        finally
        {
            if ( installedFiles != null )
                installedFiles.removeAllElements();
            CleanUp();
        }

        if (result == SUCCESS && rebootNeeded)
            result = REBOOT_NEEDED;

        saveError( result );
        return result;
    }

    /**
     * Aborts the install :), cleans up all the installed files
     */
    public synchronized void AbortInstall()
    {
        try
        {
            if (installedFiles != null)
            {
                java.util.Enumeration   ve = GetInstallQueue();
                while ( ve.hasMoreElements() )
                {
                    InstallObject   ie;
                    ie = (InstallObject)ve.nextElement();
                    try // We just want to loop through all of them if possible
                    {
                        ie.Abort();
                    }
                    catch (Throwable t)
                    {
                    }
                }       
                installedFiles.removeAllElements();
                installedFiles = null;
            }
            CloseJARFile();
        }
        catch (Throwable e)
        {
            System.out.println( Strings.error_Unexpected() + " AbortInstall");
            e.printStackTrace();
        }
        finally
        {
            try {
                CleanUp();
            }
            catch (Throwable e) {
                e.printStackTrace();
            }
        }
    }

    /*
     * CleanUp
     * call it when done with the install
     *
     */
    private synchronized void CleanUp()
    {

        if (progress != null)
            progress.Complete();
        
        CloseJARFile();
        progress = null;
        zigPtr = 0;
        if ( installedFiles != null )
        {
//            if (installedFiles.size() != 0)
//                System.err.println("Installed files are not empty!");
            java.util.Enumeration   ve = GetInstallQueue();
            while ( ve.hasMoreElements() )
            {
                InstallObject   ie;
                ie = (InstallObject)ve.nextElement();
                ie.Abort();
            }       
            installedFiles.removeAllElements();
        }
        installedFiles = null;
        packageName = null;  // used to see if StartInstall() has been called
    }

    /**
     * ScheduleForInstall
     * call this to put an InstallObject on the install queue
     * Do not call installedFiles.addElement directly, because this routine also handles
     * progress messages
     */
    protected void ScheduleForInstall(InstallObject ob)
    {
        if (progress != null)
            progress.ScheduleForInstall( ob );
        installedFiles.addElement( ob );
    }

    /**
     * GetQualifiedRegName
     *
     * This routine converts a package-relative component registry name
     * into a full name that can be used in calls to the version registry.
     */
    private String GetQualifiedRegName( String name )
    {
        String packagePrefix = (packageName.length() == 0) ? "" : packageName+"/" ;

        if (name.toUpperCase().startsWith("=COMM=/"))
            // bug 66403: JRI_GetStringUTFChars() returns bad string in native
            // code after a substring() method call (str looks OK in java!)
            // name = name.substring(7);
            name = (name.substring(7)+"===").substring(0,name.length()-7);
        else if (name.charAt(0) != '/')   // Relative package path
            name = packagePrefix + name;

        return name;
    }



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
     * XXX Subdirectory stuff NOT IMPLEMENED YET
     *     jarSource and localDestionation can also point to directories.  In this case,
     *     directory will be recursively copied from jarSource to localDestination, with
     *     version of each file specified in VersionInfo agument. If
     *     one points to a file, and another to a directory, that is an error.
     *     If jarSource points to a directory and localDestination doesn't exist it
     *     is created as a directory.
     */
     public int AddSubcomponent(String name,
                    VersionInfo version,
                    String jarSource,
                    FolderSpec folderSpec,
                    String relativePath,
                    boolean forceInstall)
     {
        InstallFile ie;
        int   result = SUCCESS;
        try
        {
            if ( jarSource == null || jarSource.length() == 0 || folderSpec == null )
                throw (new SoftUpdateException( "", INVALID_ARGUMENTS ));

            if (packageName == null) {
                // probably didn't call StartInstall()
                throw new SoftUpdateException( Strings.error_BadPackageNameAS(),
                                               BAD_PACKAGE_NAME);
            }

            if ((name == null) || (name.length() == 0)) {
                // Default subName = location in jar file
                name = GetQualifiedRegName( jarSource );
            }
            else
                name = GetQualifiedRegName( name );

            if ( (relativePath == null) || (relativePath.length() == 0))
                relativePath = jarSource;

            /* Check for existence of the newer version */

            boolean versionNewer = false;
            if ( (forceInstall == false ) &&
                 (version !=  null) &&
                 ( VersionRegistry.validateComponent( name ) == 0 ) )
            {
                VersionInfo oldVersion = VersionRegistry.componentVersion(name);
                if ( version.compareTo( oldVersion ) > 0 )
                    versionNewer = true;
            }
            else
                versionNewer = true;

            if (versionNewer)
            {
                ie = new InstallFile( this, name, version, jarSource,
                        folderSpec, relativePath, forceInstall );
                ie.ExtractFile();
                ScheduleForInstall( ie );
            }
        }
        catch (SoftUpdateException e)
        {
            result = e.GetError();
        }
        catch (netscape.security.AppletSecurityException e)
        {
            result = ACCESS_DENIED;
        }
        catch (Throwable e)
        {
            System.out.println(Strings.error_Unexpected() + "AddSubcomponent");
            e.printStackTrace();
            result = UNEXPECTED_ERROR;
        }

        saveError( result );
        return result;
    }


    /** 
     * Various alternate flavors of AddSubcomponent()
     */

    public int AddSubcomponent( String jarSource )
    {
        return AddSubcomponent( "", versionInfo, jarSource, packageFolder, "", force );
    }
    
    public int AddSubcomponent( String name, String jarSource )
    {
        return AddSubcomponent( name, versionInfo,  jarSource, packageFolder, "", force );
    }
    
    public int AddSubcomponent( String regname, String jarSource, boolean forceInstall )
    {
        return AddSubcomponent( regname, versionInfo, jarSource, packageFolder, "", forceInstall );
    }
    
    public int AddSubcomponent( String regname, VersionInfo version, String jarSource )
    {
        return AddSubcomponent( regname, version, jarSource, packageFolder, "", force );
    }
    
    public int AddSubcomponent( String regname, VersionInfo version, String jarSource, boolean forceInstall )
    {
        return AddSubcomponent( regname, version, jarSource, packageFolder, "", forceInstall );
    }
   
    public int AddSubcomponent( String regname, String jarSource, String relativePath )
    {
        return AddSubcomponent( regname, versionInfo, jarSource, packageFolder, relativePath, force );
    }
 
    public int AddSubcomponent( String regname, String jarSource, String relativePath, boolean forceInstall )
    {
        return AddSubcomponent( regname, versionInfo, jarSource, packageFolder, relativePath, forceInstall );
    }
 
    public int AddSubcomponent( String regname, String jarSource, FolderSpec folderSpec, String relativePath )
    {
        return AddSubcomponent( regname, versionInfo, jarSource, folderSpec, relativePath, force );
    }
    
    public int AddSubcomponent( String regname, String jarSource, FolderSpec folderSpec, String relativePath, 
                                boolean forceInstall )
    {
        return AddSubcomponent( regname, versionInfo,  jarSource, folderSpec, relativePath, forceInstall );
    }
    
    public int AddSubcomponent( String regname, VersionInfo version, String jarSource, FolderSpec folderSpec, 
                                String relativePath )
    {
        return AddSubcomponent( regname, version, jarSource, folderSpec, relativePath, force );
    }


    /**
     * executes the file
     * @param jarSource     name of the file to execute inside JAR archive
     * @param args          command-line argument string (Win/Unix only)
     */
    public int
    Execute(String jarSource)
    {
        return Execute( jarSource, null );
    }

    public int
    Execute(String jarSource, String args)
    {
        int errcode = SUCCESS;

        try
        {
            InstallExecute ie = new InstallExecute( this, jarSource, args );
            ie.ExtractFile();
            ScheduleForInstall( ie );
        }
        catch (SoftUpdateException e)
        {
            errcode = e.GetError();
        }
        catch (netscape.security.AppletSecurityException e)
        {
            errcode = ACCESS_DENIED;
        }
        catch (Throwable e)
        {
            e.printStackTrace();
            System.out.println(Strings.error_Unexpected() + "Execute");
            errcode = UNEXPECTED_ERROR;
        }

        saveError( errcode );
        return errcode;
    }



   /**
     * Mac-only, simulates Mac toolbox Gestalt function
     * OSErr Gestalt(String selector, long * response)
     * @param selector      4-character string, 
     * @return      2 item array. 1st item corresponds to OSErr, 
     *                       2nd item corresponds to response
     */
    public int[] Gestalt(String selector)
    {
        int a[] = new int[2];
        a[0] = 0;
        a[1] = 0;
        try
        {
            a[1] = NativeGestalt(selector);
        }
        catch (SoftUpdateException s)
        {
            a[0] = s.GetError();
        }
        catch (Throwable t)
        {
            a[0] = -1;
            System.out.println(Strings.error_Unexpected() + "Gestalt");
        }
        return a;
    }
    
    private native int
    NativeGestalt(String selector) throws SoftUpdateException;



    /**
     * Patch
     *
     */

    public int Patch( String regName, VersionInfo version, String patchname )
    {
        int errcode = SUCCESS;

        if ( regName == null || patchname == null )
            return saveError( INVALID_ARGUMENTS );

        String rname = GetQualifiedRegName( regName );

        try {
            InstallPatch ip = new InstallPatch(this, rname, version, patchname);
            ip.ApplyPatch();
            ScheduleForInstall( ip );
        }
        catch (SoftUpdateException e) {
            errcode = e.GetError();
        }
        catch (Throwable e)
        {
            System.out.println(Strings.error_Unexpected() + "Patch");
            e.printStackTrace();
            errcode = UNEXPECTED_ERROR;
        }
        saveError( errcode );
        return errcode;
    }

    public int Patch( String regName, String patchname )
    {
        return Patch( regName, versionInfo, patchname );
    }

    public int
    Patch( String regName, VersionInfo version, String patchname,
            FolderSpec folder, String filename )
    {
        if ( folder == null || regName == null || regName.length() == 0 || patchname == null )
            return saveError( INVALID_ARGUMENTS );

        int errcode = SUCCESS;

        try {
            String rname = GetQualifiedRegName( regName );

            InstallPatch ip = new InstallPatch( this, rname, version,
                                                patchname, folder, filename );
            ip.ApplyPatch();
            ScheduleForInstall( ip );
        }
        catch (SoftUpdateException e) {
            errcode = e.GetError();
        }
        catch (Throwable e) {
            System.out.println(Strings.error_Unexpected() + "Patch");
            e.printStackTrace();
            errcode = UNEXPECTED_ERROR;
        }
        saveError( errcode );
        return errcode;
    }

    public int
    Patch( String regName, String patchname, FolderSpec folder, String file )
    {
        return Patch( regName, versionInfo, patchname, folder, file );
    }
    
    private native int NativeDeleteFile(String path);
    private native int NativeVerifyDiskspace (String path, int bytesRequired);
    private native int NativeMakeDirectory (String path);
    private native String[] ExtractDirEntries(String Dir);

    public int DeleteFile( FolderSpec folder, String relativeFileName )
    {
        // deletes specified file from disk w/o looking in the registry. folder object obtained 
        // using GetFolder() representing directory containing file. relative path and file name
        // Returns usual error codes + an error indicating file does not exist.
    
        String path;
        int status = -1; 
    
        try 
        {
            path = ((FolderSpec)folder).MakeFullPath(relativeFileName);
            status = NativeDeleteFile(path);
        }
        catch ( Exception e )
        {
            path = null;
        }
        return status; 
    }
    
    
    public int DeleteComponent(String registryName)
    {
        // Finds named registry component and deletes both the file and the entry in the Client 
        // VR. registryNameThe name of the component in the registry.
        // Returns usual errors codes + code to indicate item doesn't exist in registry, registry 
        // item wasn't a file item, or the related file doesn't exist. If the file is in use we will
        // store the name and to try to delete it on subsequent start-ups until we're successful.
    
        int status = -1; 
    
        FolderSpec spec = (FolderSpec)GetComponentFolder(registryName, null);
        
        if (spec != null)
        {
            status = VersionRegistry.deleteComponent(registryName);
            if ( status == 0 )
                status = DeleteFile(spec, ""); /*XXX*/
        }    
        return status; 
    }
    
    
    public FolderSpec GetFolder( String targetFolder, String subdirectory )
    {
        return GetFolder( GetFolder(targetFolder), subdirectory );
    }
    

    public FolderSpec GetFolder( FolderSpec folder, String subdir )
    {
        FolderSpec spec = null;
        String     path;
        String     newPath;

        if ( subdir == null || subdir.length() == 0 )
        {
            // no subdir, return what we were passed
            spec = folder;
        }
        else if ( folder != null )
        {
            try { 
                path = folder.MakeFullPath( subdir );
            } catch (Exception e ) {
                path = null;
            }

            if (path != null)
            {
                newPath = path + System.getProperty("file.separator");
                spec = new FolderSpec("Installed", newPath, userPackageName);
                if (spec != null)
                {
                    if (NativeMakeDirectory(path) != 0)
                        spec = null;
                }
            }
        }
        return spec;
    }

        
    
   // Returns true if there is enough free diskspace, false if there isn't.
   // The drive containg the folder is checked for # of free bytes.
    
    public boolean VerifyDiskspace( FolderSpec folder, int bytesRequired )
    {
        String path = folder.toString();

        if (path == null)
            return false;

        return ( (NativeVerifyDiskspace (path, bytesRequired)) != 0);

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
        
        
    public int AddDirectory(  String name, 
                                VersionInfo version, 
                                String jarSource,
                                FolderSpec folderSpec, 
                                String subdir, 
                                boolean forceInstall)
    
    {
        InstallFile ie;
        int   result = SUCCESS;
        try
        {
            if ( jarSource == null || jarSource.length() == 0 || folderSpec == null )
                throw (new SoftUpdateException( "", INVALID_ARGUMENTS ));

            if (packageName == null) {
                // probably didn't call StartInstall()
                throw (new SoftUpdateException( Strings.error_BadPackageNameAS(), 
                    BAD_PACKAGE_NAME));
            }
    
            if ((name == null) || (name.length() == 0)) {
                // Default subName = location in jar file
                name = GetQualifiedRegName( jarSource );
            }
            else 
                name = GetQualifiedRegName( name );

            if ( subdir == null ) {
                subdir = "";
            }
            else if ( subdir.length() != 0 ) {
                subdir += "/";
            }


            System.out.println("AddDirectory " + name 
                + " from " + jarSource + " into " + 
                folderSpec.MakeFullPath(subdir));
         

            String[] matchingFiles = ExtractDirEntries( jarSource );
            int i;
            boolean bInstall;
            VersionInfo oldVer;
             
            for(i=0; i< matchingFiles.length;i++)
            {
                String fullRegName = name + "/" + matchingFiles[i];

                if ( (forceInstall == false) && (version != null) &&
                    (VersionRegistry.validateComponent(fullRegName) == 0) )
                {
                    // Only install if newer
                    oldVer = VersionRegistry.componentVersion(fullRegName);
                    bInstall = ( version.compareTo(oldVer) > 0 );
                }
                else {
                    // file doesn't exist or "forced" install
                    bInstall = true;
                }

                if ( bInstall ) {
                    ie = new InstallFile( this, 
                                fullRegName, 
                                version,
                                jarSource + "/" + matchingFiles[i], 
                                (FolderSpec) folderSpec, 
                                subdir + matchingFiles[i], 
                                forceInstall );
                    ie.ExtractFile();
                    ScheduleForInstall( ie );
                }
             }        
      }
      catch (SoftUpdateException e)
      {
          result = e.GetError();
      }
      catch (ForbiddenTargetException e)
      {
          result = ACCESS_DENIED;
      }
      catch (Throwable e)
      {
          System.out.println(Strings.error_Unexpected() + "AddDirectory");
          result = UNEXPECTED_ERROR;
      }
      
      saveError( result );
      return (result);
    }

    // Alternate forms of AddDirectory()

    public int AddDirectory( String jarSource )
    {
        return AddDirectory( "", versionInfo, jarSource, packageFolder, 
                    "", force );
    }

    public int AddDirectory( String regName, String jarSource )
    {
        return AddDirectory( regName, versionInfo, jarSource, packageFolder,
                    "", force );
    }

    public int AddDirectory( String regName, String jarSource, boolean forceMode )
    {
        return AddDirectory( regName, versionInfo, jarSource, packageFolder,
                    "", forceMode );
    }

    public int AddDirectory( String regName, VersionInfo version, String jarSource)
    {
        return AddDirectory( regName, version, jarSource, packageFolder,
                    "", force );
    }

    public int AddDirectory( String regName, VersionInfo version, String jarSource,
                             boolean forceMode )
    {
        return AddDirectory( regName, version, jarSource, packageFolder, 
                    "", forceMode );
    }

    public int AddDirectory( String regName, String jarSource, 
                             FolderSpec folder, String subdir )
    {
        return AddDirectory( regName, versionInfo, jarSource, folder, subdir, force );
    }

    public int AddDirectory( String regName, String jarSource,
                             FolderSpec folder, String subdir, boolean forceMode )
    {
        return AddDirectory( regName, versionInfo, jarSource, folder, subdir, forceMode );
    }

    public int AddDirectory( String regName, VersionInfo version, String jarSource,
                             FolderSpec folder, String subdir )
    {
        return AddDirectory( regName, version, jarSource, folder, subdir, force );
    }

}
