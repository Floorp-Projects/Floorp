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
		{"s40", "Failure while deleting file. File does not exist. "},
		{"s41", "Failure while deleting file. File is read only. "},
		{"s42", "Failure while deleting file. File is a directory. "},
		{"s43", "Delete file "},
		{"s44", "Failure while deleting component. Component not found in registry. "},
		{"s45", "Delete component "},
        {"s46", "Replace file "},
        {"s47", "Install file "},
        // stop localizing
    };
}
