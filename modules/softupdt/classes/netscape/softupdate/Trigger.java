/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
