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
package netscape.softupdate;

/**
 * A class for 'triggering' the SmartUpdate and obtaining info
 * No instances, all static methods
 */
public class Trigger {

    /**
     * diff levels are borrowed from the VersionInfo class
     */
    public static final int MAJOR_DIFF =  4;
    public static final int MINOR_DIFF =  3;
    public static final int REL_DIFF   =  2;
    public static final int BLD_DIFF   =  1;
    public static final int EQUAL      =  0;

    /**
     * DEFAULT_MODE has UI, triggers conditionally
     * @see StartSoftwareUpdate flags argument
     *
     */
    public static int DEFAULT_MODE = 0;
    /**
     * FORCE_MODE will install the package regardless of what was installed previously
     * @see StartSoftwareUpdate flags argument
     */
    public static int FORCE_MODE = 1;
    /**
     * SILENT_MODE will not display the UI
     */
    public static int SILENT_MODE = 2;


    /**
     * @return true if SmartUpdate is enabled
     */
    public static native boolean UpdateEnabled();

    /**
     * @param componentName     version registry name of the component
     * @return  version of the package. null if not installed, or SmartUpdate disabled
     */
    public static VersionInfo GetVersionInfo( String componentName )
    {
        if (UpdateEnabled())
            return VersionRegistry.componentVersion( componentName );
        else
            return null;
    }

    /**
     * Unconditionally starts the software update
     * @param url   URL of the JAR file
     * @param flags SmartUpdate modifiers (SILENT_INSTALL, FORCE_INSTALL)
     * @return false if SmartUpdate did not trigger
     */
    public static native boolean StartSoftwareUpdate( String url, int flags );

    public static boolean StartSoftwareUpdate( String url )
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
     *                      object or a string version
     * @param flags         Same flags as StartSoftwareUpdate (force, silent)
     */
    public static boolean
    ConditionalSoftwareUpdate( String url,
                               String componentName,
                               int diffLevel,
                               VersionInfo version,
                               int flags)
    {
        try
        {
            boolean needJar = false;

            if ((version == null) || (componentName == null))
                needJar = true;
            else
            {
                int stat = VersionRegistry.validateComponent( componentName );
                if ( stat == VersionRegistry.REGERR_NOFIND ||
                     stat == VersionRegistry.REGERR_NOFILE )
                {
                    // either component is not in the registry or it's a file
                    // node and the physical file is missing
                    needJar = true;
                }
                else
                {
                    VersionInfo oldVer =
                        VersionRegistry.componentVersion( componentName );

                    if ( oldVer == null )
                        needJar = true;
                    else if ( diffLevel < 0 )
                        needJar = (version.compareTo( oldVer ) <= diffLevel);
                    else
                        needJar = (version.compareTo( oldVer ) >= diffLevel);
                }
            }

            if (needJar)
                return StartSoftwareUpdate( url, flags );
            else
                return false;
        }
        catch(Throwable e)
        {
            e.printStackTrace();
        }
        return false;
    }

    public static boolean
    ConditionalSoftwareUpdate( String url,
                               String componentName,
                               VersionInfo version)
    {
        return ConditionalSoftwareUpdate( url, componentName,
            BLD_DIFF, version, DEFAULT_MODE );
    }

    public static boolean
    ConditionalSoftwareUpdate( String url,
                               String componentName,
                               String version)
    {
        return ConditionalSoftwareUpdate( url, componentName,
            BLD_DIFF, new VersionInfo(version), DEFAULT_MODE );
    }

    public static boolean
    ConditionalSoftwareUpdate( String url,
                               String componentName,
                               VersionInfo version,
                               int flags)
    {
        return ConditionalSoftwareUpdate( url, componentName,
            BLD_DIFF, version, flags );
    }


    public static boolean
    ConditionalSoftwareUpdate( String url,
                               String componentName,
                               String version,
                               int flags)
    {
        return ConditionalSoftwareUpdate( url, componentName,
            BLD_DIFF, new VersionInfo(version), flags );
    }


    public static boolean
    ConditionalSoftwareUpdate( String url,
                               String componentName,
                               int diffLevel,
                               VersionInfo version)
    {
        return ConditionalSoftwareUpdate( url, componentName,
            diffLevel, version, DEFAULT_MODE );
    }


    public static boolean
    ConditionalSoftwareUpdate( String url,
                               String componentName,
                               int diffLevel,
                               String version)
    {
        return ConditionalSoftwareUpdate( url, componentName,
            diffLevel, new VersionInfo(version), DEFAULT_MODE );
    }


    public static boolean
    ConditionalSoftwareUpdate( String url,
                               String componentName,
                               int diffLevel,
                               String version,
                               int flags)
    {
        return ConditionalSoftwareUpdate( url, componentName,
            diffLevel, new VersionInfo(version), flags );
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

    public static int
    CompareVersion( String regName, VersionInfo version )
    {
        if (!UpdateEnabled())
            return EQUAL;

        VersionInfo regVersion = GetVersionInfo( regName );

        if ( regVersion == null ||
            VersionRegistry.validateComponent( regName ) ==
            VersionRegistry.REGERR_NOFILE ) {

            regVersion = new VersionInfo(0,0,0,0);
        }

        return regVersion.compareTo( version );
    }

    public static int
    CompareVersion( String regName, String version )
    {
        return CompareVersion( regName, new VersionInfo( version ) );
    }

    public static int
    CompareVersion( String regName, int maj, int min, int rel, int bld )
    {
        return CompareVersion( regName, new VersionInfo(maj, min, rel, bld) );
    }

}
