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
package com.netscape.jss.pkcs12;

import java.io.*;
import java.text.*;
import java.util.*;
import com.netscape.jss.crypto.*;
import com.netscape.jss.pkcs11.*;
import com.netscape.jss.util.*;

/**
 * A PKCS #12 "virtual token".  Currently, these extend
 * tokens found in the PK11Token class.
 *
 * @author mharmsen
 * @version $Revision: 1.1 $ $Date: 2000/12/15 20:50:34 $ 
 * @see com.netscape.jss.pkcs12.PK12Token
 **/

public
class PK12Token
extends PK11Token
{
    //////////////////////////////////////////////////////////////////////////
    //  Inner "Member" Classes
    //////////////////////////////////////////////////////////////////////////

    protected abstract
    class PK12Store
    implements CryptoStore
    {
        //////////////////////////////////////////////////////////////////////
        //  Inner "Member" Public Methods
        //////////////////////////////////////////////////////////////////////

        public X509Certificate
        getCertByNickname( String nickname )
        throws ObjectNotFoundException, NotImplementedException,
               TokenException
        {
            throw new
            NotImplementedException( "Method \""                            +
                                     "getCertByNickname( String nickname )" +
                                     "\" is not implemented!" );
        }
 

        public X509Certificate
        getCertByDER( byte[] derCert )
        throws ObjectNotFoundException, NotImplementedException,
               InvalidDERException, TokenException
        {
            throw new
            NotImplementedException( "Method \""                      +
                                     "getCertByDER( byte[] derCert )" +
                                     "\" is not implemented!" );
        }
 

        public X509Certificate
        getCertByIssuerAndSerialNum( byte[] derIssuer, String issuer,
                                     long serialNumber )
        throws ObjectNotFoundException, NotImplementedException,
               InvalidDERException, TokenException
        {
            throw new
            NotImplementedException( "Method \""                         +
                                     "getCertByIssuerAndSerialNum( "     +
                                     "byte[] derIssuer, String issuer, " +
                                     "long serialNumber )"               +
                                     "\" is not implemented!" );
        }
 
/*
        public X509Certificate
        getCertByPrivateKey( PrivateKey key )
        throws ObjectNotFoundException, NotImplementedException,
               IllegalArgumentException, TokenException
        {
            throw new
            NotImplementedException( "Method \""                             +
                                     "getCertByPrivateKey( PrivateKey key )" +
                                     "\" is not implemented!" );
        }
 
 
        public X509Certificate
        getCertByPublicKey( PublicKey key )
        throws ObjectNotFoundException, NotImplementedException,
               IllegalArgumentException, TokenException
        {
            throw new
            NotImplementedException( "Method \""                           +
                                     "getCertByPublicKey( PublicKey key )" +
                                     "\" is not implemented!" );
        }
 
 
        public X509Certificate
        getCertByKeyID( byte[] keyID )
        throws ObjectNotFoundException, NotImplementedException,
               IllegalArgumentException, TokenException
        {
            throw new
            NotImplementedException( "Method \""                      +
                                     "getCertByKeyID( byte[] keyID )" +
                                     "\" is not implemented!" );
        }
 
 
        public void
        deleteCert( X509Certificate cert )
        throws ObjectNotFoundException, NotImplementedException,
               IllegalArgumentException, TokenException
        {
            throw new
            NotImplementedException( "Method \""                      +
                                     "deleteCert( X509Certificate cert )" +
                                     "\" is not implemented!" );
        }
 */

        /**
         * Add a new certificate to the private "certificates"
         * data member.
         *
         * For example:
         * <pre>
         *     storeCertByNickname( certificate, nickname );
         * </pre>
         *
         * @param newCertificate                  The new certificate which
         *                                        must already exist on the
         *                                        corresponding PKCS #11 token.
         *
         * @exception NoSuchItemOnTokenException  This exception is thrown in
         *                                        the case of an inability to
         *                                        remove the certificate from
         *                                        the vector.
         **/

        public synchronized void
        storeCertByNickname( X509Certificate newCertificate, String nickname )
        throws NoSuchItemOnTokenException, NotImplementedException,
               TokenException
        {
            // (1) Add specified certificate to the private
            //     data member named "certificates"

            if( certificates.contains( newCertificate ) != true ) {
                // Add this certificate
                certificates.addElement( newCertificate );
            }
            else {
                // Update this certificate
                if( certificates.removeElement( newCertificate ) != true ) {
                    Assert.notReached( "Certificate has vanished " +
                                       "from vector!" );
                    throw new
                    NoSuchItemOnTokenException( "storeCertByNickname():  " +
                                                "Could not update "        +
                                                "certificate named \""     +
                                                nickname                   +
                                                "\"!" );
                }
                else {
                    certificates.addElement( newCertificate );
                }
            }


            // (2) Call HCL native methods in "exportToPKCS12File"
            //     to export all data contained in the private data
            //     member named "certificates" into a disk file
            //     specified by the data member named "filename",
            //     always creating/recreating it from scratch.
            //
            //     NOTE:  This assumes that a certificate may
            //            exist without it's corresponding key
            //            in the key vector space.  However, a
            //            key may NOT exist without its
            //            corresponding certificate existing
            //            in its certificate vector space.

            exportToPKCS12File();
        }


        public PrivateKey
        getPrivKeyByKeyID( byte[] keyID )
        throws NoSuchItemOnTokenException, NotImplementedException,
               TokenException
        {
            throw new
            NotImplementedException( "Method \""                         +
                                     "getPrivKeyByKeyID( byte[] keyID )" +
                                     "\" is not implemented!" );
        }
 
 
        public PrivateKey
        getPrivKeyByCert( java.security.cert.Certificate cert )
        throws ObjectNotFoundException, NotImplementedException,
               IllegalArgumentException, TokenException
        {
            throw new
            NotImplementedException( "Method \""                       +
                                     "getPrivKeyByCert( "              +
                                     "java.security.cert.Certificate " +
                                     "cert )"                          +
                                     "\" is not implemented!" );
        }
 
 
        public void
        deletePrivKey( PrivateKey key )
        throws NotImplementedException, NoSuchItemOnTokenException,
               TokenException
        {
            throw new
            NotImplementedException( "Method \""                       +
                                     "deletePrivKey( PrivateKey key )" +
                                     "\" is not implemented!" );
        }
 

        public void
        storePrivKey( PrivateKey newKey )
        throws NoSuchItemOnTokenException, NotImplementedException,
               TokenException
        {
            throw new
            NotImplementedException( "Method \""              +
                                     "storePrivKey( "         +
                                     "PK11PrivKey newKey )"   +
                                     "\" is not implemented!" );
        }


        public void
        storePasswordByNickname( Password password, String nickname )
        throws NotImplementedException, TokenException
        {
            throw new
            NotImplementedException( "Method \""                            +
                                     "storePasswordByNickname( "            +
                                     "Password password, String nickname )" +
                                     "\" is not implemented!" );
        }
 
 
        public Password
        getPasswordByNickname( String nickname )
        throws ObjectNotFoundException, NotImplementedException,
               TokenException
        {
            throw new
            NotImplementedException( "Method \""               +
                                     "getPasswordByNickname( " +
                                     "String nickname )"       +
                                     "\" is not implemented!" );
        }
 
 
        public void
        deletePasswordByNickname( String nickname )
        throws ObjectNotFoundException, NotImplementedException,
               TokenException
        {
            throw new
            NotImplementedException( "Method \""                  +
                                     "deletePasswordByNickname( " +
                                     "String nickname )"          +
                                     "\" is not implemented!" );
        }
 

        public void
        storeObjectByNickname( Serializable object, String nickname )
        throws NotImplementedException, TokenException
        {
            throw new
            NotImplementedException( "Method \""               +
                                     "storeObjectByNickname( " +
                                     "Serializable object, "   +
                                     "String nickname )"       +
                                     "\" is not implemented!" );
        }
 
 
        public Serializable
        getObjectByNickname( String nickname )
        throws ObjectNotFoundException, NotImplementedException,
               TokenException
        {
            throw new
            NotImplementedException( "Method \""             +
                                     "getObjectByNickname( " +
                                     "String nickname )"     +
                                     "\" is not implemented!" );
        }
 
 
        public void
        deleteObjectByNickname( String nickname )
        throws ObjectNotFoundException, NotImplementedException,
               TokenException
        {
            throw new
            NotImplementedException( "Method \""                +
                                     "deleteObjectByNickname( " +
                                     "String nickname )"        +
                                     "\" is not implemented!" );
        }
 
 
        //////////////////////////////////////////////////////////////////////
        //  Inner "Member" Protected Methods
        //////////////////////////////////////////////////////////////////////

        /**
         * This protected synchronized method is always called
         * whenever it is known that a file does NOT exist
         * (as denoted by the second argument to the PK12Token
         * factory "constructor" method).  This method is called
         * whenever a certificate, or a certificate along with its
         * corresponding private key need to be saved to an external file.
         *
         * In each case, this new file, referenced by the PK12Token
         * "file" data member, is created/recreated and EVERYTHING
         * in the "certificates" PK12Token data member is copied
         * into this new PKCS #12 file.
         *
         * CALLED BY:  storeCertByNickname()
         **/

        protected synchronized void
        exportToPKCS12File()
        {
            PK12TokenProxy exportProxy = createPK12TokenExportContext();

            for( Enumeration certificate = certificates.elements() ;
                 certificate.hasMoreElements() ; ) {

                exportPK12Token( exportProxy,
                                 ( X509Certificate ) certificate.nextElement() );
            }

            destroyPK12TokenExportContext( exportProxy );
        }
 
 
        //////////////////////////////////////////////////////////////////////
        //  Inner "Member" Private Methods
        //////////////////////////////////////////////////////////////////////

        /**
         * These private native methods are always called via the protected
         * synchronized java method called exportToPKCS12File():
         *
         *     createPK12TokenExportContext():   creates, opens, and
         *                                       truncates the PKCS #12 file
         *                                       intended for export
         *
         *     exportPK12Token():                actually exports a single
         *                                       piece of data from the Java 
         *                                       data member "certificates"
         *                                       along with its corresponding
         *                                       private key if it exists
         *
         *     destroyPK12TokenExportContext():  closes the PKCS #12 file
         *                                       intended for export
         *
         * CALLED BY:  exportToPKCS12File()
         **/

        private native PK12TokenProxy
        createPK12TokenExportContext();

        private native void
        exportPK12Token( PK12TokenProxy exportProxy, X509Certificate certificate );

        private native void
        destroyPK12TokenExportContext( PK12TokenProxy exportProxy );
    }


    //////////////////////////////////////////////////////////////////////////
    //  Factory Constructor Methods
    //////////////////////////////////////////////////////////////////////////

    /**
     * A user calls this static factory method instead of a java constructer
     * to initialize the PK12Token class.  This native function obtains a
     * PKCS #11 slot, transforms it into a JNI byte array, and then calls
     * the protected java constructor, PK12Token( filename, flag, slot ).
     *
     * For example:
     * <pre>
     *     PK12Token token = makePK12Token( "pkcs12file.p12",
     *                                      PK12Token.Flag.FILE_EXISTS );
     * </pre>
     *
     * @param filename    A String containing the name of a PKCS #12 file.
     *
     * @param flag        A Flag containing whether the PKCS #12 file
     *                    should already exist (in which case "flag" is
     *                    Flag.FILE_EXISTS) or should be created (in which
     *                    case "flag" is Flag.CREATE_FILE).
     *
     * @return PK12Token  This method actually invokes the constructor.
     **/

    static public native PK12Token
    makePK12Token( String filename, Flag flag );


    //////////////////////////////////////////////////////////////////////////
    // Constructor
    //////////////////////////////////////////////////////////////////////////

    /**
     * Default constructor which should never be called.
     **/

    private
    PK12Token()
    {
        Assert.assert( false );
    }


    /**
     * Create a new PK12Token.  Note that this constructor is always
     * invoked from the native code which comprises the "factory method",
     * makePK12Token( filename, flag ).
     *
     * CALLED BY:                    makePK12Token( String filename,
     *                                              Flag flag )
     *
     * @param filename               A String containing the name of a
     *                               PKCS #12 file.
     *
     * @param flag                   A Flag containing whether the PKCS #12
     *                               file should already exist (in which case
     *                               "flag" is Flag.FILE_EXISTS) or should be
     *                               created (in which case "flag" is
     *                               Flag.CREATE_FILE).
     *
     * @param slot                   A byte array containing the corresponding
     *                               PKCS #11 slot.
     *
     * @exception InvalidPKCS12FileException  An exception thrown whenever a file is
     *                               expected to exist, and doesn't.
     **/

    protected
    PK12Token( String filename, Flag flag, byte[] slot )
    throws InvalidPKCS12FileException
    {
        // (1) Call constructor of superclass
        super( slot , false, false);


        // (2) Ensure that a valid filename was passed in
        File file;
        try {
            file = new File( filename );
        }
        catch( NullPointerException e ) {
            throw new InvalidPKCS12FileException( "PK12Token( String filename, " +
                                         "Flag flag ):  Must specify "  +
                                         "non-null \"filename\"!" );
        }


        // (3) Perform initialization dependent upon flag
        if( flag == Flag.FILE_EXISTS )
        {
            // (a) Check to ensure that specified filename exists
            //     and is a "normal" file, and if it is, then
            //     call HCL native methods to "importFromPKCS12File"
            //     all data contained in a disk file specified by
            //     the data member named "filename" into the
            //     private data member named "certificates".
            //
            //     NOTE:  This assumes that the PKCS #12 disk
            //            file only contains certificates and
            //            keys, since HCL can only process
            //            these two types of items from a
            //            PKCS #12 file.
            if( ( file.exists() == true ) &&  ( file.isFile() == true ) ) {
                importFromPKCS12File();
            }
            else {
                throw new InvalidPKCS12FileException( "PK12Token( String filename, "  +
                                             "Flag flag ):  Could not open " +
                                             "file named \""                 +
                                             filename                        +
                                             "\"!" );
            }
        }
        else if( flag == Flag.CREATE_FILE ) {
            // (b) Check to ensure that specified filename does NOT exist
            if( file.exists() == false ) {
                throw new InvalidPKCS12FileException( "PK12Token( String filename, " +
                                             "Flag flag ):  File named \""  +
                                             filename                       +
                                             "\" already exists!" );
            }
        }
        else {
            // (c) Bad or missing flag value
            Assert.notReached( "Caller was able to insert an illegal " +
                               "value for Flag!" );
            throw new InvalidPKCS12FileException( "PK12Token( String filename, "     +
                                         "Flag flag ):  Only the \"flag\" " +
                                         "values of Flag.FILE_EXISTS or "   +
                                         "Flag.CREATE_FILE are allowed!" );
        }
    }


    //////////////////////////////////////////////////////////////////////////
    // Finalizer (Destructor)
    //////////////////////////////////////////////////////////////////////////

    /**
     * Destroy a PK12Token.  Actually, this method does
     * nothing more than call its superclass's finalize
     * method.
     **/

    // protected void
    // finalize()
    // {
    //     super.finalize();
    // }


    //////////////////////////////////////////////////////////////////////////
    //  Exception Classes
    //////////////////////////////////////////////////////////////////////////

    /**
     * Thrown if the operation requires a specified file
     * to exist, and it doesn't.
     **/

    static public
    class InvalidPKCS12FileException
    extends Exception
    {
        public
        InvalidPKCS12FileException() {}

        public
        InvalidPKCS12FileException( String message ) {
            super( message );
        }
    }


    //////////////////////////////////////////////////////////////////////////
    //  Protected Methods
    //////////////////////////////////////////////////////////////////////////

    /**
     * This protected synchronized method is always called whenever it is
     * known that a file already exists (as denoted by the second argument to
     * the PK12Token factory "constructor" method).  This method is called
     * whenever a certificate or key need to be read in from an external file.
     *
     * In each case, the new file is opened, and all of its
     * certificates are read into the internal "certificates" data member as
     * well as the default certificate database; all of its keys are read into
     * either the PKCS #11 internal module or the PKCS #11 FIPS module
     * (whichever one has been selected), as well as the default key database.
     *
     * CALLED BY:  PK12Token() constructor
     **/

    protected synchronized void
    importFromPKCS12File()
    {
        PK12TokenProxy importProxy = createPK12TokenImportContext();

        importPK12Token( importProxy );

        destroyPK12TokenImportContext( importProxy );
    }


    /**
     * These private native methods are always called via the protected
     * synchronized java method called importFromPKCS12File():
     *
     *     createPK12TokenImportContext():   opens the PKCS #12 file
     *                                       intended to be imported
     *
     *     importPK12Token():                actually imports all certificate
     *                                       data into the Java data member
     *                                       "certificates" and places this
     *                                       information into the default
     *                                       certificate database; if a
     *                                       private key is encountered,
     *                                       then it is placed into the
     *                                       selected key token and also
     *                                       into the default key database
     *
     *     destroyPK12TokenImportContext():  closes the PKCS #12 file
     *                                       intended for import
     *
     * CALLED BY:  importFromPKCS12File()
     **/

    protected native PK12TokenProxy
    createPK12TokenImportContext();

    protected native void
    importPK12Token( PK12TokenProxy importProxy );

    protected native void
    destroyPK12TokenImportContext( PK12TokenProxy importProxy );


    //////////////////////////////////////////////////////////////////////////
    // Public Data
    //////////////////////////////////////////////////////////////////////////

    // Emulate enumerations
    static public
    class Flag
    {
        private Flag() {}

        static public final Flag FILE_EXISTS = new Flag();
        static public final Flag CREATE_FILE = new Flag();
    }


    //////////////////////////////////////////////////////////////////////////
    // Protected Data
    //////////////////////////////////////////////////////////////////////////


    //////////////////////////////////////////////////////////////////////////
    // Private Data
    //////////////////////////////////////////////////////////////////////////

    private String filename;
    private Vector certificates;
}


class PK12TokenProxy
extends com.netscape.jss.util.NativeProxy
{
    //////////////////////////////////////////////////////////////////////////
    // Constructor
    //////////////////////////////////////////////////////////////////////////

    public
    PK12TokenProxy( byte[] pointer )
    {
        super( pointer );
    }


    //////////////////////////////////////////////////////////////////////////
    // Finalizer (Destructor)
    //////////////////////////////////////////////////////////////////////////

    protected void
    finalize()
    throws Throwable
    {
        super.finalize();
    }


    //////////////////////////////////////////////////////////////////////////
    //  Protected Methods
    //////////////////////////////////////////////////////////////////////////


    /**
     *  This is a "no-op" because all freeing of "C" resources is performed
     *  inside "C" by the
     *  <CODE>destroyPK12TokenExportContext( exportProxy )</CODE> and the
     *  <CODE>destroyPK12TokenImportContext( importProxy )</CODE> functions.
     **/

    protected void
    releaseNativeResources() {}
}

