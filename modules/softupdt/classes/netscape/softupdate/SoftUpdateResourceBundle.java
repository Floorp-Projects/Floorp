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
/* Internationalization of string resources for SoftwareUpdate
 */
package netscape.softupdate;

import java.util.ListResourceBundle;

/**
 */

public class SoftUpdateResourceBundle extends ListResourceBundle {

	/**
     * Overrides ListResourceBundle
     */
    public Object[][] getContents() {
        return contents;
    }

    static final Object[][] contents = {
        // Non-Localizable Key					// localize these strings
        {"s1", "Low"},
        {"s2", "Medium"},
        {"s3", "High"},
        {"s4", "Installing Java software on your computer."},
        {"s5", "Installing and running software on your computer. "},
        {"s6", "Installing and running software without warning you."},
        {"s7", "Reading and writing to a windows .INI file"},
        {"s8", "Reading and writing to the Windows Registry: Use extreme caution!"},
        {"s9", "SmartUpdate: "},
        {"s10", "Getting ready to install "},
        {"s11", "Press \"Install\" to install "},
        {"s12", ""},
        {"s13", "Installing "},
        {"s14", " will perform actions listed bellow:"},
        {"s15", "Detailed SmartUpdate information: "},
        {"s16", "Execute a program:"},
        {"s17", "SmartUpdate:"},
        {"s18", "Installer script does not have a certificate."},
        {"s19", "Installer script had more than one certificate"},
        {"s20", "Silent mode was denied."},
        {"s21", "Must call StartInstall() to initialize SoftwareUpdate."},
        {"s22", "Certificate of the Installer file does not match the certificate of the installed file"},
        {"s23", "Package name is null or empty in StartInstall."},
        {"s24", "Unexpected error in "},
        {"s25", "Bad package name in AddSubcomponent. Did you call StartInstall()?"},
        {"s26", "Illegal file path"},
        {"s27", "Unexpected error when installing a file"},
        {"s28", "Verification failed. 'this' must be passed to SoftwareUpdate constructor."},
        {"s29", "SmartUpdate disabled"},
        {"s30", "Security integrity check failed."},
        {"s31", "Could not get installer name out of the global MIME headers inside the JAR file."},
        {"s32", "Extraction of JAR file failed."},
        {"s33", "Install"},
        {"s34", "Cancel"},
        {"s35", "More Info..."},
        {"s36", "Close"},
        {"s37", "Installing Java class files in the \"Java Download\" directory. This form of access allows new files to be added to this single directory on your computer's main hard disk, potentially replacing other files that have previously been installed in the directory. "},
        {"s38", "Installing software on your computer's main hard disk, potentially deleting other files on the hard disk. Each time a program that has this form of access attempts to install software, it must display a dialog box that lets you choose whether to go ahead with the installation. If you go ahead, the installation program can execute any software on your computer. This potentially dangerous form of access is typically requested by an installation program after you have downloaded new software or a new version of software that you have previously installed. You should not grant this form of access unless you are installing or updating software from a reliable source."},
        {"s39", "Installing software on your computer's main hard disk without giving you any warning, potentially deleting other files on the hard disk. Any software on the hard disk may be executed in the process. This is an extremely dangerous form of access. It should be granted by system administrators only."},
        // stop localizing
    };
}
