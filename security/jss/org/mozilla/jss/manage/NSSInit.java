/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Security Services for Java.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
package org.mozilla.jss;

import org.mozilla.jss.util.Debug;
import org.mozilla.jss.util.PasswordCallback;
import org.mozilla.jss.util.ConsolePasswordCallback;
import org.mozilla.jss.KeyDatabaseException;
import org.mozilla.jss.CertDatabaseException;
import org.mozilla.jss.crypto.AlreadyInitializedException;

/**
 * This class initializes Java NSS and sets up the password callback.
 */
public final class NSSInit {

    /********************************************************************/
    /* The following VERSION Strings should be updated in the following */
    /* files everytime a new release of JSS is generated:               */
    /*                                                                  */
    /*     jssjava:  ns/ninja/cmd/jssjava/jssjava.c                     */
    /*     jss.jar:  ns/ninja/org/mozilla/jss/manage/NSSInit.java      */
    /*     jss.dll:  ns/ninja/org/mozilla/jss/manage/NSSInit.c         */
    /*                                                                  */
    /********************************************************************/

    public static final String
    JAR_JSS_VERSION     = "JSS_VERSION = JSS_2_1   11 Dec 2000";
    public static final String
    JAR_JDK_VERSION     = "JDK_VERSION = JDK 1.2.2";
    public static final String
    JAR_SVRCORE_VERSION = "SVRCORE_VERSION = SVRCORE_2_5_1";
    public static final String
    JAR_NSS_VERSION     = "NSS_VERSION = NSS_2_8_4_RTM";
    public static final String
    JAR_DBM_VERSION     = "DBM_VERSION = DBM_1_54";
    public static final String
    JAR_NSPR_VERSION    = "NSPR_VERSION = v3.5.1";

    /**
     * Loads the JSS dynamic library if necessary.
     * The system property "jss.load" will be set to "no" by jssjava
     * because it is statically linked to the jss libraries. If this
     * property is not set, that means we are not running jssjava
     * and need to dynamically load the library.
     * <p>This method is idempotent.
     */
    synchronized static void loadNativeLibraries()
    {
        if( ! mNativeLibrariesLoaded &&
            ! ("no").equals(System.getProperty("jss.load")) )
        {
            try {
                Debug.trace(Debug.VERBOSE, "about to load jss library");
                System.loadLibrary("jss21");
                Debug.trace(Debug.VERBOSE, "jss library loaded");
            } catch( UnsatisfiedLinkError e) {
                Debug.trace(Debug.ERROR, "ERROR: Unable to load jss library");
                throw e;
            }
            mNativeLibrariesLoaded = true;
        }
    }
    static private boolean mNativeLibrariesLoaded = false;

    /**
     * Initialize Java NSS. This method opens the security module, key,
     * and certificate databases and initializes the Random Number Generator.
     * The certificate and key databases are opened in read-only mode.
     *
     * <p>This method also attempts to load the native implementation library.
     * On UNIX systems, this library is named <code>libjss.so</code>,
     * and it must be present in the <code>LD_LIBRARY_PATH</code>.
     * On Windows systems, the library is named
     * <code>jss.dll</code> and must be present in the <code>PATH</code>.
     * If the library cannot be found, an <code>UnsatisfiedLinkError</code>
     * is thrown.
     *
     * <p>This method should only be called once by an application,
	 *	otherwise an
     * <code>AlreadyInitializedException</code> will be thrown.
     *
     * @param modDBName The complete path, relative or absolute, of the
	 *		security module database.
     *      If it does not exist, it will be created.
     * @param keyDBName The complete path, relative or absolute, of the key
	 *		database. It must already exist.
     * @param certDBName The complete path, relative or absolute, of the
	 *		certificate database. It must already exist.
     * @exception KeyDatabaseException If the key database does not exist
     *      or cannot be opened.
     * @exception CertDatabaseException If the certificate database does
     *      not exist or cannot be opened.
     * @exception AlreadyInitializedException If this method has already
     *      been called.
	 * @exception UnsatisfiedLinkError If the implementation dynamic library
	 *		cannot be found or loaded.
     */
    public static synchronized void
    initialize( String modDBName, String keyDBName,
        String certDBName )
        throws KeyDatabaseException, CertDatabaseException,
        AlreadyInitializedException
    {
        if (mNSSInitialized) throw new AlreadyInitializedException();
 
        loadNativeLibraries();
        initializeNative(modDBName,
                         keyDBName,
                         certDBName,
                         true,      // readOnly
                         "Netscape Communications Corp     ",
                         "Internal Crypto Services         ",
                         "Internal Crypto Services Token   ",
                         "Internal Key Storage Token       ",
        "Netscape Internal Cryptographic Services                         ",
        "Netscape Internal Private Key and Certificate Storage            ",
        "Netscape Internal FIPS-140-1 Cryptographic Services              ",
        "Netscape Internal FIPS-140-1 Private Key and Certificate Storage ");

        setPasswordCallback( new ConsolePasswordCallback() );
        mNSSInitialized = true;
    }
    static private boolean mNSSInitialized = false;

    /**
     * Indicates whether Java NSS has been initialized.
     *
     * @return <code>true</code> if <code>initialize</code> has been called,
     * <code>false</code> otherwise.
     */
    public static synchronized boolean isInitialized()
    {
       return mNSSInitialized;
    }

    private static native void initializeNative(
                String modDBName,
                String keyDBName,
                String certDBName,
                boolean readOnly,
                String manuString,
                String libraryString,
                String tokString,
                String keyTokString,
                String slotString,
                String keySlotString,
                String fipsString,
                String fipsKeyString)
        throws KeyDatabaseException, CertDatabaseException,
        AlreadyInitializedException;

    /**
     * Sets the password callback.
     * This password callback will be called when access is required
     * to the key database, and to any PKCS #11 token. Once a token
     * has been logged into successfully, it is not necessary to login to
     * that token again. By default,
     * a <code>ConsolePasswordCallback</code> is used to obtain passwords
     * from the console.
     *
     * <p>This method may be called multiple times to alter the password
     * callback.
     * 
     * @see org.mozilla.jss.util.PasswordCallback
     * @see org.mozilla.jss.util.ConsolePasswordCallback
     */
    public static synchronized native void
    setPasswordCallback(PasswordCallback cb);
}
