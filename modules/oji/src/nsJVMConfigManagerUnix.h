/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Initial Developer of the Original Code is Sun Microsystems.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pete Zha <pete.zha@sun.com> (original author)
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

#ifndef nsJVMConfigManagerUnix_h___
#define nsJVMConfigManagerUnix_h___

#include "nsJVMConfigManager.h"
#include "nsString.h"
#include "nsILineInputStream.h"
#include "nsHashtable.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsIFileStreams.h"
#include "nsArray.h"

class nsJVMConfigManagerUnix : public nsIJVMConfigManager
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIJVMCONFIGMANAGER

    nsJVMConfigManagerUnix();
    virtual ~nsJVMConfigManagerUnix();

protected:
    nsresult InitJVMConfigList();

    void ClearJVMConfigList();

    nsresult InitJVMConfigList(nsILineInputStream* aGlobal,
                               nsILineInputStream* aPrivate);

    /**
     * Parse a stream for information about Java installation(s).
     */
    nsresult ParseStream(nsILineInputStream* aStream);

    /**
     * Parse a line for information about a Java installation, and ensure its
     * JVMConfig is in our JVMConfigList."
     * A line looks like this:
     * "version=1.4.2 | type=jre | os=linux | arch=i386 | ......"
     */

    nsresult ParseLine(nsAString& aLine);

    /**
     * Search for Java installations in the default location.
     */
    nsresult SearchDefault();

    /**
     * Search a specific directory to see if there are Java installations.
     */
    nsresult SearchDirectory(nsAString& aDirName);

    /**
     * Add a directory to the Java configuration list if
     * it contains a java installation.
     */
    nsresult AddDirectory(nsIFile* aHomeDir);
    
    nsresult AddDirectory(nsAString& aHomeDirName);

    /**
     * Get a value by specific key from the line. A line should look like this:
     * key=value | key2=value2 | key3=value3
     * Please see the proposal in bug 185000 for more detail about the format.
     */
    static PRBool GetValueFromLine(nsAString& aLine, const char* aKey,
                                   nsAString& _retval);

    static nsresult GetLineInputStream(nsIFile* aFile,
                                       nsILineInputStream** _retval);

    /**
     * Get value of mozilla<version>.plugin.path from a line
     */
    static nsresult GetMozillaPluginPath(nsAString& aLine, nsAString& _retval);

    /**
     * Get agent version by string
     */
    static nsresult GetAgentVersion(nsCAutoString& _retval);

    /**
     * Get agent version by float
     */
    static nsresult GetAgentVersion(float* _retval);

    static nsresult GetNSVersion(nsAString& _retval);

    /**
     * Check for existing arch directory.
     */
    static PRBool TestArch(nsILocalFile* aPluginPath, nsAString& aArch);

    /**
     * Check for existing NS version directory.
     */
    static PRBool TestNSVersion(nsILocalFile* aArchPath, nsAString& aNSVersion);

    /**
     * Test if a specific node in the base directory exists.
     */
    static PRBool TestExists(nsILocalFile* aBaseDir, nsAString& aSubName);

    /**
     * The table to store the config list.
     */
    nsHashtable mJVMConfigList;
};

#endif // nsJVMConfigManagerUnix_h___
