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
package netscape.softupdate ;

public class VersionInfo {
    public static final int MAJOR_DIFF =  4;
    public static final int MINOR_DIFF =  3;
    public static final int REL_DIFF   =  2;
    public static final int BLD_DIFF   =  1;
    public static final int EQUAL      =  0;

    /* Equivalent to Version Registry structure fields */
    private int major      = 0;
    private int minor      = 0;
    private int release    = 0;
    private int build      = 0;
    private int check      = 0;

    public VersionInfo(int maj, int min, int rel, int bld)
    {
        this(maj, min, rel, bld, 0);
    }

    public VersionInfo(int maj, int min, int rel, int bld, int checksum)
    {
        major   = maj;
        minor   = min;
        release = rel;
        build   = bld;
        check   = checksum;
    }

    public VersionInfo(String version)
    {
        int dot = version.indexOf('.');

        try {
            if ( dot == -1 ) {
                major = Integer.parseInt(version);
            }
            else  {
                major = Integer.parseInt(version.substring(0,dot));

                int prev = dot+1;
                dot = version.indexOf('.',prev);
                if ( dot == -1 ) {
                    minor = Integer.parseInt(version.substring(prev));
                }
                else {
                    minor = Integer.parseInt(version.substring(prev, dot));

                    prev = dot+1;
                    dot = version.indexOf('.',prev);
                    if ( dot == -1 ) {
                        release = Integer.parseInt(version.substring(prev));
                    }
                    else {
                        release = Integer.parseInt(version.substring(prev,dot));

                        if ( version.length() > dot ) {
                            build = Integer.parseInt(version.substring(dot+1));
                        }
                    }
                }
            }
        }
        catch (Exception e) {
           // parseInt can throw a NumberFormatException--in that case just
           // trap the exception so we don't blow away someone's JAR install.
        }
    }

    // Text representation of the version info
    public String toString()
    {
        String vers = String.valueOf(major)   + "." +
                    String.valueOf(minor)   + "." +
                    String.valueOf(release) + "." +
                    String.valueOf(build);
        // vers += ", checksum: " + check;
        return vers;
    }

    /*
     * compareTo() -- Compares version info.
     * Returns -n, 0, n, where n = {1-4}
     */
    public int compareTo(VersionInfo vi)
    {
        int diff;

        if ( vi == null ) {
            diff = MAJOR_DIFF;
        }
        else if ( this.major == vi.major ) {
            if ( this.minor == vi.minor ) {
                if ( this.release == vi.release ) {
                    if ( this.build == vi.build )
                        diff = EQUAL;
                    else if ( this.build > vi.build )
                        diff = BLD_DIFF;
                    else
                        diff = -BLD_DIFF;
                }
                else if ( this.release > vi.release )
                    diff = REL_DIFF;
                else
                    diff = -REL_DIFF;
            }
            else if ( this. minor > vi.minor )
                diff = MINOR_DIFF;
            else
                diff = -MINOR_DIFF;
        }
        else if ( this.major > vi.major )
            diff = MAJOR_DIFF;
        else
            diff = -MAJOR_DIFF;

        return diff;
    }

    public int compareTo(String version)
    {
        return compareTo(new VersionInfo(version));
    }

    public int compareTo(int maj, int min, int rel, int bld)
    {
        return compareTo(new VersionInfo(maj, min, rel, bld));
    }

}
