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

package org.mozilla.jss.crypto;

import java.security.NoSuchAlgorithmException;
import org.mozilla.jss.asn1.*;
import javax.crypto.spec.IvParameterSpec;
import java.util.*;

/**
 * An algorithm for performing symmetric encryption.
 */
public class EncryptionAlgorithm extends Algorithm {

    public static class Mode {
        private String name;

        private static Hashtable nameHash = new Hashtable();

        private Mode() { }
        private Mode(String name) {
            this.name = name;
            nameHash.put(name.toLowerCase(), this);
        }

        public static Mode fromString(String name)
            throws NoSuchAlgorithmException
        {
            Mode m = (Mode) nameHash.get(name.toLowerCase());
            if( m == null ) {
                throw new NoSuchAlgorithmException(
                    "Unrecognized mode \"" + name + "\"");
            }
            return m;
        }

        public String toString() {
            return name;
        }

        public static final Mode NONE = new Mode("NONE");
        public static final Mode ECB = new Mode("ECB");
        public static final Mode CBC = new Mode("CBC");
    }

    public static class Alg {
        private String name;

        private static Hashtable nameHash = new Hashtable();

        private Alg() { }
        private Alg(String name) {
            this.name = name;
            nameHash.put(name.toLowerCase(), this);
        }

        private static Alg fromString(String name)
            throws NoSuchAlgorithmException
        {
            Alg a = (Alg) nameHash.get(name.toLowerCase());
            if( a == null ) {
                throw new NoSuchAlgorithmException("Unrecognized algorithm \""
                    + name + "\"");
            }
            return a;
        }

        public String toString() {
            return name;
        }

        public static final Alg RC4 = new Alg("RC4");
        public static final Alg DES = new Alg("DES");
        public static final Alg DESede = new Alg("DESede");
        public static final Alg AES = new Alg("AES");
        public static final Alg RC2 = new Alg("RC2");
    }

    public static class Padding {
        private String name;

        private static Hashtable nameHash = new Hashtable();

        private Padding() { }
        private Padding(String name) {
            this.name = name;
            nameHash.put(name.toLowerCase(), this);
        }

        public String toString() {
            return name;
        }

        public static Padding fromString(String name)
            throws NoSuchAlgorithmException
        {
            Padding p = (Padding) nameHash.get(name.toLowerCase());
            if( p == null ) {
                throw new NoSuchAlgorithmException("Unrecognized Padding " +
                    "type \"" + name + "\"");
            }
            return p;
        }

        public static final Padding NONE = new Padding("NoPadding");
        public static final Padding PKCS5 = new Padding("PKCS5Padding");
    }

    private static String makeName(Alg alg, Mode mode, Padding padding) {
        StringBuffer buf = new StringBuffer();
        buf.append(alg.toString());
        buf.append('/');
        buf.append(mode.toString());
        buf.append('/');
        buf.append(padding.toString());
        return buf.toString();
    }

    protected EncryptionAlgorithm(int oidTag, Alg alg, Mode mode,
        Padding padding, Class paramClass, int blockSize,
        OBJECT_IDENTIFIER oid, int keyStrength)
    {
        super(oidTag, makeName(alg, mode, padding), oid, paramClass);
        this.alg = alg;
        this.mode = mode;
        this.padding = padding;
        this.blockSize = blockSize;
        if(oid!=null) {
            oidMap.put(oid, this);
        }
        if( name != null ) {
            nameMap.put(name.toLowerCase(), this);
        }
        this.keyStrength = keyStrength;
        algList.addElement(this);
    }

    protected EncryptionAlgorithm(int oidTag, Alg alg, Mode mode,
        Padding padding, Class []paramClasses, int blockSize, 
        OBJECT_IDENTIFIER oid, int keyStrength)
    {
        super(oidTag, makeName(alg, mode, padding), oid, paramClasses);
        this.alg = alg;
        this.mode = mode;
        this.padding = padding;
        this.blockSize = blockSize;
        if(oid!=null) {
            oidMap.put(oid, this);
        }
        if( name != null ) {
            nameMap.put(name.toLowerCase(), this);
        }
        this.keyStrength = keyStrength;
        algList.addElement(this);
    }

    private int blockSize;
    private Alg alg;
    private Mode mode;
    private Padding padding;
    private int keyStrength;

    /**
     * Returns the base algorithm, without the parameters. For example,
     * the base algorithm of "AES/CBC/NoPadding" is "AES".
     */
    public Alg getAlg() {
        return alg;
    }

    /**
     * Returns the mode of this algorithm.
     */
    public Mode getMode() {
        return mode;
    }

    /**
     * Returns the padding type of this algorithm.
     */
    public Padding getPadding() {
        return padding;
    }

    /**
     * Returns the key strength of this algorithm in bits. Algorithms that
     * use continuously variable key sizes (such as RC4) will return 0 to
     * indicate they can use any key size.
     */
    public int getKeyStrength() {
        return keyStrength;
    }

    ///////////////////////////////////////////////////////////////////////
    // mapping
    ///////////////////////////////////////////////////////////////////////
    private static Hashtable oidMap = new Hashtable();
    private static Hashtable nameMap = new Hashtable();
    private static Vector algList = new Vector();

    public static EncryptionAlgorithm fromOID(OBJECT_IDENTIFIER oid)
        throws NoSuchAlgorithmException
    {
        Object alg = oidMap.get(oid);
        if( alg == null ) {
            throw new NoSuchAlgorithmException("OID: " + oid.toString());
        } else {
            return (EncryptionAlgorithm) alg;
        }
    }

    // Note: after we remove this deprecated method, we can remove
    // nameMap.
    /**
     * @deprecated This method is deprecated because algorithm strings
     *  don't contain key length, which is necessary to distinguish between
     *  AES algorithms.
     */
    public static EncryptionAlgorithm fromString(String name)
        throws NoSuchAlgorithmException
    {
        Object alg = nameMap.get(name.toLowerCase());
        if( alg == null ) {
            throw new NoSuchAlgorithmException();
        } else {
            return (EncryptionAlgorithm) alg;
        }
    }

    public static EncryptionAlgorithm lookup(String algName, String modeName,
        String paddingName, int keyStrength)
        throws NoSuchAlgorithmException
    {
        int len = algList.size();
        Alg alg = Alg.fromString(algName);
        Mode mode = Mode.fromString(modeName);
        Padding padding = Padding.fromString(paddingName);
        int i;
        for(i = 0; i < len; ++i ) {
            EncryptionAlgorithm cur =
                (EncryptionAlgorithm) algList.elementAt(i);
            if( cur.alg == alg && cur.mode == mode && cur.padding == padding ) {
                if( cur.keyStrength == 0 || cur.keyStrength == keyStrength ) {
                    break;
                }
            }
        }
        if( i == len ) {
            throw new NoSuchAlgorithmException(algName + "/" + modeName + "/"
                + paddingName + " with key strength " + keyStrength +
                " not found");
        }
        return (EncryptionAlgorithm) algList.elementAt(i);
    }
        

    /** 
     * The blocksize of the algorithm in bytes. Stream algorithms (such as
     * RC4) have a blocksize of 1.
     */
    public int getBlockSize() {
        return blockSize;
    }

    /**
     * Returns <code>true</code> if this algorithm performs padding.
     * @deprecated Call <tt>getPaddingType()</tt> instead.
     */
    public boolean isPadded() {
        return ! Padding.NONE.equals(padding);
    }

    /**
     * Returns the type of padding for this algorithm.
     */
    public Padding getPaddingType() {
        return padding;
    }

    //
    // In JDK 1.4, Sun introduced javax.crypto.spec.IvParameterSpec,
    // which obsoletes org.mozilla.jss.crypto.IVParameterSpec. However,
    // we still need to support pre-1.4 runtimes, so we have to be
    // prepared for this new class not to be available. Here we try to load
    // the new 1.4 class. If we succeed, we will accept either JSS's
    // IVParameterSpec or Java's IvParameterSpec. If we fail, which will
    // happen if we are running a pre-1.4 runtime, we just accept
    // JSS's IVParameterSpec.
    //
    private static Class[] IVParameterSpecClasses = null;
    static {
        try {
            IVParameterSpecClasses = new Class[2];
            IVParameterSpecClasses[0] = IVParameterSpec.class;
            IVParameterSpecClasses[1] = IvParameterSpec.class;
        } catch(NoClassDefFoundError e) {
            // We must be running on a pre-1.4 JRE.
            IVParameterSpecClasses = new Class[1];
            IVParameterSpecClasses[0] = IVParameterSpec.class;
        }
    }

    /**
     * Returns the number of bytes that this algorithm expects in
     * its initialization vector.
     *
     * @return The size in bytes of the IV for this algorithm.  A size of
     *      0 means this algorithm does not take an IV.
     */
    public native int getIVLength();

    public static final EncryptionAlgorithm
    RC4 = new EncryptionAlgorithm(SEC_OID_RC4, Alg.RC4, Mode.NONE, Padding.NONE,
            (Class)null, 1, OBJECT_IDENTIFIER.RSA_CIPHER.subBranch(4), 0);

    public static final EncryptionAlgorithm
    DES_ECB = new EncryptionAlgorithm(SEC_OID_DES_ECB, Alg.DES, Mode.ECB,
        Padding.NONE, (Class)null, 8, OBJECT_IDENTIFIER.ALGORITHM.subBranch(6),
        56);

    public static final EncryptionAlgorithm
    DES_CBC = new EncryptionAlgorithm(SEC_OID_DES_CBC, Alg.DES, Mode.CBC,
        Padding.NONE, IVParameterSpecClasses, 8,
        OBJECT_IDENTIFIER.ALGORITHM.subBranch(7), 56);

    public static final EncryptionAlgorithm
    DES_CBC_PAD = new EncryptionAlgorithm(CKM_DES_CBC_PAD, Alg.DES, Mode.CBC,
        Padding.PKCS5, IVParameterSpecClasses, 8, null, 56); // no oid

    public static final EncryptionAlgorithm
    DES3_ECB = new EncryptionAlgorithm(CKM_DES3_ECB, Alg.DESede, Mode.ECB,
        Padding.NONE, (Class)null, 8, null, 168); // no oid

    public static final EncryptionAlgorithm
    DES3_CBC = new EncryptionAlgorithm(SEC_OID_DES_EDE3_CBC, Alg.DESede,
        Mode.CBC, Padding.NONE, IVParameterSpecClasses, 8,
        OBJECT_IDENTIFIER.RSA_CIPHER.subBranch(7), 168);

    public static final EncryptionAlgorithm
    DES3_CBC_PAD = new EncryptionAlgorithm(CKM_DES3_CBC_PAD, Alg.DESede,
        Mode.CBC, Padding.PKCS5, IVParameterSpecClasses, 8,
        null, 168); //no oid

    public static final EncryptionAlgorithm
    RC2_CBC = new EncryptionAlgorithm(SEC_OID_RC2_CBC, Alg.RC2, Mode.CBC,
        Padding.NONE, IVParameterSpecClasses, 8,
        OBJECT_IDENTIFIER.RSA_CIPHER.subBranch(2), 0);

    public static final OBJECT_IDENTIFIER AES_ROOT_OID = 
        new OBJECT_IDENTIFIER( new long[] 
            { 2, 16, 840, 1, 101, 3, 4, 1 } );

    public static final EncryptionAlgorithm
    AES_128_ECB = new EncryptionAlgorithm(CKM_AES_ECB, Alg.AES, Mode.ECB,
        Padding.NONE, (Class)null, 16,
        AES_ROOT_OID.subBranch(1), 128);

    public static final EncryptionAlgorithm
    AES_128_CBC = new EncryptionAlgorithm(CKM_AES_CBC, Alg.AES, Mode.CBC,
        Padding.NONE, IVParameterSpecClasses, 16,
        AES_ROOT_OID.subBranch(2), 128);

    public static final EncryptionAlgorithm
    AES_192_ECB = new EncryptionAlgorithm(CKM_AES_ECB, Alg.AES, Mode.ECB,
        Padding.NONE, (Class)null, 16, AES_ROOT_OID.subBranch(21), 192);

    public static final EncryptionAlgorithm
    AES_192_CBC = new EncryptionAlgorithm(CKM_AES_CBC, Alg.AES, Mode.CBC,
        Padding.NONE, IVParameterSpecClasses, 16,
        AES_ROOT_OID.subBranch(22), 192);

    public static final EncryptionAlgorithm
    AES_256_ECB = new EncryptionAlgorithm(CKM_AES_ECB, Alg.AES, Mode.ECB,
        Padding.NONE, (Class)null, 16, AES_ROOT_OID.subBranch(41), 256);

    public static final EncryptionAlgorithm
    AES_256_CBC = new EncryptionAlgorithm(CKM_AES_CBC, Alg.AES, Mode.CBC,
        Padding.NONE, IVParameterSpecClasses, 16,
        AES_ROOT_OID.subBranch(42), 256);

    public static final EncryptionAlgorithm
    AES_CBC_PAD = new EncryptionAlgorithm(CKM_AES_CBC_PAD, Alg.AES, Mode.CBC,
        Padding.PKCS5, IVParameterSpecClasses, 16, null, 256); // no oid
}
