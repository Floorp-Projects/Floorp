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
 * The Original Code is the Netscape Security Services for Java.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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
package org.mozilla.jss;

import org.mozilla.jss.crypto.*;
import org.mozilla.jss.util.*;
import org.mozilla.jss.asn1.*;
import java.security.cert.CertificateException;
import java.security.GeneralSecurityException;
import org.mozilla.jss.pkcs11.PK11Cert;
import java.util.*;
import org.mozilla.jss.pkcs11.KeyType;
import org.mozilla.jss.pkcs11.PK11Token;
import org.mozilla.jss.pkcs11.PK11Module;
import org.mozilla.jss.pkcs11.PK11SecureRandom;
import java.security.cert.CertificateEncodingException;
import org.mozilla.jss.CRLImportException;
import org.mozilla.jss.provider.java.security.JSSMessageDigestSpi;

/**
 * This class is the starting poing for the crypto package.
 * Use it to initialize the subsystem and to lookup certs, keys, and tokens.
 * Initialization is done with static methods, and must be done before
 * an instance can be created.  All other operations are done with instance
 * methods.
 * @version $Revision: 1.27 $ $Date: 2004/07/20 20:39:24 $
 */
public final class CryptoManager implements TokenSupplier
{
    /**
     * CertUsage options for validation
     */
    public final static class CertUsage {
        private int usage;
        private String name;
        static private ArrayList list = new ArrayList();
        private CertUsage() {};
        private CertUsage(int usage, String name) {
            this.usage = usage;
            this.name =  name;
            this.list.add(this);

        }
        public int getUsage() {
            return usage;
        }

        static public Iterator getCertUsages() {
            return list.iterator();

        }
        public String toString() {
            return name;
        }



        // certUsage, these must be kept in sync with nss/lib/certdb/certt.h
        public static final CertUsage SSLClient = new CertUsage(0, "SSLClient");
        public static final CertUsage SSLServer = new CertUsage(1, "SSLServer");
        public static final CertUsage SSLServerWithStepUp = new CertUsage(2, "SSLServerWithStepUp");
        public static final CertUsage SSLCA = new CertUsage(3, "SSLCA");
        public static final CertUsage EmailSigner = new CertUsage(4, "EmailSigner");
        public static final CertUsage EmailRecipient = new CertUsage(5, "EmailRecipient");
        public static final CertUsage ObjectSigner = new CertUsage(6, "ObjectSigner");
        public static final CertUsage UserCertImport = new CertUsage(7, "UserCertImport");
        public static final CertUsage VerifyCA = new CertUsage(8, "VerifyCA");
        public static final CertUsage ProtectedObjectSigner = new CertUsage(9, "ProtectedObjectSigner");
        public static final CertUsage StatusResponder = new CertUsage(10, "StatusResponder");
        public static final CertUsage AnyCA = new CertUsage(11, "AnyCA");
    }

    public final static class NotInitializedException extends Exception {}
    public final static class NicknameConflictException extends Exception {}
    public final static class UserCertConflictException extends Exception {}
    public final static class InvalidLengthException extends Exception {}

    /**
     * The various options that can be used to initialize CryptoManager.
     */
    public final static class InitializationValues {
        protected InitializationValues() {
            Assert.notReached("Default constructor");
        }

        /////////////////////////////////////////////////////////////
        // Constants
        /////////////////////////////////////////////////////////////
        /**
         * Token names must be this length exactly.
         */
        public final int TOKEN_LENGTH = 33;
        /**
         * Slot names must be this length exactly.
         */
        public final int SLOT_LENGTH = 65;
        /**
         * ManufacturerID must be this length exactly.
         */
        public final int MANUFACTURER_LENGTH = 33;
        /**
         * Library description must be this length exactly.
         */
        public final int LIBRARY_LENGTH = 33;

        /**
         * This class enumerates the possible modes for FIPS compliance.
         */
        public static final class FIPSMode {
            private FIPSMode() {}

            /**
             * Enable FIPS mode.
             */
            public static final FIPSMode ENABLED = new FIPSMode();
            /**
             * Disable FIPS mode.
             */
            public static final FIPSMode DISABLED = new FIPSMode();
            /**
             * Leave FIPS mode unchanged.  All servers except Admin
             * Server should use this, because only Admin Server should
             * be altering FIPS mode.
             */
            public static final FIPSMode UNCHANGED = new FIPSMode();
                }

        public InitializationValues(String configDir) {
            this.configDir = configDir;
        }

        public InitializationValues(String configDir, String certPrefix,
            String keyPrefix, String secmodName)
        {
            this.configDir = configDir;
            this.certPrefix = certPrefix;
            this.keyPrefix = keyPrefix;
            this.secmodName = secmodName;
        }

        public String configDir = null;
        public String certPrefix = null;
        public String keyPrefix = null;
        public String secmodName = null;

        /**
         * The password callback to be used by JSS whenever a password
         * is needed. May be NULL, in which the library will immediately fail
         * to get a password if it tries to login automatically while
         * performing
         * a cryptographic operation.  It will still work if the token
         * has been manually logged in with <code>CryptoToken.login</code>.
         * <p>The default is a <code>ConsolePasswordCallback</code>.
         */
        public PasswordCallback passwordCallback =
            new ConsolePasswordCallback();

        /**
         * The FIPS mode of the security library.  Servers should
         * use <code>FIPSMode.UNCHANGED</code>, since only
         * Admin Server is supposed to alter this value.
         * <p>The default is <code>FIPSMode.UNCHANGED</code>.
         */
        public FIPSMode fipsMode = FIPSMode.UNCHANGED;

        /**
         * To open the databases in read-only mode, set this flag to
         * <code>true</code>.  The default is <code>false</code>, meaning
         * the databases are opened in read-write mode.
         */
        public boolean readOnly = false;

        ////////////////////////////////////////////////////////////////////
        // Manufacturer ID
        ////////////////////////////////////////////////////////////////////
        /**
         * Returns the Manufacturer ID of the internal PKCS #11 module.
         * <p>The default is <code>"mozilla.org                     "</code>.
         */
        public String getManufacturerID() { return manufacturerID; }

        /**
         * Sets the Manufacturer ID of the internal PKCS #11 module.
         * This value must be exactly <code>MANUFACTURER_LENGTH</code>
         * characters long.
         * @exception InvalidLengthException If <code>s.length()</code> is not
         *      exactly <code>MANUFACTURER_LENGTH</code>.
         */
        public void setManufacturerID(String s) throws InvalidLengthException {
            if( s.length() != MANUFACTURER_LENGTH ) {
                throw new InvalidLengthException();
            }
            manufacturerID = s;
        }
        private String manufacturerID =
            "mozilla.org                      ";

        ////////////////////////////////////////////////////////////////////
        // Library Description
        ////////////////////////////////////////////////////////////////////
        /**
         * Returns the description of the internal PKCS #11 module.
         * <p>The default is <code>"Internal Crypto Services         "</code>.
         */
        public String getLibraryDescription() { return libraryDescription; }

        /**
         * Sets the description of the internal PKCS #11 module.
         * This value must be exactly <code>LIBRARY_LENGTH</code>
         *  characters long.
         * @exception InvalidLengthException If <code>s.length()</code> is
         *      not exactly <code>LIBRARY_LENGTH</code>.
         */
        public void setLibraryDescription(String s)
            throws InvalidLengthException
        {
            if( s.length() != LIBRARY_LENGTH ) {
                throw new InvalidLengthException();
            }
            libraryDescription = s;
        }
        private String libraryDescription =
            "Internal Crypto Services         ";

        ////////////////////////////////////////////////////////////////////
        // Internal Token Description
        ////////////////////////////////////////////////////////////////////
        /**
         * Returns the description of the internal PKCS #11 token.
         * <p>The default is <code>"Internal Crypto Services Token   "</code>.
         */
        public String getInternalTokenDescription() {
            return internalTokenDescription;
        }

        /**
         * Sets the description of the internal PKCS #11 token.
         * This value must be exactly <code>TOKEN_LENGTH</code> characters long.
         * @exception InvalidLengthException If <code>s.length()</code> is
         *      not exactly <code>TOKEN_LENGTH</code>.
         */
        public void setInternalTokenDescription(String s)
            throws InvalidLengthException
        {
            if(s.length() != TOKEN_LENGTH) {
                throw new InvalidLengthException();
            }
            internalTokenDescription = s;
        }
        private String internalTokenDescription =
            "Internal Crypto Services Token   ";

        ////////////////////////////////////////////////////////////////////
        // Internal Key Storage Token Description
        ////////////////////////////////////////////////////////////////////
        /**
         * Returns the description of the internal PKCS #11 key storage token.
         * <p>The default is <code>"Internal Key Storage Token       "</code>.
         */
        public String getInternalKeyStorageTokenDescription() {
            return internalKeyStorageTokenDescription;
        }

        /**
         * Sets the description of the internal PKCS #11 key storage token.
         * This value must be exactly <code>TOKEN_LENGTH</code> characters long.
         * @exception InvalidLengthException If <code>s.length()</code> is
         *      not exactly <code>TOKEN_LENGTH</code>.
         */
        public void setInternalKeyStorageTokenDescription(String s)
            throws InvalidLengthException
        {
            if(s.length() != TOKEN_LENGTH) {
                throw new InvalidLengthException();
            }
            internalKeyStorageTokenDescription = s;
        }
        private String internalKeyStorageTokenDescription =
            "Internal Key Storage Token       ";

        ////////////////////////////////////////////////////////////////////
        // Internal Slot Description
        ////////////////////////////////////////////////////////////////////
        /**
         * Returns the description of the internal PKCS #11 slot.
         * <p>The default is <code>"NSS Internal Cryptographic Services                              "</code>.
         */
        public String getInternalSlotDescription() {
            return internalSlotDescription;
        }

        /**
         * Sets the description of the internal PKCS #11 slot.
         * This value must be exactly <code>SLOT_LENGTH</code> characters
         * long.
         * @exception InvalidLengthException If <code>s.length()</code> is
         *      not exactly <code>SLOT_LENGTH</code>.
         */
        public void setInternalSlotDescription(String s)
            throws InvalidLengthException
        {
            if(s.length() != SLOT_LENGTH)  {
                throw new InvalidLengthException();
            }
            internalSlotDescription = s;
        }
        private String internalSlotDescription =
            "NSS Internal Cryptographic Services                              ";

        ////////////////////////////////////////////////////////////////////
        // Internal Key Storage Slot Description
        ////////////////////////////////////////////////////////////////////
        /**
         * Returns the description of the internal PKCS #11 key storage slot.
         * <p>The default is <code>"NSS Internal Private Key and Certificate Storage                 "</code>.

         */
        public String getInternalKeyStorageSlotDescription() {
            return internalKeyStorageSlotDescription;
        }

        /**
         * Sets the description of the internal PKCS #11 key storage slot.
         * This value must be exactly <code>SLOT_LENGTH</code> characters
         * long.
         * @exception InvalidLengthException If <code>s.length()</code> is
         *      not exactly <code>SLOT_LENGTH</code>.
         */
        public void setInternalKeyStorageSlotDescription(String s)
            throws InvalidLengthException
        {
            if(s.length() != SLOT_LENGTH) {
                throw new InvalidLengthException();
            }
            internalKeyStorageSlotDescription = s;
        }
        private String internalKeyStorageSlotDescription =
            "NSS Internal Private Key and Certificate Storage                 ";

        ////////////////////////////////////////////////////////////////////
        // FIPS Slot Description
        ////////////////////////////////////////////////////////////////////
        /**
         * Returns the description of the internal PKCS #11 FIPS slot.
         * <p>The default is <code>"NSS Internal FIPS-140-1 Cryptographic Services                   "</code>.
         */
        public String getFIPSSlotDescription() {
            return FIPSSlotDescription;
        }

        /**
         * Sets the description of the internal PKCS #11 FIPS slot.
         * This value must be exactly <code>SLOT_LENGTH</code> characters
         * long.
         * @exception InvalidLengthException If <code>s.length()</code> is
         *      not exactly <code>SLOT_LENGTH</code>.
         */
        public void setFIPSSlotDescription(String s)
            throws InvalidLengthException
        {
            if(s.length() != SLOT_LENGTH) {
                throw new InvalidLengthException();
            }
            FIPSSlotDescription = s;
        }
        private String FIPSSlotDescription =
            "NSS Internal FIPS-140-1 Cryptographic Services                   ";

        ////////////////////////////////////////////////////////////////////
        // FIPS Key Storage Slot Description
        ////////////////////////////////////////////////////////////////////
        /**
         * Returns the description of the internal PKCS #11 FIPS
         * Key Storage slot.
         * <p>The default is <code>"NSS Internal FIPS-140-1 Private Key and Certificate Storage      "</code>.
         */
        public String getFIPSKeyStorageSlotDescription() {
            return FIPSKeyStorageSlotDescription;
        }

        /**
         * Sets the description of the internal PKCS #11 FIPS Key Storage slot.
         * This value must be exactly <code>SLOT_LENGTH</code> characters
         * long.
         * @exception InvalidLengthException If <code>s.length()</code> is
         *      not exactly <code>SLOT_LENGTH</code>.
         */
        public void setFIPSKeyStorageSlotDescription(String s)
            throws InvalidLengthException
        {
            if(s.length() != SLOT_LENGTH) {
                throw new InvalidLengthException();
            }
            FIPSKeyStorageSlotDescription = s;
        }
        private String FIPSKeyStorageSlotDescription =
            "NSS Internal FIPS-140-1 Private Key and Certificate Storage      ";

        /**
         * To have NSS check the OCSP responder for when verifying
         * certificates, set this flags to true. It is false by
         * default.
         */
        public boolean ocspCheckingEnabled = false;

        /**
         * Specify the location and cert of the responder.
         * If OCSP checking is enabled *and* this variable is
         * set to some URL, all OCSP checking will be done via
         * this URL.
         *
         * If this variable is null, the OCSP responder URL will
         * be obtained from the AIA extension in the certificate
         * being queried.
         *
         * If this is set, you must also set ocspResponderCertNickname
         *
         */
        public String ocspResponderURL = null;

        /**
         * The nickname of the cert to trust (expected) to
         * sign the OCSP responses.
         * Only checked when the OCSPResponder value is set.
         */
        public String ocspResponderCertNickname = null;


        /**
         * Install the JSS crypto provider. Default is true.
         */
        public boolean installJSSProvider = true;

        /**
         * Remove the Sun crypto provider. Default is false.
         */
        public boolean removeSunProvider = false;

        /**
         * If <tt>true</tt>, none of the underlying NSS components will
         * be initialized. Only the Java portions of JSS will be
         * initialized. This should only be used if NSS has been initialized
         * elsewhere.
         *
         * <p>Specifically, the following components will <b>not</b> be
         *  configured by <tt>CryptoManager.initialize</tt> if this flag is set:
         * <ul>
         * <li>The NSS databases.
         * <li>OCSP checking.
         * <li>The NSS password callback.
         * <li>The internal PKCS #11 software token's identifier labels:
         *      slot, token, module, and manufacturer.
         * <li>The minimum PIN length for the software token.
         * <li>The frequency with which the user must login to the software
         *      token.
         * <li>The cipher strength policy (export/domestic).
         * </ul>
         *
         * <p>The default is <tt>false</tt>.
         */
        public boolean initializeJavaOnly = false;
    }

    ////////////////////////////////////////////////////
    //  Module and Token Management
    ////////////////////////////////////////////////////

    /**
     * Retrieves the internal cryptographic services token. This is the
     * token built into NSS that performs bulk
     * cryptographic operations.
     * <p>In FIPS mode, the internal cryptographic services token is the
     * same as the internal key storage token.
     *
     * @return The internal cryptographic services token.
     */
    public synchronized CryptoToken getInternalCryptoToken() {
        return internalCryptoToken;
    }

    /**
     * Retrieves the internal key storage token.  This is the token
     * provided by NSS to store private keys.
     * The keys stored in this token are stored in an encrypted key database.
     * <p>In FIPS mode, the internal key storage token is the same as
     * the internal cryptographic services token.
     *
     * @return The internal key storage token.
     */
    public synchronized CryptoToken getInternalKeyStorageToken() {
        return internalKeyStorageToken;
    }

    /**
     * Looks up the CryptoToken with the given name.  Searches all
     * loaded cryptographic modules for the token.
     *
     * @param name The name of the token.
     * @exception org.mozilla.jss.crypto.NoSuchTokenException If no token
     *  is found with the given name.
     */
    public synchronized CryptoToken getTokenByName(String name)
        throws NoSuchTokenException
    {
        Enumeration tokens = getAllTokens();
        CryptoToken token;

        while(tokens.hasMoreElements()) {
            token = (CryptoToken) tokens.nextElement();
            try {
                if( name.equals(token.getName()) ) {
                    return token;
                }
            } catch( TokenException e ) {
                Assert._assert(false, "Got a token exception");
            }
        }
        throw new NoSuchTokenException();
    }

    /**
     * Retrieves all tokens that support the given algorithm.
     *
     */
    public synchronized Enumeration getTokensSupportingAlgorithm(Algorithm alg)
    {
        Enumeration tokens = getAllTokens();
        Vector goodTokens = new Vector();
        CryptoToken tok;

        while(tokens.hasMoreElements()) {
            tok = (CryptoToken) tokens.nextElement();
            if( tok.doesAlgorithm(alg) ) {
                goodTokens.addElement(tok);
            }
        }
        return goodTokens.elements();
    }

    /**
     * Retrieves all tokens. This is an enumeration of all tokens on all
     * modules.
     *
     * @return All tokens accessible from JSS. Each item of the enumeration
     *      is a <code>CryptoToken</code>
     * @see org.mozilla.jss.crypto.CryptoToken
     */
    public synchronized Enumeration getAllTokens() {
        Enumeration modules = getModules();
        Enumeration tokens;
        Vector allTokens = new Vector();

        while(modules.hasMoreElements()) {
            tokens = ((PK11Module)modules.nextElement()).getTokens();
            while(tokens.hasMoreElements()) {
                allTokens.addElement( tokens.nextElement() );
            }
        }
        return allTokens.elements();
    }

    /**
     * Retrieves all tokens except those built into NSS.
     * This excludes the internal token and the internal
     * key storage token (which are one and the same in FIPS mode).
     *
     * @return All tokens accessible from JSS, except for the built-in
     *      internal tokens.
     */
    public synchronized Enumeration getExternalTokens() {
        Enumeration modules = getModules();
        Enumeration tokens;
        PK11Token token;
        Vector allTokens = new Vector();

        while(modules.hasMoreElements()) {
            tokens = ((PK11Module)modules.nextElement()).getTokens();
            while(tokens.hasMoreElements()) {
                token = (PK11Token) tokens.nextElement();
                if( ! token.isInternalCryptoToken() &&
                    ! token.isInternalKeyStorageToken() )
                {
                    allTokens.addElement( token );
                }
            }
        }
        return allTokens.elements();
    }

    /**
     * Retrieves all installed cryptographic modules.
     *
     * @return An enumeration of all installed PKCS #11 modules. Each
     *      item in the enumeration is a <code>PK11Module</code>.
     * @see org.mozilla.jss.pkcs11.PK11Module
     */
    public synchronized Enumeration getModules() {
        return moduleVector.elements();
    }

    // Need to reload modules after adding new one
    //public native addModule(String name, String libraryName);

    /**
     * The list of modules. This should be initialized by the constructor
     * and updated whenever 1) a new module is added, 2) a module is deleted,
     * or 3) FIPS mode is switched.
     */
    private Vector moduleVector;

    /**
     * Re-creates the Vector of modules that is stored by CryptoManager.
     * This entails going into native code to enumerate all modules,
     * wrap each one in a PK11Module, and storing the PK11Module in the vector.
     */
    private synchronized void reloadModules() {
        moduleVector = new Vector();
        putModulesInVector(moduleVector);

        // Get the internal tokens
        Enumeration tokens = getAllTokens();

        internalCryptoToken = null;
        internalKeyStorageToken = null;
        while(tokens.hasMoreElements()) {
            PK11Token token = (PK11Token) tokens.nextElement();
            if( token.isInternalCryptoToken() ) {
                Assert._assert(internalCryptoToken == null);
                internalCryptoToken = token;
            }
            if( token.isInternalKeyStorageToken() ) {
                Assert._assert(internalKeyStorageToken == null);
                internalKeyStorageToken = token;
            }
        }
        Assert._assert(internalKeyStorageToken != null);
        Assert._assert(internalCryptoToken != null);
    }

    /**
     * The internal cryptographic services token.
     */
    private CryptoToken internalCryptoToken;

    /**
     * The internal key storage token.
     */
    private CryptoToken internalKeyStorageToken;

    /**
     * Native code to traverse all PKCS #11 modules, wrap each one in
     * a PK11Module, and insert each PK11Module into the given vector.
     */
    private native void putModulesInVector(Vector vector);


    ///////////////////////////////////////////////////////////////////////
    // Constructor and Accessors
    ///////////////////////////////////////////////////////////////////////

    /**
     * Constructor, for internal use only.
     */
    protected CryptoManager()  {
        TokenSupplierManager.setTokenSupplier(this);
        reloadModules();
    }

    /**
     * Retrieve the single instance of CryptoManager.
     * This cannot be called before initialization.
     *
     * @see #initialize(CryptoManager.InitializationValues)
     * @exception NotInitializedException If
     *      <code>initialize(InitializationValues</code> has not yet been
     *      called.
     */
    public synchronized static CryptoManager getInstance()
        throws NotInitializedException
    {
        if(instance==null) {
            throw new NotInitializedException();
        }
        return instance;
    }

    /**
     * The singleton instance, and a static initializer to create it.
     */
    private static CryptoManager instance=null;


    ///////////////////////////////////////////////////////////////////////
    // FIPS management
    ///////////////////////////////////////////////////////////////////////

    /**
     * Enables or disables FIPS-140-1 compliant mode. If this returns true,
     * you must reloadModules(). This should only be called once in a program,
     * at the beginning, because it invalidates tokens and modules.
     *
     * @param fips true to turn FIPS compliant mode on, false to turn it off.
     */
    private static native boolean enableFIPS(boolean fips)
        throws GeneralSecurityException;

    /**
     * Determines whether FIPS-140-1 compliance is active.
     *
     * @return true if the security library is in FIPS-140-1 compliant mode.
     */
    public synchronized native boolean FIPSEnabled();


    ///////////////////////////////////////////////////////////////////////
    // Password Callback management
    ///////////////////////////////////////////////////////////////////////

    /**
     * This function sets the global password callback.  It is
     * not thread-safe to change this.
     * <p>The callback may be NULL, in which case password callbacks will
     * fail gracefully.
     */
    public synchronized void setPasswordCallback(PasswordCallback pwcb) {
        passwordCallback = pwcb;
        setNativePasswordCallback( pwcb );
    }
    private native void setNativePasswordCallback(PasswordCallback cb);

    /**
     * Returns the currently registered password callback.
     */
    public synchronized PasswordCallback getPasswordCallback() {
        return passwordCallback;
    }

    private PasswordCallback passwordCallback;


    ////////////////////////////////////////////////////
    // Initialization
    ////////////////////////////////////////////////////

    /**
     * Initialize the security subsystem.  Opens the databases, loads all
     * PKCS #11 modules, initializes the internal random number generator.
     * The <code>initialize</code> methods that take arguments should be
     * called only once, otherwise they will throw
     * an exception. It is OK to call them after calling
     * <code>initialize()</code>.
     *
     * @param configDir The directory containing the security databases.
     * @exception org.mozilla.jss.util.KeyDatabaseException Unable to open
     *  the key database, or it was currupted.
     * @exception org.mozilla.jss.util.CertDatabaseException Unable
     *  to open the certificate database, or it was currupted.
     **/
    public static synchronized void initialize( String configDir )
        throws  KeyDatabaseException,
                CertDatabaseException,
                AlreadyInitializedException,
                GeneralSecurityException
    {
        initialize( new InitializationValues(configDir) );
    }

    /**
     * Initialize the security subsystem.  Opens the databases, loads all
     * PKCS #11 modules, initializes the internal random number generator.
     * The <code>initialize</code> methods that take arguments should be
     * called only once, otherwise they will throw
     * an exception. It is OK to call them after calling
     * <code>initialize()</code>.
     *
     * @param values The options with which to initialize CryptoManager.
     * @exception org.mozilla.jss.util.KeyDatabaseException Unable to open
     *  the key database, or it was currupted.
     * @exception org.mozilla.jss.util.CertDatabaseException Unable
     *  to open the certificate database, or it was currupted.
     **/
    public static synchronized void initialize( InitializationValues values )
        throws
        KeyDatabaseException,
        CertDatabaseException,
        AlreadyInitializedException,
        GeneralSecurityException
    {
        if(instance != null) {
            throw new AlreadyInitializedException();
        }
        loadNativeLibraries();
        if (values.ocspResponderURL != null) {
            if (values.ocspResponderCertNickname == null) {
                throw new GeneralSecurityException(
                    "Must set ocspResponderCertNickname");
            }
        }
        initializeAllNative2(values.configDir,
                            values.certPrefix,
                            values.keyPrefix,
                            values.secmodName,
                            values.readOnly,
                            values.getManufacturerID(),
                            values.getLibraryDescription(),
                            values.getInternalTokenDescription(),
                            values.getInternalKeyStorageTokenDescription(),
                            values.getInternalSlotDescription(),
                            values.getInternalKeyStorageSlotDescription(),
                            values.getFIPSSlotDescription(),
                            values.getFIPSKeyStorageSlotDescription(),
                            values.ocspCheckingEnabled,
                            values.ocspResponderURL,
                            values.ocspResponderCertNickname,
                            values.initializeJavaOnly
                            );

        instance = new CryptoManager();
        instance.setPasswordCallback(values.passwordCallback);
        if( values.fipsMode != InitializationValues.FIPSMode.UNCHANGED) {
            if( enableFIPS(values.fipsMode ==
                    InitializationValues.FIPSMode.ENABLED) )
            {
                instance.reloadModules();
            }
        }

        // Force class load before we install the provider. Otherwise we get
        // an infinite loop as the Security manager tries to instantiate the
        // digest to verify its own JAR file.
        JSSMessageDigestSpi mds = new JSSMessageDigestSpi.SHA1();
        // Force the KeyType class to load before we can install JSS as a
        // provider.  JSS's signature provider accesses KeyType.
        KeyType kt = KeyType.getKeyTypeFromAlgorithm(
            SignatureAlgorithm.RSASignatureWithSHA1Digest);

        if( values.installJSSProvider ) {
            int position = java.security.Security.insertProviderAt(
                            new JSSProvider(), 1);
            // This returns -1 if the provider was already installed, in which
            // case it is not installed again.  Is this
            // an error? I don't think so, although it might be confusing
            // if the provider is not in the position they expected.
            // However, this will only happen if they are installing the
            // provider themselves, so presumably they know what they're
            // doing.
        }
        if( values.removeSunProvider ) {
            java.security.Security.removeProvider("SUN");
        }
    }

    private static native void
    initializeAllNative2(String configDir,
                        String certPrefix,
                        String keyPrefix,
                        String secmodName,
                        boolean readOnly,
                        String manufacturerID,
                        String libraryDescription,
                        String internalTokenDescription,
                        String internalKeyStorageTokenDescription,
                        String internalSlotDescription,
                        String internalKeyStorageSlotDescription,
                        String fipsSlotDescription,
                        String fipsKeyStorageSlotDescription,
                        boolean ocspCheckingEnabled,
                        String ocspResponderURL,
                        String ocspResponderCertNickname,
                        boolean initializeJavaOnly
)
        throws KeyDatabaseException,
        CertDatabaseException,
        AlreadyInitializedException;

    /////////////////////////////////////////////////////////////
    // Cert Lookup
    /////////////////////////////////////////////////////////////
    /**
     * Retrieves all CA certificates in the trust database.  This
     * is a fairly expensive operation in that it involves traversing
     * the entire certificate database.
     * @return An array of all CA certificates stored permanently
     *      in the trust database.
     */
    public native X509Certificate[]
    getCACerts();

    /**
     * Retrieves all certificates in the trust database.  This
     * is a fairly expensive operation in that it involves traversing
     * the entire certificate database.
     * @return An array of all certificates stored permanently
     *      in the trust database.
     */
    public native X509Certificate[]
    getPermCerts();

    /**
     * Imports a chain of certificates.  The leaf certificate may be a
     *  a user certificate, that is, a certificate that belongs to the
     *  current user and whose private key is available for use.
     *  If the leaf certificate is a user certificate, it is stored
     *  on the token
     *  that contains the corresponding private key, and is assigned the
     *  given nickname.
     *
     * @param certPackage An encoded certificate or certificate chain.
     *      Acceptable
     *      encodings are binary PKCS #7 <i>SignedData</i> objects and
     *      DER-encoded certificates, which may or may not be wrapped
     *      in a Base-64 encoding package surrounded by
     *      "<code>-----BEGIN CERTIFICATE-----</code>" and
     *      "<code>-----END CERTIFICATE-----</code>".
     * @param nickname The nickname for the user certificate.  It must
     *      be unique. It is ignored if there is no user certificate.
     * @return The leaf certificate from the chain.
     * @exception CertificateEncodingException If the package encoding
     *      was not recognized.
     * @exception CertificateNicknameConflictException If the leaf certificate
     *      is a user certificate, and another certificate already has the
     *      given nickname.
     * @exception UserCertConflictException If the leaf certificate
     *      is a user certificate, but it has already been imported.
     * @exception NoSuchItemOnTokenException If the leaf certificate is
     *      a user certificate, but the matching private key cannot be found.
     * @exception TokenException If an error occurs importing a leaf
     *      certificate into a token.
     */
    public X509Certificate
    importCertPackage(byte[] certPackage, String nickname )
        throws CertificateEncodingException,
            NicknameConflictException,
            UserCertConflictException,
            NoSuchItemOnTokenException,
            TokenException
    {
        return importCertPackageNative(certPackage, nickname, false, false);
    }

    /**
     * Imports a chain of certificates.  The leaf of the chain is a CA
     * certificate AND a user certificate (this would only be called by
     * a CA installing its own certificate).
     *
     * @param certPackage An encoded certificate or certificate chain.
     *      Acceptable
     *      encodings are binary PKCS #7 <i>SignedData</i> objects and
     *      DER-encoded certificates, which may or may not be wrapped
     *      in a Base-64 encoding package surrounded by
     *      "<code>-----BEGIN CERTIFICATE-----</code>" and
     *      "<code>-----END CERTIFICATE-----</code>".
     * @param nickname The nickname for the user certificate.  It must
     *      be unique.
     * @return The leaf certificate from the chain.
     * @exception CertificateEncodingException If the package encoding
     *      was not recognized.
     * @exception CertificateNicknameConflictException If the leaf certificate
     *      another certificate already has the given nickname.
     * @exception UserCertConflictException If the leaf certificate
     *      has already been imported.
     * @exception NoSuchItemOnTokenException If the the private key matching
     *      the leaf certificate cannot be found.
     * @exception TokenException If an error occurs importing the leaf
     *      certificate into a token.
     */
    public X509Certificate
    importUserCACertPackage(byte[] certPackage, String nickname)
        throws CertificateEncodingException,
            NicknameConflictException,
            UserCertConflictException,
            NoSuchItemOnTokenException,
            TokenException
    {
        return importCertPackageNative(certPackage, nickname, false, true);
    }



    /**
     * Imports a chain of certificates, none of which is a user certificate.
     *
     * @param certPackage An encoded certificate or certificate chain.
     *      Acceptable
     *      encodings are binary PKCS #7 <i>SignedData</i> objects and
     *      DER-encoded certificates, which may or may not be wrapped
     *      in a Base-64 encoding package surrounded by
     *      "<code>-----BEGIN CERTIFICATE-----</code>" and
     *      "<code>-----END CERTIFICATE-----</code>".
     * @return The leaf certificate from the chain.
     * @exception CertificateEncodingException If the package encoding
     *      was not recognized.
     * @exception TokenException If an error occurs importing a leaf
     *      certificate into a token.
     */
    public X509Certificate
    importCACertPackage(byte[] certPackage)
        throws CertificateEncodingException,
            TokenException
    {
        try {
            return importCertPackageNative(certPackage, null, true, false);
        } catch(NicknameConflictException e) {
            Assert.notReached("importing CA certs caused nickname conflict");
            Debug.trace(Debug.ERROR,
                "importing CA certs caused nickname conflict");
        } catch(UserCertConflictException e) {
            Assert.notReached("importing CA certs caused user cert conflict");
            Debug.trace(Debug.ERROR,
                "importing CA certs caused user cert conflict");
        } catch(NoSuchItemOnTokenException e) {
            Assert.notReached("importing CA certs caused NoSuchItemOnToken"+
                "Exception");
            Debug.trace(Debug.ERROR,
                "importing CA certs caused NoSuchItemOnTokenException");
        }
        return null;
    }

    /**
     * Imports a single certificate into the permanent certificate
     * database.
     *
     * @param derCert the certificate you want to add
     * @param nickname the nickname you want to refer to the certificate as
     *        (must not be null)
     */

    public InternalCertificate
        importCertToPerm(X509Certificate cert, String nickname)
        throws TokenException, InvalidNicknameException
    {
        if (nickname==null) {
            throw new InvalidNicknameException("Nickname must be non-null");
        }

        else {
            return importCertToPermNative(cert,nickname);
        }
    }

    private native InternalCertificate
        importCertToPermNative(X509Certificate cert, String nickname)
        throws TokenException;

    /**
     * @param noUser true if we know that none of the certs are user certs.
     *      In this case, no attempt will be made to find a matching private
     *      key for the leaf certificate.
     */
    private native X509Certificate
    importCertPackageNative(byte[] certPackage, String nickname,
        boolean noUser, boolean leafIsCA)
        throws CertificateEncodingException,
            NicknameConflictException,
            UserCertConflictException,
            NoSuchItemOnTokenException,
            TokenException;

    /*============ CRL importing stuff ********************************/

    private static int TYPE_KRL = 0;
    private static int TYPE_CRL = 1;
    /**
     * Imports a CRL, and stores it into the cert7.db
     * Validate CRL then import it to the dbase.  If there is already a CRL with the
      * same CA in the dbase, it will be replaced if derCRL is more up to date.
     *
     * @param crl the DER-encoded CRL.
     * @param url the URL where this CRL can be retrieved from (for future updates).
     *    [ note that CRLs are not retrieved automatically ]. Can be null
     * @exception CRLImportException If the package encoding
     *      was not recognized.
     */
     public void
    importCRL(byte[] crl,String url)
        throws CRLImportException,
            TokenException
    {
        importCRLNative(crl,url,TYPE_CRL);
    }


    /**
     * Imports a CRL, and stores it into the cert7.db
     *
     * @param the DER-encoded CRL.
     */
    private native
    void importCRLNative(byte[] crl, String url, int rl_type)
        throws CRLImportException, TokenException;



    /*============ Cert Exporting stuff ********************************/


    /**
     * Exports one or more certificates into a PKCS #7 certificate container.
     * This is just a <i>SignedData</i> object whose <i>certificates</i>
     * field contains the given certificates but whose <i>content</i> field
     * is empty.
     *
     * @param certs One or more certificates that should be exported into
     *      the PKCS #7 object.  The leaf certificate should be the first
     *      in the chain.  The output of <code>buildCertificateChain</code>
     *      would be appropriate here.
     * @exception CertificateEncodingException If the array is empty,
     *        or an error occurred encoding the certificates.
     * @return A byte array containing a PKCS #7 <i>SignedData</i> object.
     * @see #buildCertificateChain
     */
    public native byte[]
    exportCertsToPKCS7(X509Certificate[] certs)
        throws CertificateEncodingException;

    /**
     * Looks up a certificate given its nickname.
     *
     * @param nickname The nickname of the certificate to look for.
     * @return The certificate matching this nickname, if one is found.
     * @exception ObjectNotFoundException If no certificate could be found
     *      with the given nickname.
     * @exception TokenException If an error occurs in the security library.
     */
    public org.mozilla.jss.crypto.X509Certificate
    findCertByNickname(String nickname)
        throws ObjectNotFoundException, TokenException
    {
        Assert._assert(nickname!=null);
        return findCertByNicknameNative(nickname);
    }

    /**
     * Returns all certificates with the given nickname.
     *
     * @param nickname The nickname of the certificate to look for.
     * @return The certificates matching this nickname. The array may be empty
     *      if no matching certs were found.
     * @exception TokenException If an error occurs in the security library.
     */
    public org.mozilla.jss.crypto.X509Certificate[]
    findCertsByNickname(String nickname)
        throws TokenException
    {
        Assert._assert(nickname!=null);
        return findCertsByNicknameNative(nickname);
    }

    /**
     * Looks up a certificate by issuer and serial number. The internal
     *      database and all PKCS #11 modules are searched.
     *
     * @param derIssuer The DER encoding of the certificate issuer name.
     *      The issuer name has ASN.1 type <i>Name</i>, which is defined in
     *      X.501.
     * @param serialNumber The certificate serial number.
     * @exception ObjectNotFoundException If the certificate is not found
     *      in the internal certificate database or on any PKCS #11 token.
     * @exception TokenException If an error occurs in the security library.
     */
    public org.mozilla.jss.crypto.X509Certificate
    findCertByIssuerAndSerialNumber(byte[] derIssuer, INTEGER serialNumber)
        throws ObjectNotFoundException, TokenException
    {
      try {
        ANY sn = (ANY) ASN1Util.decode(ANY.getTemplate(),
                                 ASN1Util.encode(serialNumber) );
        return findCertByIssuerAndSerialNumberNative(derIssuer,
            sn.getContents() );
      } catch( InvalidBERException e ) {
        Assert.notReached("Invalid BER encoding of INTEGER");
        return null;
      }
    }

    /**
     * @param serialNumber The contents octets of a DER-encoding of the
     *  certificate serial number.
     */
    private native org.mozilla.jss.crypto.X509Certificate
    findCertByIssuerAndSerialNumberNative(byte[] derIssuer, byte[] serialNumber)
        throws ObjectNotFoundException, TokenException;

    protected native org.mozilla.jss.crypto.X509Certificate
    findCertByNicknameNative(String nickname)
        throws ObjectNotFoundException, TokenException;

    protected native org.mozilla.jss.crypto.X509Certificate[]
    findCertsByNicknameNative(String nickname)
        throws TokenException;

    /////////////////////////////////////////////////////////////
    // build cert chains
    /////////////////////////////////////////////////////////////
    /**
     * Given a certificate, constructs its certificate chain. It may
     * or may not chain up to a trusted root.
     * @param leaf The certificate that is the starting point of the chain.
     * @return An array of certificates, starting at the leaf and ending
     *      with the highest certificate on the chain that was found.
     * @throws CertificateException If the certificate is not recognized
     *      by the underlying provider.
     */
    public org.mozilla.jss.crypto.X509Certificate[]
    buildCertificateChain(org.mozilla.jss.crypto.X509Certificate leaf)
        throws java.security.cert.CertificateException, TokenException
    {
        if( ! (leaf instanceof PK11Cert) ) {
            throw new CertificateException(
                        "Certificate is not a PKCS #11 certificate");
        }
        return buildCertificateChainNative((PK11Cert)leaf);
    }

    native org.mozilla.jss.crypto.X509Certificate[]
    buildCertificateChainNative(PK11Cert leaf)
        throws CertificateException, TokenException;


    /////////////////////////////////////////////////////////////
    // lookup private keys
    /////////////////////////////////////////////////////////////
    /**
     * Looks up the PrivateKey matching the given certificate.
     *
     * @exception ObjectNotFoundException If no private key can be
     *      found matching the given certificate.
     * @exception TokenException If an error occurs in the security library.
     */
    public org.mozilla.jss.crypto.PrivateKey
    findPrivKeyByCert(org.mozilla.jss.crypto.X509Certificate cert)
        throws ObjectNotFoundException, TokenException
    {
        Assert._assert(cert!=null);
        if(! (cert instanceof org.mozilla.jss.pkcs11.PK11Cert)) {
            Assert.notReached("non-pkcs11 cert passed to PK11Finder");
            throw new ObjectNotFoundException();
        }
        return findPrivKeyByCertNative(cert);
    }

    protected native org.mozilla.jss.crypto.PrivateKey
    findPrivKeyByCertNative(org.mozilla.jss.crypto.X509Certificate cert)
        throws ObjectNotFoundException, TokenException;

    /////////////////////////////////////////////////////////////
    // Provide Pseudo-Random Number Generation
    /////////////////////////////////////////////////////////////

    /**
     * Retrieves a FIPS-140-1 validated random number generator.
     *
     * @return A JSS SecureRandom implemented with FIPS-validated NSS.
     */
    public org.mozilla.jss.crypto.JSSSecureRandom
    createPseudoRandomNumberGenerator()
    {
        return new PK11SecureRandom();
    }

    /**
     * Retrieves a FIPS-140-1 validated random number generator.
     *
     * @return A JSS SecureRandom implemented with FIPS-validated NSS.
     */
    public org.mozilla.jss.crypto.JSSSecureRandom
    getSecureRNG() {
        return new PK11SecureRandom();
    }

    /********************************************************************/
    /* The following VERSION Strings should be updated in the following */
    /* files everytime a new release of JSS is generated:               */
    /*                                                                  */
    /*     jss.jar:  ns/ninja/org/mozilla/jss/manage/CryptoManager.java */
    /*     jss.dll:  ns/ninja/org/mozilla/jss/manage/CryptoManager.c    */
    /*                                                                  */
    /********************************************************************/

    public static final String
    JAR_JSS_VERSION     = "JSS_VERSION = JSS_4_0_BETA";
    public static final String
    JAR_JDK_VERSION     = "JDK_VERSION = N/A";
    public static final String
    JAR_NSS_VERSION     = "NSS_VERSION = NSS_3_9_RTM";
    public static final String
    JAR_DBM_VERSION     = "DBM_VERSION = N/A";
    public static final String
    JAR_NSPR_VERSION    = "NSPR_VERSION = NSPR_4_5_RTM";

    /**
     * Loads the JSS dynamic library if necessary.
     * <p>This method is idempotent.
     */
    synchronized static void loadNativeLibraries()
    {
        if( ! mNativeLibrariesLoaded )
        {
            System.loadLibrary("jss3");
            Debug.trace(Debug.VERBOSE, "jss library loaded");
            mNativeLibrariesLoaded = true;
        }
    }
    static private boolean mNativeLibrariesLoaded = false;

    // Hashtable is synchronized.
    private Hashtable perThreadTokenTable = new Hashtable();

    /**
     * Sets the default token for the current thread. This token will
     * be used when JSS is called through the JCA interface, which has
     * no means of specifying which token to use.
     *
     * <p>If no token is set, the InternalKeyStorageToken will be used. Setting
     * this thread's token to <tt>null</tt> will also cause the
     * InternalKeyStorageToken to be used.
     *
     * @param The token to use for crypto operations. Specifying <tt>null</tt>
     * will cause the InternalKeyStorageToken to be used.
     */
    public void setThreadToken(CryptoToken token) {
        if( token != null ) {
            perThreadTokenTable.put(Thread.currentThread(), token);
        } else {
            perThreadTokenTable.remove(Thread.currentThread());
        }
    }

    /**
     * Returns the default token for the current thread. This token will
     * be used when JSS is called through the JCA interface, which has
     * no means of specifying which token to use.
     *
     * <p>If no token is set, the InternalKeyStorageToken will be used. Setting
     * this thread's token to <tt>null</tt> will also cause the
     * InternalKeyStorageToken to be used.
     *
     * @return The default token for this thread. If it has not been specified,
     * it will be the InternalKeyStorageToken.
     */
    public CryptoToken getThreadToken() {
        CryptoToken tok =
            (CryptoToken) perThreadTokenTable.get(Thread.currentThread());
        if( tok == null ) {
            tok = getInternalKeyStorageToken();
        }
        return tok;
    }
    /////////////////////////////////////////////////////////////
    // isCertValid
    /////////////////////////////////////////////////////////////
    /**
     * Verify a certificate that exists in the given cert database,
     * check if is valid and that we trust the issuer. Verify time
     * against Now.
     * @param nickname The nickname of the certificate to verify.
     * @param checkSig verify the signature of the certificate
     * @param certUsage see exposed certUsage defines to verify Certificate
     * @return true for success; false otherwise
     *
     * @exception InvalidNicknameException If the nickname is null
     * @exception ObjectNotFoundException If no certificate could be found
     *      with the given nickname.
     */

    public boolean isCertValid(String nickname, boolean checkSig,
            CertUsage certUsage)
        throws ObjectNotFoundException, InvalidNicknameException
    {
        if (nickname==null) {
            throw new InvalidNicknameException("Nickname must be non-null");
        }
        return verifyCertNowNative(nickname, checkSig, certUsage.getUsage());
    }

    private native boolean verifyCertNowNative(String nickname,
        boolean checkSig, int cUsage) throws ObjectNotFoundException;

    /////////////////////////////////////////////////////////////
    // isCertValid
    /////////////////////////////////////////////////////////////
    /**
     * Verify a certificate in memory. Check if
     * valid and that we trust the issuer. Verify time
     * against Now.
     * @param certificate in memory
     * @param checkSig verify the signature of the certificate
     * @param certUsage see exposed certUsage defines to verify Certificate
     * @return true for success; false otherwise
     *
     * @exception TokenException unable to insert temporary certificate
     *            into database.
     * @exception CertificateEncodingException If the package encoding
     *      was not recognized.
     */

    public boolean isCertValid(byte[] certPackage, boolean checkSig,
            CertUsage certUsage)
        throws TokenException, CertificateEncodingException
    {
        return verifyCertTempNative(certPackage , checkSig,
                                    certUsage.getUsage());
    }


    private native boolean verifyCertTempNative(byte[] certPackage,
        boolean checkSig, int cUsage)
        throws TokenException, CertificateEncodingException;

     ///////////////////////////////////////////////////////////////////////
    // OCSP management
    ///////////////////////////////////////////////////////////////////////

    /**
     * Enables OCSP, note when you Initialize JSS for the first time, for
     * backwards compatibility, the initialize will enable OCSP if you
     * previously set values.ocspCheckingEnabled and
     * values.ocspResponderURL/values.ocspResponderCertNickname
     * configureOCSP will allow changing of the the OCSPResponder at runtime.
     *      * @param ocspChecking true or false to enable/disable OCSP
     *      * @param ocspResponderURL - url of the OCSP responder
     *      * @param ocspResponderCertNickname - the nickname of the OCSP
     *        signer certificate or the CA certificate found in the cert DB
     */

    public void configureOCSP(
        boolean ocspCheckingEnabled,
        String ocspResponderURL,
        String ocspResponderCertNickname )
    throws GeneralSecurityException
    {
        configureOCSPNative(ocspCheckingEnabled,
                                   ocspResponderURL,
                                    ocspResponderCertNickname );
    }

    private native void configureOCSPNative( boolean ocspCheckingEnabled,
                    String ocspResponderURL,
                    String ocspResponderCertNickname )
                    throws GeneralSecurityException;

}
