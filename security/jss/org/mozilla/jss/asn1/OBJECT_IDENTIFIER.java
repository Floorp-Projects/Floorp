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
package org.mozilla.jss.asn1;

import java.io.InputStream;
import java.io.OutputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import org.mozilla.jss.util.Assert;
import java.util.Vector;
import java.util.StringTokenizer;

public class OBJECT_IDENTIFIER implements ASN1Value {

    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////
    // Standard object identifiers
    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////

    /**
     * The OID space for RSA Data Security, Inc.
     */
    public static final OBJECT_IDENTIFIER RSADSI =
        new OBJECT_IDENTIFIER( new long[]{1, 2, 840, 113549} );

    /**
     * The OID space for RSA's PKCS (public key cryptography standards).
     */
    public static final OBJECT_IDENTIFIER PKCS =
        RSADSI.subBranch(1);

    /**
     * The OID space for RSA's PKCS #1.
     */
    public static final OBJECT_IDENTIFIER PKCS1 =
        PKCS.subBranch(1);

    /**
     * The OID space for RSA's PKCS #2, which has since been folded into
     * PKCS #1.
     */
    public static final OBJECT_IDENTIFIER PKCS2 = 
        PKCS.subBranch(2);

    /**
     * The OID space for RSA's message digest algorithms.
     */
    public static final OBJECT_IDENTIFIER RSA_DIGEST = RSADSI.subBranch(2);

    /**
     * The OID space for RSA's password-based encryption standard.
     */
    public static final OBJECT_IDENTIFIER PKCS5 = PKCS.subBranch(5);

    /**
     * The OID space for RSA's Selected Attribute Types standard, PKCS #9.
     */
    public static final OBJECT_IDENTIFIER PKCS9 = PKCS.subBranch(9);

    /**
     * The OID space for RSA's personal information exchange syntax standard.
     */
    public static final OBJECT_IDENTIFIER PKCS12 = PKCS.subBranch(12);

    /**
     * The OID space for RSA's ciphers.
     */
    public static final OBJECT_IDENTIFIER RSA_CIPHER = RSADSI.subBranch(3);

    /**
     * The OID space for FIPS standardized algorithms.
     */
    public static final OBJECT_IDENTIFIER ALGORITHM =
        new OBJECT_IDENTIFIER( new long[] { 1, 3, 14, 3, 2 } );

    /**
     * The OID space for FIPS-180-2 SHA256/SHA384/SHA512 standardized algorithms.
     */
    public static final OBJECT_IDENTIFIER HASH_ALGORITHM =
        new OBJECT_IDENTIFIER( new long[] {2, 16, 840, 1, 101, 3, 4 } );


    /**
     * The OID space for PKIX.
     */
    public static final OBJECT_IDENTIFIER PKIX =
        new OBJECT_IDENTIFIER( new long[] { 1, 3, 6, 1, 5, 5, 7 } );

    public static final OBJECT_IDENTIFIER
    id_cmc = PKIX.subBranch( 7 );

    /**
     * CMC control attributes
     */
    public static final OBJECT_IDENTIFIER
    id_cmc_cMCStatusInfo = id_cmc.subBranch(1);
    public static final OBJECT_IDENTIFIER
    id_cmc_identification = id_cmc.subBranch(2);
    public static final OBJECT_IDENTIFIER
    id_cmc_identityProof = id_cmc.subBranch(3);
    public static final OBJECT_IDENTIFIER
    id_cmc_dataReturn = id_cmc.subBranch(4);
    public static final OBJECT_IDENTIFIER
    id_cmc_transactionId = id_cmc.subBranch(5);
    public static final OBJECT_IDENTIFIER
    id_cmc_senderNonce = id_cmc.subBranch(6);
    public static final OBJECT_IDENTIFIER
    id_cmc_recipientNonce = id_cmc.subBranch(7);
    public static final OBJECT_IDENTIFIER
    id_cmc_addExtensions = id_cmc.subBranch(8);
    public static final OBJECT_IDENTIFIER
    id_cmc_encryptedPOP = id_cmc.subBranch(9);
    public static final OBJECT_IDENTIFIER
    id_cmc_decryptedPOP = id_cmc.subBranch(10);
    public static final OBJECT_IDENTIFIER
    id_cmc_lraPOPWitness = id_cmc.subBranch(11);
    public static final OBJECT_IDENTIFIER
    id_cmc_getCert = id_cmc.subBranch(15);
    public static final OBJECT_IDENTIFIER
    id_cmc_getCRL  = id_cmc.subBranch(16);
    public static final OBJECT_IDENTIFIER
    id_cmc_revokeRequest = id_cmc.subBranch(17);
    public static final OBJECT_IDENTIFIER
    id_cmc_regInfo = id_cmc.subBranch(18);
    public static final OBJECT_IDENTIFIER
    id_cmc_responseInfo = id_cmc.subBranch(19);
    public static final OBJECT_IDENTIFIER
    id_cmc_QueryPending = id_cmc.subBranch(21);
    public static final OBJECT_IDENTIFIER
    id_cmc_idPOPLinkRandom = id_cmc.subBranch(22);
    public static final OBJECT_IDENTIFIER
    id_cmc_idPOPLinkWitness = id_cmc.subBranch(23);
    public static final OBJECT_IDENTIFIER
    id_cmc_idConfirmCertAcceptance = id_cmc.subBranch(24);

    public static final OBJECT_IDENTIFIER
    id_cct = PKIX.subBranch( 12 );

    public static final OBJECT_IDENTIFIER
    id_cct_PKIData = id_cct.subBranch( 2 );

    public static final OBJECT_IDENTIFIER
    id_cct_PKIResponse = id_cct.subBranch( 3 );


    public static final Tag TAG = new Tag(Tag.Class.UNIVERSAL, 6);
    public Tag getTag() {
        return TAG;
    }
    public static final Form FORM = Form.PRIMITIVE;

    private long[] numbers;

    /**
     * Creates an OBJECT_IDENTIFIER from an array of longs, which constitute
     * the numbers that make up the OBJECT IDENTIFIER.
     */
    public OBJECT_IDENTIFIER( long[] numbers ) {
        checkLongArray(numbers);
        this.numbers = numbers;
    }

    /**
     * Checks the given array of numbers to see if it is a valid OID.
     * This is not an exhaustive test, it just looks for obvious problems.
     * It will throw an assertion if a problem is found. With DEBUG turned
     * off, it just checks for null.
     */
    private static void checkLongArray(long[] numbers) {
        Assert._assert(numbers != null);
        if(numbers == null) {
            throw new NullPointerException();
        }
        Assert._assert(numbers.length >= 2);
        Assert._assert( numbers[0]==0 || numbers[0]==1 || numbers[0]==2 );
    }
    

    /**
     * Creates an OBJECT_IDENTIFIER from a String version.  The proper format
     * for the OID string is dotted numbers, for example:
     * "<code>3.2.456.53.23.64</code>".
     *
     * @exception NumberFormatException If the given string cannot be
     *      parsed into an OID.
     */
    public OBJECT_IDENTIFIER( String dottedOID ) throws NumberFormatException {

        if( dottedOID == null || dottedOID.length()==0 ) {
            throw new NumberFormatException("OID string is zero-length");
        }

        StringTokenizer stok = new StringTokenizer(dottedOID, ".");
        numbers = new long[ stok.countTokens() ];
        int i = 0;
        while(stok.hasMoreElements()) {
            numbers[i++] = Long.parseLong( stok.nextToken() );
        }
        Assert._assert( i == numbers.length );
        checkLongArray(numbers);
    }

    public long[] getNumbers() {
        return numbers;
    }

    public int hashCode() {
        int code = 1;
        for(int i = 0; i < numbers.length; i++) {
            code = (int) (code + numbers[i])*10;
        }
        return code;
    }

    /**
     * Creates a new OBJECT_IDENTIFIER that is a sub-branch of this one.
     * For example, if <code>OBJECT_IDENTIFIER oid</code> has the value
     * { 1 3 5 6 },
     * then calling <code>oid.subBranch(4)</code> would return a new
     * OBJECT_IDENTIFIER with the value { 1 3 5 6 4 }.
     */
    public OBJECT_IDENTIFIER subBranch(long num) {
        long[] nums = new long[ numbers.length + 1];
        System.arraycopy(numbers, 0, nums, 0, numbers.length);
        nums[numbers.length] = num;
        return new OBJECT_IDENTIFIER(nums);
    }

    /**
     * Creates a new OBJECT_IDENTIFIER that is a sub-branch of this one.
     * For example, if <code>OBJECT_IDENTIFIER oid</code> has the value
     * { 1 3 5 6 },
     * then calling <code>oid.subBranch(new long[]{ 4, 3})</code>
     * would return a new
     * OBJECT_IDENTIFIER with the value { 1 3 5 6 4 3}.
     */
    public OBJECT_IDENTIFIER subBranch(long[] newNums) {
        long[] nums = new long[ numbers.length + newNums.length];
        System.arraycopy(numbers, 0, nums, 0, numbers.length);
        System.arraycopy(newNums, 0, nums, numbers.length, newNums.length);
        return new OBJECT_IDENTIFIER(nums);
    }

    public boolean equals(Object obj) {
        if(obj == null || ! (obj instanceof OBJECT_IDENTIFIER)) {
            return false;
        }
        long[] nums = ((OBJECT_IDENTIFIER)obj).numbers;
        if( nums.length != numbers.length ) {
            return false;
        }
        for(int i = 0; i < nums.length; i++) {
            if( nums[i] != numbers[i] ) {
                return false;
            }
        }
        return true;
    }

    public String toString() {
        String ret = "{" + String.valueOf(numbers[0]);
        for(int i=1; i < numbers.length; i++) {
            ret = ret + " " + numbers[i];
        }
        ret += "}";
        return ret;
    }

    public void encode(OutputStream ostream) throws IOException {
        encode(TAG, ostream);
    }

    private byte[] encodedContents = null;
    /**
     * Gets the encoding of the contents, or a cached copy.
     * Since the content encoding is the same regardless of the Tag,
     * this only needs to be computed once.
     */
    private byte[] getEncodedContents() {
        if( encodedContents == null ) {
            encodedContents = computeEncodedContents();
        }
        return encodedContents;
    }

    // We cache our encoding for a given tag.  99% of the time, only
    // one tag will be used for an instance, so we will get a cache hit.
    // In the remaining 1%, we'll have to recompute the encoding.
    byte[] cachedEncoding=null;
    Tag tagForCache=null;
    /**
     * Returns the encoding for the given tag.  If the encoding for
     * this tag was previously computed (and no encoding for a different
     *  tag has since been computed), this method returns a cached copy.
     * Otherwise, the encoding will be recomputed.
     */
    private byte[] getEncoding(Tag tag) {
        if( ! tag.equals(tagForCache) ) {
            // recompute for new tag
            ByteArrayOutputStream out = new ByteArrayOutputStream();

            ASN1Header head = getHeader(tag);
            try {
                head.encode(out);
            } catch( IOException e ) {
                // should never happen on a byte array output stream
                Assert.notReached("exception while encoding ASN.1 header");
            }

            out.write( getEncodedContents(), 0, getEncodedContents().length );

            tagForCache = tag;
            cachedEncoding = out.toByteArray();
        }
        return cachedEncoding;
    }

    /**
     * Compute the ASN1 header for this tag.
     */
    private ASN1Header getHeader(Tag implicitTag) {
        return new ASN1Header( implicitTag, FORM, getEncodedContents().length );
    }

    /**
     * Actually computes the encoding of this object identifier.
     */
    private byte[] computeEncodedContents() {
        ByteArrayOutputStream out = new ByteArrayOutputStream();

        // handle first number
        Assert._assert(numbers.length >= 2);
        long n = numbers[0];
        Assert._assert( n == 0 || n == 1 || n == 2 );
        long outb = ( numbers[0] * 40 ) + numbers[1];
        Assert._assert( ((byte)outb) == outb );
        out.write( (byte)outb );

        // handle consecutive numbers
        for( int i = 2; i < numbers.length; i++ ) {
            n = numbers[i];
            Assert._assert( n >= 0 );

            // array of output bytes, in reverse order.  10 bytes, at 7 bits
            // per byte, is 70 bits, which is more than enough to handle
            // the maximum value of a long, which takes up 63 bits.
            byte[] rev = new byte[10];
            int idx=0; // index into reversed bytes

            // Create reversed byte list
            do {
                rev[idx++] = (byte) (n % 128);
                n = n / 128;
            } while( n > 0 );
            idx--; // backup to point to last element

            // now print them in reverse order
            while( idx > 0 ) {
                // all but last byte have MSB==1
                out.write( rev[idx--]  | 0x80 );
            }
            Assert._assert(idx == 0);
            // last byte has MSB==0
            out.write( rev[0] );
        }

        return out.toByteArray();
    }
        

    public void encode(Tag implicitTag, OutputStream ostream)
        throws IOException
    {
        ostream.write( getEncoding(implicitTag) );
    }

    private static final Template templateInstance = new Template();
    public static Template getTemplate() {
        return templateInstance;
    }

///////////////////////////////////////////////////////////////////////
// OBJECT_IDENTIFIER.Template
//
public static class Template implements ASN1Template {

    public Tag getTag() {
        return OBJECT_IDENTIFIER.TAG;
    }
    public boolean tagMatch(Tag tag) {
        return( tag.equals(OBJECT_IDENTIFIER.TAG) );
    }

    public Form getForm() {
        return OBJECT_IDENTIFIER.FORM;
    }
    public boolean formMatch(Form form) {
        return( form == OBJECT_IDENTIFIER.FORM );
    }

    public ASN1Value decode(InputStream istream)
        throws IOException, InvalidBERException
    {
        return decode(getTag(), istream);
    }

    public ASN1Value decode(Tag implicitTag, InputStream istream)
        throws IOException, InvalidBERException
    {
      try {
        ASN1Header head = new ASN1Header(istream);
        long remainingContent = head.getContentLength();

        // Check the information gleaned from the header
        if( ! head.getTag().equals( implicitTag ) ) {
            throw new InvalidBERException("Incorrect tag for "+
                "OBJECT IDENTIFIER: "+ head.getTag() );
        }
        if( head.getForm() != getForm() ) {
            throw new InvalidBERException("Incorrect form for OBJECT "+
                "IDENTIFIER");
        }
        if( remainingContent < 1 ) {
            throw new InvalidBERException("Invalid 0 length for OBJECT"+
                " IDENTIFIER");
        }

        Vector numberV = new Vector();

        // handle first byte, which contains first two numbers
        byte b = readByte(istream);
        remainingContent--;
        long num = b % 40;
        numberV.addElement( new Long( b % 40 ) ); // second number
        numberV.insertElementAt( new Long( b / 40 ), 0); // first number

        // handle the rest of the numbers
        while( remainingContent > 0 ) {
            num = 0;

            // keep reading until MSB == 0
            int bitcount=0;
            do {
                if( (bitcount+=7) > 63 ) {
                    // we're about to overflow our long
                    throw new InvalidBERException("OBJECT IDENTIFIER "+
                        "element too long; max is 63 bits");
                }
                b = readByte(istream);
                remainingContent--;
                num <<= 7;
                num |= (b & 0x7f);
            } while( (b & 0x80) != 0 );

            numberV.addElement( new Long( num ) );
        }

        // convert Vector to array
        long numbers[] = new long[ numberV.size() ];
        for(int i = 0; i < numbers.length; i++) {
            numbers[i] = ((Long)numberV.elementAt(i)).longValue();
        }

        // create OBJECT_IDENTIFIER from array
        return new OBJECT_IDENTIFIER(numbers);

      } catch(InvalidBERException e) {
        throw new InvalidBERException(e, "OBJECT IDENTIFIER");
      }
    }

    /**
     *  Reads in a byte from the stream, throws an InvalidBERException
     *  if EOF is reached.
     */
    private static byte readByte(InputStream istream)
        throws InvalidBERException, IOException
    {
        int n = istream.read();
        if( n == -1 ) {
            throw new InvalidBERException("End-of-file reached while "+
                "decoding OBJECT IDENTIFIER");
        }
        Assert._assert( (n & 0xff) == n );
        return (byte) n;
    }
        
} // end of OBJECT_IDENTIFIER.Template

}
