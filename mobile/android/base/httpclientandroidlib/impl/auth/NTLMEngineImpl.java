/*
 * ====================================================================
 *
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 */

package ch.boye.httpclientandroidlib.impl.auth;

import java.security.Key;
import java.security.MessageDigest;
import java.util.Arrays;

import javax.crypto.Cipher;
import javax.crypto.spec.SecretKeySpec;

import org.mozilla.apache.commons.codec.binary.Base64;
import ch.boye.httpclientandroidlib.util.EncodingUtils;

/**
 * Provides an implementation for NTLMv1, NTLMv2, and NTLM2 Session forms of the NTLM
 * authentication protocol.
 *
 * @since 4.1
 */
final class NTLMEngineImpl implements NTLMEngine {

    // Flags we use
    protected final static int FLAG_UNICODE_ENCODING = 0x00000001;
    protected final static int FLAG_TARGET_DESIRED = 0x00000004;
    protected final static int FLAG_NEGOTIATE_SIGN = 0x00000010;
    protected final static int FLAG_NEGOTIATE_SEAL = 0x00000020;
    protected final static int FLAG_NEGOTIATE_NTLM = 0x00000200;
    protected final static int FLAG_NEGOTIATE_ALWAYS_SIGN = 0x00008000;
    protected final static int FLAG_NEGOTIATE_NTLM2 = 0x00080000;
    protected final static int FLAG_NEGOTIATE_128 = 0x20000000;
    protected final static int FLAG_NEGOTIATE_KEY_EXCH = 0x40000000;

    /** Secure random generator */
    private static final java.security.SecureRandom RND_GEN;
    static {
        java.security.SecureRandom rnd = null;
        try {
            rnd = java.security.SecureRandom.getInstance("SHA1PRNG");
        } catch (Exception e) {
        }
        RND_GEN = rnd;
    }

    /** Character encoding */
    static final String DEFAULT_CHARSET = "ASCII";

    /** The character set to use for encoding the credentials */
    private String credentialCharset = DEFAULT_CHARSET;

    /** The signature string as bytes in the default encoding */
    private static byte[] SIGNATURE;

    static {
        byte[] bytesWithoutNull = EncodingUtils.getBytes("NTLMSSP", "ASCII");
        SIGNATURE = new byte[bytesWithoutNull.length + 1];
        System.arraycopy(bytesWithoutNull, 0, SIGNATURE, 0, bytesWithoutNull.length);
        SIGNATURE[bytesWithoutNull.length] = (byte) 0x00;
    }

    /**
     * Returns the response for the given message.
     *
     * @param message
     *            the message that was received from the server.
     * @param username
     *            the username to authenticate with.
     * @param password
     *            the password to authenticate with.
     * @param host
     *            The host.
     * @param domain
     *            the NT domain to authenticate in.
     * @return The response.
     * @throws HttpException
     *             If the messages cannot be retrieved.
     */
    final String getResponseFor(String message, String username, String password,
            String host, String domain) throws NTLMEngineException {

        final String response;
        if (message == null || message.trim().equals("")) {
            response = getType1Message(host, domain);
        } else {
            Type2Message t2m = new Type2Message(message);
            response = getType3Message(username, password, host, domain, t2m.getChallenge(), t2m
                    .getFlags(), t2m.getTarget(), t2m.getTargetInfo());
        }
        return response;
    }

    /**
     * Creates the first message (type 1 message) in the NTLM authentication
     * sequence. This message includes the user name, domain and host for the
     * authentication session.
     *
     * @param host
     *            the computer name of the host requesting authentication.
     * @param domain
     *            The domain to authenticate with.
     * @return String the message to add to the HTTP request header.
     */
    String getType1Message(String host, String domain) throws NTLMEngineException {
        return new Type1Message(domain, host).getResponse();
    }

    /**
     * Creates the type 3 message using the given server nonce. The type 3
     * message includes all the information for authentication, host, domain,
     * username and the result of encrypting the nonce sent by the server using
     * the user's password as the key.
     *
     * @param user
     *            The user name. This should not include the domain name.
     * @param password
     *            The password.
     * @param host
     *            The host that is originating the authentication request.
     * @param domain
     *            The domain to authenticate within.
     * @param nonce
     *            the 8 byte array the server sent.
     * @return The type 3 message.
     * @throws NTLMEngineException
     *             If {@encrypt(byte[],byte[])} fails.
     */
    String getType3Message(String user, String password, String host, String domain,
            byte[] nonce, int type2Flags, String target, byte[] targetInformation)
            throws NTLMEngineException {
        return new Type3Message(domain, host, user, password, nonce, type2Flags, target,
                targetInformation).getResponse();
    }

    /**
     * @return Returns the credentialCharset.
     */
    String getCredentialCharset() {
        return credentialCharset;
    }

    /**
     * @param credentialCharset
     *            The credentialCharset to set.
     */
    void setCredentialCharset(String credentialCharset) {
        this.credentialCharset = credentialCharset;
    }

    /** Strip dot suffix from a name */
    private static String stripDotSuffix(String value) {
        int index = value.indexOf(".");
        if (index != -1)
            return value.substring(0, index);
        return value;
    }

    /** Convert host to standard form */
    private static String convertHost(String host) {
        return stripDotSuffix(host);
    }

    /** Convert domain to standard form */
    private static String convertDomain(String domain) {
        return stripDotSuffix(domain);
    }

    private static int readULong(byte[] src, int index) throws NTLMEngineException {
        if (src.length < index + 4)
            throw new NTLMEngineException("NTLM authentication - buffer too small for DWORD");
        return (src[index] & 0xff) | ((src[index + 1] & 0xff) << 8)
                | ((src[index + 2] & 0xff) << 16) | ((src[index + 3] & 0xff) << 24);
    }

    private static int readUShort(byte[] src, int index) throws NTLMEngineException {
        if (src.length < index + 2)
            throw new NTLMEngineException("NTLM authentication - buffer too small for WORD");
        return (src[index] & 0xff) | ((src[index + 1] & 0xff) << 8);
    }

    private static byte[] readSecurityBuffer(byte[] src, int index) throws NTLMEngineException {
        int length = readUShort(src, index);
        int offset = readULong(src, index + 4);
        if (src.length < offset + length)
            throw new NTLMEngineException(
                    "NTLM authentication - buffer too small for data item");
        byte[] buffer = new byte[length];
        System.arraycopy(src, offset, buffer, 0, length);
        return buffer;
    }

    /** Calculate a challenge block */
    private static byte[] makeRandomChallenge() throws NTLMEngineException {
        if (RND_GEN == null) {
            throw new NTLMEngineException("Random generator not available");
        }
        byte[] rval = new byte[8];
        synchronized (RND_GEN) {
            RND_GEN.nextBytes(rval);
        }
        return rval;
    }

    /** Calculate an NTLM2 challenge block */
    private static byte[] makeNTLM2RandomChallenge() throws NTLMEngineException {
        if (RND_GEN == null) {
            throw new NTLMEngineException("Random generator not available");
        }
        byte[] rval = new byte[24];
        synchronized (RND_GEN) {
            RND_GEN.nextBytes(rval);
        }
        // 8-byte challenge, padded with zeros to 24 bytes.
        Arrays.fill(rval, 8, 24, (byte) 0x00);
        return rval;
    }

    /**
     * Calculates the LM Response for the given challenge, using the specified
     * password.
     *
     * @param password
     *            The user's password.
     * @param challenge
     *            The Type 2 challenge from the server.
     *
     * @return The LM Response.
     */
    static byte[] getLMResponse(String password, byte[] challenge)
            throws NTLMEngineException {
        byte[] lmHash = lmHash(password);
        return lmResponse(lmHash, challenge);
    }

    /**
     * Calculates the NTLM Response for the given challenge, using the specified
     * password.
     *
     * @param password
     *            The user's password.
     * @param challenge
     *            The Type 2 challenge from the server.
     *
     * @return The NTLM Response.
     */
    static byte[] getNTLMResponse(String password, byte[] challenge)
            throws NTLMEngineException {
        byte[] ntlmHash = ntlmHash(password);
        return lmResponse(ntlmHash, challenge);
    }

    /**
     * Calculates the NTLMv2 Response for the given challenge, using the
     * specified authentication target, username, password, target information
     * block, and client challenge.
     *
     * @param target
     *            The authentication target (i.e., domain).
     * @param user
     *            The username.
     * @param password
     *            The user's password.
     * @param targetInformation
     *            The target information block from the Type 2 message.
     * @param challenge
     *            The Type 2 challenge from the server.
     * @param clientChallenge
     *            The random 8-byte client challenge.
     *
     * @return The NTLMv2 Response.
     */
    static byte[] getNTLMv2Response(String target, String user, String password,
            byte[] challenge, byte[] clientChallenge, byte[] targetInformation)
            throws NTLMEngineException {
        byte[] ntlmv2Hash = ntlmv2Hash(target, user, password);
        byte[] blob = createBlob(clientChallenge, targetInformation);
        return lmv2Response(ntlmv2Hash, challenge, blob);
    }

    /**
     * Calculates the LMv2 Response for the given challenge, using the specified
     * authentication target, username, password, and client challenge.
     *
     * @param target
     *            The authentication target (i.e., domain).
     * @param user
     *            The username.
     * @param password
     *            The user's password.
     * @param challenge
     *            The Type 2 challenge from the server.
     * @param clientChallenge
     *            The random 8-byte client challenge.
     *
     * @return The LMv2 Response.
     */
    static byte[] getLMv2Response(String target, String user, String password,
            byte[] challenge, byte[] clientChallenge) throws NTLMEngineException {
        byte[] ntlmv2Hash = ntlmv2Hash(target, user, password);
        return lmv2Response(ntlmv2Hash, challenge, clientChallenge);
    }

    /**
     * Calculates the NTLM2 Session Response for the given challenge, using the
     * specified password and client challenge.
     *
     * @param password
     *            The user's password.
     * @param challenge
     *            The Type 2 challenge from the server.
     * @param clientChallenge
     *            The random 8-byte client challenge.
     *
     * @return The NTLM2 Session Response. This is placed in the NTLM response
     *         field of the Type 3 message; the LM response field contains the
     *         client challenge, null-padded to 24 bytes.
     */
    static byte[] getNTLM2SessionResponse(String password, byte[] challenge,
            byte[] clientChallenge) throws NTLMEngineException {
        try {
            byte[] ntlmHash = ntlmHash(password);

            // Look up MD5 algorithm (was necessary on jdk 1.4.2)
            // This used to be needed, but java 1.5.0_07 includes the MD5
            // algorithm (finally)
            // Class x = Class.forName("gnu.crypto.hash.MD5");
            // Method updateMethod = x.getMethod("update",new
            // Class[]{byte[].class});
            // Method digestMethod = x.getMethod("digest",new Class[0]);
            // Object mdInstance = x.newInstance();
            // updateMethod.invoke(mdInstance,new Object[]{challenge});
            // updateMethod.invoke(mdInstance,new Object[]{clientChallenge});
            // byte[] digest = (byte[])digestMethod.invoke(mdInstance,new
            // Object[0]);

            MessageDigest md5 = MessageDigest.getInstance("MD5");
            md5.update(challenge);
            md5.update(clientChallenge);
            byte[] digest = md5.digest();

            byte[] sessionHash = new byte[8];
            System.arraycopy(digest, 0, sessionHash, 0, 8);
            return lmResponse(ntlmHash, sessionHash);
        } catch (Exception e) {
            if (e instanceof NTLMEngineException)
                throw (NTLMEngineException) e;
            throw new NTLMEngineException(e.getMessage(), e);
        }
    }

    /**
     * Creates the LM Hash of the user's password.
     *
     * @param password
     *            The password.
     *
     * @return The LM Hash of the given password, used in the calculation of the
     *         LM Response.
     */
    private static byte[] lmHash(String password) throws NTLMEngineException {
        try {
            byte[] oemPassword = password.toUpperCase().getBytes("US-ASCII");
            int length = Math.min(oemPassword.length, 14);
            byte[] keyBytes = new byte[14];
            System.arraycopy(oemPassword, 0, keyBytes, 0, length);
            Key lowKey = createDESKey(keyBytes, 0);
            Key highKey = createDESKey(keyBytes, 7);
            byte[] magicConstant = "KGS!@#$%".getBytes("US-ASCII");
            Cipher des = Cipher.getInstance("DES/ECB/NoPadding");
            des.init(Cipher.ENCRYPT_MODE, lowKey);
            byte[] lowHash = des.doFinal(magicConstant);
            des.init(Cipher.ENCRYPT_MODE, highKey);
            byte[] highHash = des.doFinal(magicConstant);
            byte[] lmHash = new byte[16];
            System.arraycopy(lowHash, 0, lmHash, 0, 8);
            System.arraycopy(highHash, 0, lmHash, 8, 8);
            return lmHash;
        } catch (Exception e) {
            throw new NTLMEngineException(e.getMessage(), e);
        }
    }

    /**
     * Creates the NTLM Hash of the user's password.
     *
     * @param password
     *            The password.
     *
     * @return The NTLM Hash of the given password, used in the calculation of
     *         the NTLM Response and the NTLMv2 and LMv2 Hashes.
     */
    private static byte[] ntlmHash(String password) throws NTLMEngineException {
        try {
            byte[] unicodePassword = password.getBytes("UnicodeLittleUnmarked");
            MD4 md4 = new MD4();
            md4.update(unicodePassword);
            return md4.getOutput();
        } catch (java.io.UnsupportedEncodingException e) {
            throw new NTLMEngineException("Unicode not supported: " + e.getMessage(), e);
        }
    }

    /**
     * Creates the NTLMv2 Hash of the user's password.
     *
     * @param target
     *            The authentication target (i.e., domain).
     * @param user
     *            The username.
     * @param password
     *            The password.
     *
     * @return The NTLMv2 Hash, used in the calculation of the NTLMv2 and LMv2
     *         Responses.
     */
    private static byte[] ntlmv2Hash(String target, String user, String password)
            throws NTLMEngineException {
        try {
            byte[] ntlmHash = ntlmHash(password);
            HMACMD5 hmacMD5 = new HMACMD5(ntlmHash);
            // Upper case username, mixed case target!!
            hmacMD5.update(user.toUpperCase().getBytes("UnicodeLittleUnmarked"));
            hmacMD5.update(target.getBytes("UnicodeLittleUnmarked"));
            return hmacMD5.getOutput();
        } catch (java.io.UnsupportedEncodingException e) {
            throw new NTLMEngineException("Unicode not supported! " + e.getMessage(), e);
        }
    }

    /**
     * Creates the LM Response from the given hash and Type 2 challenge.
     *
     * @param hash
     *            The LM or NTLM Hash.
     * @param challenge
     *            The server challenge from the Type 2 message.
     *
     * @return The response (either LM or NTLM, depending on the provided hash).
     */
    private static byte[] lmResponse(byte[] hash, byte[] challenge) throws NTLMEngineException {
        try {
            byte[] keyBytes = new byte[21];
            System.arraycopy(hash, 0, keyBytes, 0, 16);
            Key lowKey = createDESKey(keyBytes, 0);
            Key middleKey = createDESKey(keyBytes, 7);
            Key highKey = createDESKey(keyBytes, 14);
            Cipher des = Cipher.getInstance("DES/ECB/NoPadding");
            des.init(Cipher.ENCRYPT_MODE, lowKey);
            byte[] lowResponse = des.doFinal(challenge);
            des.init(Cipher.ENCRYPT_MODE, middleKey);
            byte[] middleResponse = des.doFinal(challenge);
            des.init(Cipher.ENCRYPT_MODE, highKey);
            byte[] highResponse = des.doFinal(challenge);
            byte[] lmResponse = new byte[24];
            System.arraycopy(lowResponse, 0, lmResponse, 0, 8);
            System.arraycopy(middleResponse, 0, lmResponse, 8, 8);
            System.arraycopy(highResponse, 0, lmResponse, 16, 8);
            return lmResponse;
        } catch (Exception e) {
            throw new NTLMEngineException(e.getMessage(), e);
        }
    }

    /**
     * Creates the LMv2 Response from the given hash, client data, and Type 2
     * challenge.
     *
     * @param hash
     *            The NTLMv2 Hash.
     * @param clientData
     *            The client data (blob or client challenge).
     * @param challenge
     *            The server challenge from the Type 2 message.
     *
     * @return The response (either NTLMv2 or LMv2, depending on the client
     *         data).
     */
    private static byte[] lmv2Response(byte[] hash, byte[] challenge, byte[] clientData)
            throws NTLMEngineException {
        HMACMD5 hmacMD5 = new HMACMD5(hash);
        hmacMD5.update(challenge);
        hmacMD5.update(clientData);
        byte[] mac = hmacMD5.getOutput();
        byte[] lmv2Response = new byte[mac.length + clientData.length];
        System.arraycopy(mac, 0, lmv2Response, 0, mac.length);
        System.arraycopy(clientData, 0, lmv2Response, mac.length, clientData.length);
        return lmv2Response;
    }

    /**
     * Creates the NTLMv2 blob from the given target information block and
     * client challenge.
     *
     * @param targetInformation
     *            The target information block from the Type 2 message.
     * @param clientChallenge
     *            The random 8-byte client challenge.
     *
     * @return The blob, used in the calculation of the NTLMv2 Response.
     */
    private static byte[] createBlob(byte[] clientChallenge, byte[] targetInformation) {
        byte[] blobSignature = new byte[] { (byte) 0x01, (byte) 0x01, (byte) 0x00, (byte) 0x00 };
        byte[] reserved = new byte[] { (byte) 0x00, (byte) 0x00, (byte) 0x00, (byte) 0x00 };
        byte[] unknown1 = new byte[] { (byte) 0x00, (byte) 0x00, (byte) 0x00, (byte) 0x00 };
        long time = System.currentTimeMillis();
        time += 11644473600000l; // milliseconds from January 1, 1601 -> epoch.
        time *= 10000; // tenths of a microsecond.
        // convert to little-endian byte array.
        byte[] timestamp = new byte[8];
        for (int i = 0; i < 8; i++) {
            timestamp[i] = (byte) time;
            time >>>= 8;
        }
        byte[] blob = new byte[blobSignature.length + reserved.length + timestamp.length + 8
                + unknown1.length + targetInformation.length];
        int offset = 0;
        System.arraycopy(blobSignature, 0, blob, offset, blobSignature.length);
        offset += blobSignature.length;
        System.arraycopy(reserved, 0, blob, offset, reserved.length);
        offset += reserved.length;
        System.arraycopy(timestamp, 0, blob, offset, timestamp.length);
        offset += timestamp.length;
        System.arraycopy(clientChallenge, 0, blob, offset, 8);
        offset += 8;
        System.arraycopy(unknown1, 0, blob, offset, unknown1.length);
        offset += unknown1.length;
        System.arraycopy(targetInformation, 0, blob, offset, targetInformation.length);
        return blob;
    }

    /**
     * Creates a DES encryption key from the given key material.
     *
     * @param bytes
     *            A byte array containing the DES key material.
     * @param offset
     *            The offset in the given byte array at which the 7-byte key
     *            material starts.
     *
     * @return A DES encryption key created from the key material starting at
     *         the specified offset in the given byte array.
     */
    private static Key createDESKey(byte[] bytes, int offset) {
        byte[] keyBytes = new byte[7];
        System.arraycopy(bytes, offset, keyBytes, 0, 7);
        byte[] material = new byte[8];
        material[0] = keyBytes[0];
        material[1] = (byte) (keyBytes[0] << 7 | (keyBytes[1] & 0xff) >>> 1);
        material[2] = (byte) (keyBytes[1] << 6 | (keyBytes[2] & 0xff) >>> 2);
        material[3] = (byte) (keyBytes[2] << 5 | (keyBytes[3] & 0xff) >>> 3);
        material[4] = (byte) (keyBytes[3] << 4 | (keyBytes[4] & 0xff) >>> 4);
        material[5] = (byte) (keyBytes[4] << 3 | (keyBytes[5] & 0xff) >>> 5);
        material[6] = (byte) (keyBytes[5] << 2 | (keyBytes[6] & 0xff) >>> 6);
        material[7] = (byte) (keyBytes[6] << 1);
        oddParity(material);
        return new SecretKeySpec(material, "DES");
    }

    /**
     * Applies odd parity to the given byte array.
     *
     * @param bytes
     *            The data whose parity bits are to be adjusted for odd parity.
     */
    private static void oddParity(byte[] bytes) {
        for (int i = 0; i < bytes.length; i++) {
            byte b = bytes[i];
            boolean needsParity = (((b >>> 7) ^ (b >>> 6) ^ (b >>> 5) ^ (b >>> 4) ^ (b >>> 3)
                    ^ (b >>> 2) ^ (b >>> 1)) & 0x01) == 0;
            if (needsParity) {
                bytes[i] |= (byte) 0x01;
            } else {
                bytes[i] &= (byte) 0xfe;
            }
        }
    }

    /** NTLM message generation, base class */
    static class NTLMMessage {
        /** The current response */
        private byte[] messageContents = null;

        /** The current output position */
        private int currentOutputPosition = 0;

        /** Constructor to use when message contents are not yet known */
        NTLMMessage() {
        }

        /** Constructor to use when message contents are known */
        NTLMMessage(String messageBody, int expectedType) throws NTLMEngineException {
            messageContents = Base64.decodeBase64(EncodingUtils.getBytes(messageBody,
                    DEFAULT_CHARSET));
            // Look for NTLM message
            if (messageContents.length < SIGNATURE.length)
                throw new NTLMEngineException("NTLM message decoding error - packet too short");
            int i = 0;
            while (i < SIGNATURE.length) {
                if (messageContents[i] != SIGNATURE[i])
                    throw new NTLMEngineException(
                            "NTLM message expected - instead got unrecognized bytes");
                i++;
            }

            // Check to be sure there's a type 2 message indicator next
            int type = readULong(SIGNATURE.length);
            if (type != expectedType)
                throw new NTLMEngineException("NTLM type " + Integer.toString(expectedType)
                        + " message expected - instead got type " + Integer.toString(type));

            currentOutputPosition = messageContents.length;
        }

        /**
         * Get the length of the signature and flags, so calculations can adjust
         * offsets accordingly.
         */
        protected int getPreambleLength() {
            return SIGNATURE.length + 4;
        }

        /** Get the message length */
        protected int getMessageLength() {
            return currentOutputPosition;
        }

        /** Read a byte from a position within the message buffer */
        protected byte readByte(int position) throws NTLMEngineException {
            if (messageContents.length < position + 1)
                throw new NTLMEngineException("NTLM: Message too short");
            return messageContents[position];
        }

        /** Read a bunch of bytes from a position in the message buffer */
        protected void readBytes(byte[] buffer, int position) throws NTLMEngineException {
            if (messageContents.length < position + buffer.length)
                throw new NTLMEngineException("NTLM: Message too short");
            System.arraycopy(messageContents, position, buffer, 0, buffer.length);
        }

        /** Read a ushort from a position within the message buffer */
        protected int readUShort(int position) throws NTLMEngineException {
            return NTLMEngineImpl.readUShort(messageContents, position);
        }

        /** Read a ulong from a position within the message buffer */
        protected int readULong(int position) throws NTLMEngineException {
            return NTLMEngineImpl.readULong(messageContents, position);
        }

        /** Read a security buffer from a position within the message buffer */
        protected byte[] readSecurityBuffer(int position) throws NTLMEngineException {
            return NTLMEngineImpl.readSecurityBuffer(messageContents, position);
        }

        /**
         * Prepares the object to create a response of the given length.
         *
         * @param length
         *            the maximum length of the response to prepare, not
         *            including the type and the signature (which this method
         *            adds).
         */
        protected void prepareResponse(int maxlength, int messageType) {
            messageContents = new byte[maxlength];
            currentOutputPosition = 0;
            addBytes(SIGNATURE);
            addULong(messageType);
        }

        /**
         * Adds the given byte to the response.
         *
         * @param b
         *            the byte to add.
         */
        protected void addByte(byte b) {
            messageContents[currentOutputPosition] = b;
            currentOutputPosition++;
        }

        /**
         * Adds the given bytes to the response.
         *
         * @param bytes
         *            the bytes to add.
         */
        protected void addBytes(byte[] bytes) {
            for (int i = 0; i < bytes.length; i++) {
                messageContents[currentOutputPosition] = bytes[i];
                currentOutputPosition++;
            }
        }

        /** Adds a USHORT to the response */
        protected void addUShort(int value) {
            addByte((byte) (value & 0xff));
            addByte((byte) (value >> 8 & 0xff));
        }

        /** Adds a ULong to the response */
        protected void addULong(int value) {
            addByte((byte) (value & 0xff));
            addByte((byte) (value >> 8 & 0xff));
            addByte((byte) (value >> 16 & 0xff));
            addByte((byte) (value >> 24 & 0xff));
        }

        /**
         * Returns the response that has been generated after shrinking the
         * array if required and base64 encodes the response.
         *
         * @return The response as above.
         */
        String getResponse() {
            byte[] resp;
            if (messageContents.length > currentOutputPosition) {
                byte[] tmp = new byte[currentOutputPosition];
                for (int i = 0; i < currentOutputPosition; i++) {
                    tmp[i] = messageContents[i];
                }
                resp = tmp;
            } else {
                resp = messageContents;
            }
            return EncodingUtils.getAsciiString(Base64.encodeBase64(resp));
        }

    }

    /** Type 1 message assembly class */
    static class Type1Message extends NTLMMessage {
        protected byte[] hostBytes;
        protected byte[] domainBytes;

        /** Constructor. Include the arguments the message will need */
        Type1Message(String domain, String host) throws NTLMEngineException {
            super();
            try {
                // Strip off domain name from the host!
                host = convertHost(host);
                // Use only the base domain name!
                domain = convertDomain(domain);

                hostBytes = host.getBytes("UnicodeLittleUnmarked");
                domainBytes = domain.toUpperCase().getBytes("UnicodeLittleUnmarked");
            } catch (java.io.UnsupportedEncodingException e) {
                throw new NTLMEngineException("Unicode unsupported: " + e.getMessage(), e);
            }
        }

        /**
         * Getting the response involves building the message before returning
         * it
         */
        @Override
        String getResponse() {
            // Now, build the message. Calculate its length first, including
            // signature or type.
            int finalLength = 32 + hostBytes.length + domainBytes.length;

            // Set up the response. This will initialize the signature, message
            // type, and flags.
            prepareResponse(finalLength, 1);

            // Flags. These are the complete set of flags we support.
            addULong(FLAG_NEGOTIATE_NTLM | FLAG_NEGOTIATE_NTLM2 | FLAG_NEGOTIATE_SIGN
                    | FLAG_NEGOTIATE_SEAL |
                    /*
                     * FLAG_NEGOTIATE_ALWAYS_SIGN | FLAG_NEGOTIATE_KEY_EXCH |
                     */
                    FLAG_UNICODE_ENCODING | FLAG_TARGET_DESIRED | FLAG_NEGOTIATE_128);

            // Domain length (two times).
            addUShort(domainBytes.length);
            addUShort(domainBytes.length);

            // Domain offset.
            addULong(hostBytes.length + 32);

            // Host length (two times).
            addUShort(hostBytes.length);
            addUShort(hostBytes.length);

            // Host offset (always 32).
            addULong(32);

            // Host String.
            addBytes(hostBytes);

            // Domain String.
            addBytes(domainBytes);

            return super.getResponse();
        }

    }

    /** Type 2 message class */
    static class Type2Message extends NTLMMessage {
        protected byte[] challenge;
        protected String target;
        protected byte[] targetInfo;
        protected int flags;

        Type2Message(String message) throws NTLMEngineException {
            super(message, 2);

            // Parse out the rest of the info we need from the message
            // The nonce is the 8 bytes starting from the byte in position 24.
            challenge = new byte[8];
            readBytes(challenge, 24);

            flags = readULong(20);
            if ((flags & FLAG_UNICODE_ENCODING) == 0)
                throw new NTLMEngineException(
                        "NTLM type 2 message has flags that make no sense: "
                                + Integer.toString(flags));
            // Do the target!
            target = null;
            // The TARGET_DESIRED flag is said to not have understood semantics
            // in Type2 messages, so use the length of the packet to decide
            // how to proceed instead
            if (getMessageLength() >= 12 + 8) {
                byte[] bytes = readSecurityBuffer(12);
                if (bytes.length != 0) {
                    try {
                        target = new String(bytes, "UnicodeLittleUnmarked");
                    } catch (java.io.UnsupportedEncodingException e) {
                        throw new NTLMEngineException(e.getMessage(), e);
                    }
                }
            }

            // Do the target info!
            targetInfo = null;
            // TARGET_DESIRED flag cannot be relied on, so use packet length
            if (getMessageLength() >= 40 + 8) {
                byte[] bytes = readSecurityBuffer(40);
                if (bytes.length != 0) {
                    targetInfo = bytes;
                }
            }
        }

        /** Retrieve the challenge */
        byte[] getChallenge() {
            return challenge;
        }

        /** Retrieve the target */
        String getTarget() {
            return target;
        }

        /** Retrieve the target info */
        byte[] getTargetInfo() {
            return targetInfo;
        }

        /** Retrieve the response flags */
        int getFlags() {
            return flags;
        }

    }

    /** Type 3 message assembly class */
    static class Type3Message extends NTLMMessage {
        // Response flags from the type2 message
        protected int type2Flags;

        protected byte[] domainBytes;
        protected byte[] hostBytes;
        protected byte[] userBytes;

        protected byte[] lmResp;
        protected byte[] ntResp;

        /** Constructor. Pass the arguments we will need */
        Type3Message(String domain, String host, String user, String password, byte[] nonce,
                int type2Flags, String target, byte[] targetInformation)
                throws NTLMEngineException {
            // Save the flags
            this.type2Flags = type2Flags;

            // Strip off domain name from the host!
            host = convertHost(host);
            // Use only the base domain name!
            domain = convertDomain(domain);

            // Use the new code to calculate the responses, including v2 if that
            // seems warranted.
            try {
                if (targetInformation != null && target != null) {
                    byte[] clientChallenge = makeRandomChallenge();
                    ntResp = getNTLMv2Response(target, user, password, nonce, clientChallenge,
                            targetInformation);
                    lmResp = getLMv2Response(target, user, password, nonce, clientChallenge);
                } else {
                    if ((type2Flags & FLAG_NEGOTIATE_NTLM2) != 0) {
                        // NTLM2 session stuff is requested
                        byte[] clientChallenge = makeNTLM2RandomChallenge();

                        ntResp = getNTLM2SessionResponse(password, nonce, clientChallenge);
                        lmResp = clientChallenge;

                        // All the other flags we send (signing, sealing, key
                        // exchange) are supported, but they don't do anything
                        // at all in an
                        // NTLM2 context! So we're done at this point.
                    } else {
                        ntResp = getNTLMResponse(password, nonce);
                        lmResp = getLMResponse(password, nonce);
                    }
                }
            } catch (NTLMEngineException e) {
                // This likely means we couldn't find the MD4 hash algorithm -
                // fail back to just using LM
                ntResp = new byte[0];
                lmResp = getLMResponse(password, nonce);
            }

            try {
                domainBytes = domain.toUpperCase().getBytes("UnicodeLittleUnmarked");
                hostBytes = host.getBytes("UnicodeLittleUnmarked");
                userBytes = user.getBytes("UnicodeLittleUnmarked");
            } catch (java.io.UnsupportedEncodingException e) {
                throw new NTLMEngineException("Unicode not supported: " + e.getMessage(), e);
            }
        }

        /** Assemble the response */
        @Override
        String getResponse() {
            int ntRespLen = ntResp.length;
            int lmRespLen = lmResp.length;

            int domainLen = domainBytes.length;
            int hostLen = hostBytes.length;
            int userLen = userBytes.length;

            // Calculate the layout within the packet
            int lmRespOffset = 64;
            int ntRespOffset = lmRespOffset + lmRespLen;
            int domainOffset = ntRespOffset + ntRespLen;
            int userOffset = domainOffset + domainLen;
            int hostOffset = userOffset + userLen;
            int sessionKeyOffset = hostOffset + hostLen;
            int finalLength = sessionKeyOffset + 0;

            // Start the response. Length includes signature and type
            prepareResponse(finalLength, 3);

            // LM Resp Length (twice)
            addUShort(lmRespLen);
            addUShort(lmRespLen);

            // LM Resp Offset
            addULong(lmRespOffset);

            // NT Resp Length (twice)
            addUShort(ntRespLen);
            addUShort(ntRespLen);

            // NT Resp Offset
            addULong(ntRespOffset);

            // Domain length (twice)
            addUShort(domainLen);
            addUShort(domainLen);

            // Domain offset.
            addULong(domainOffset);

            // User Length (twice)
            addUShort(userLen);
            addUShort(userLen);

            // User offset
            addULong(userOffset);

            // Host length (twice)
            addUShort(hostLen);
            addUShort(hostLen);

            // Host offset
            addULong(hostOffset);

            // 4 bytes of zeros - not sure what this is
            addULong(0);

            // Message length
            addULong(finalLength);

            // Flags. Currently: NEGOTIATE_NTLM + UNICODE_ENCODING +
            // TARGET_DESIRED + NEGOTIATE_128
            addULong(FLAG_NEGOTIATE_NTLM | FLAG_UNICODE_ENCODING | FLAG_TARGET_DESIRED
                    | FLAG_NEGOTIATE_128 | (type2Flags & FLAG_NEGOTIATE_NTLM2)
                    | (type2Flags & FLAG_NEGOTIATE_SIGN) | (type2Flags & FLAG_NEGOTIATE_SEAL)
                    | (type2Flags & FLAG_NEGOTIATE_KEY_EXCH)
                    | (type2Flags & FLAG_NEGOTIATE_ALWAYS_SIGN));

            // Add the actual data
            addBytes(lmResp);
            addBytes(ntResp);
            addBytes(domainBytes);
            addBytes(userBytes);
            addBytes(hostBytes);

            return super.getResponse();
        }
    }

    static void writeULong(byte[] buffer, int value, int offset) {
        buffer[offset] = (byte) (value & 0xff);
        buffer[offset + 1] = (byte) (value >> 8 & 0xff);
        buffer[offset + 2] = (byte) (value >> 16 & 0xff);
        buffer[offset + 3] = (byte) (value >> 24 & 0xff);
    }

    static int F(int x, int y, int z) {
        return ((x & y) | (~x & z));
    }

    static int G(int x, int y, int z) {
        return ((x & y) | (x & z) | (y & z));
    }

    static int H(int x, int y, int z) {
        return (x ^ y ^ z);
    }

    static int rotintlft(int val, int numbits) {
        return ((val << numbits) | (val >>> (32 - numbits)));
    }

    /**
     * Cryptography support - MD4. The following class was based loosely on the
     * RFC and on code found at http://www.cs.umd.edu/~harry/jotp/src/md.java.
     * Code correctness was verified by looking at MD4.java from the jcifs
     * library (http://jcifs.samba.org). It was massaged extensively to the
     * final form found here by Karl Wright (kwright@metacarta.com).
     */
    static class MD4 {
        protected int A = 0x67452301;
        protected int B = 0xefcdab89;
        protected int C = 0x98badcfe;
        protected int D = 0x10325476;
        protected long count = 0L;
        protected byte[] dataBuffer = new byte[64];

        MD4() {
        }

        void update(byte[] input) {
            // We always deal with 512 bits at a time. Correspondingly, there is
            // a buffer 64 bytes long that we write data into until it gets
            // full.
            int curBufferPos = (int) (count & 63L);
            int inputIndex = 0;
            while (input.length - inputIndex + curBufferPos >= dataBuffer.length) {
                // We have enough data to do the next step. Do a partial copy
                // and a transform, updating inputIndex and curBufferPos
                // accordingly
                int transferAmt = dataBuffer.length - curBufferPos;
                System.arraycopy(input, inputIndex, dataBuffer, curBufferPos, transferAmt);
                count += transferAmt;
                curBufferPos = 0;
                inputIndex += transferAmt;
                processBuffer();
            }

            // If there's anything left, copy it into the buffer and leave it.
            // We know there's not enough left to process.
            if (inputIndex < input.length) {
                int transferAmt = input.length - inputIndex;
                System.arraycopy(input, inputIndex, dataBuffer, curBufferPos, transferAmt);
                count += transferAmt;
                curBufferPos += transferAmt;
            }
        }

        byte[] getOutput() {
            // Feed pad/length data into engine. This must round out the input
            // to a multiple of 512 bits.
            int bufferIndex = (int) (count & 63L);
            int padLen = (bufferIndex < 56) ? (56 - bufferIndex) : (120 - bufferIndex);
            byte[] postBytes = new byte[padLen + 8];
            // Leading 0x80, specified amount of zero padding, then length in
            // bits.
            postBytes[0] = (byte) 0x80;
            // Fill out the last 8 bytes with the length
            for (int i = 0; i < 8; i++) {
                postBytes[padLen + i] = (byte) ((count * 8) >>> (8 * i));
            }

            // Update the engine
            update(postBytes);

            // Calculate final result
            byte[] result = new byte[16];
            writeULong(result, A, 0);
            writeULong(result, B, 4);
            writeULong(result, C, 8);
            writeULong(result, D, 12);
            return result;
        }

        protected void processBuffer() {
            // Convert current buffer to 16 ulongs
            int[] d = new int[16];

            for (int i = 0; i < 16; i++) {
                d[i] = (dataBuffer[i * 4] & 0xff) + ((dataBuffer[i * 4 + 1] & 0xff) << 8)
                        + ((dataBuffer[i * 4 + 2] & 0xff) << 16)
                        + ((dataBuffer[i * 4 + 3] & 0xff) << 24);
            }

            // Do a round of processing
            int AA = A;
            int BB = B;
            int CC = C;
            int DD = D;
            round1(d);
            round2(d);
            round3(d);
            A += AA;
            B += BB;
            C += CC;
            D += DD;

        }

        protected void round1(int[] d) {
            A = rotintlft((A + F(B, C, D) + d[0]), 3);
            D = rotintlft((D + F(A, B, C) + d[1]), 7);
            C = rotintlft((C + F(D, A, B) + d[2]), 11);
            B = rotintlft((B + F(C, D, A) + d[3]), 19);

            A = rotintlft((A + F(B, C, D) + d[4]), 3);
            D = rotintlft((D + F(A, B, C) + d[5]), 7);
            C = rotintlft((C + F(D, A, B) + d[6]), 11);
            B = rotintlft((B + F(C, D, A) + d[7]), 19);

            A = rotintlft((A + F(B, C, D) + d[8]), 3);
            D = rotintlft((D + F(A, B, C) + d[9]), 7);
            C = rotintlft((C + F(D, A, B) + d[10]), 11);
            B = rotintlft((B + F(C, D, A) + d[11]), 19);

            A = rotintlft((A + F(B, C, D) + d[12]), 3);
            D = rotintlft((D + F(A, B, C) + d[13]), 7);
            C = rotintlft((C + F(D, A, B) + d[14]), 11);
            B = rotintlft((B + F(C, D, A) + d[15]), 19);
        }

        protected void round2(int[] d) {
            A = rotintlft((A + G(B, C, D) + d[0] + 0x5a827999), 3);
            D = rotintlft((D + G(A, B, C) + d[4] + 0x5a827999), 5);
            C = rotintlft((C + G(D, A, B) + d[8] + 0x5a827999), 9);
            B = rotintlft((B + G(C, D, A) + d[12] + 0x5a827999), 13);

            A = rotintlft((A + G(B, C, D) + d[1] + 0x5a827999), 3);
            D = rotintlft((D + G(A, B, C) + d[5] + 0x5a827999), 5);
            C = rotintlft((C + G(D, A, B) + d[9] + 0x5a827999), 9);
            B = rotintlft((B + G(C, D, A) + d[13] + 0x5a827999), 13);

            A = rotintlft((A + G(B, C, D) + d[2] + 0x5a827999), 3);
            D = rotintlft((D + G(A, B, C) + d[6] + 0x5a827999), 5);
            C = rotintlft((C + G(D, A, B) + d[10] + 0x5a827999), 9);
            B = rotintlft((B + G(C, D, A) + d[14] + 0x5a827999), 13);

            A = rotintlft((A + G(B, C, D) + d[3] + 0x5a827999), 3);
            D = rotintlft((D + G(A, B, C) + d[7] + 0x5a827999), 5);
            C = rotintlft((C + G(D, A, B) + d[11] + 0x5a827999), 9);
            B = rotintlft((B + G(C, D, A) + d[15] + 0x5a827999), 13);

        }

        protected void round3(int[] d) {
            A = rotintlft((A + H(B, C, D) + d[0] + 0x6ed9eba1), 3);
            D = rotintlft((D + H(A, B, C) + d[8] + 0x6ed9eba1), 9);
            C = rotintlft((C + H(D, A, B) + d[4] + 0x6ed9eba1), 11);
            B = rotintlft((B + H(C, D, A) + d[12] + 0x6ed9eba1), 15);

            A = rotintlft((A + H(B, C, D) + d[2] + 0x6ed9eba1), 3);
            D = rotintlft((D + H(A, B, C) + d[10] + 0x6ed9eba1), 9);
            C = rotintlft((C + H(D, A, B) + d[6] + 0x6ed9eba1), 11);
            B = rotintlft((B + H(C, D, A) + d[14] + 0x6ed9eba1), 15);

            A = rotintlft((A + H(B, C, D) + d[1] + 0x6ed9eba1), 3);
            D = rotintlft((D + H(A, B, C) + d[9] + 0x6ed9eba1), 9);
            C = rotintlft((C + H(D, A, B) + d[5] + 0x6ed9eba1), 11);
            B = rotintlft((B + H(C, D, A) + d[13] + 0x6ed9eba1), 15);

            A = rotintlft((A + H(B, C, D) + d[3] + 0x6ed9eba1), 3);
            D = rotintlft((D + H(A, B, C) + d[11] + 0x6ed9eba1), 9);
            C = rotintlft((C + H(D, A, B) + d[7] + 0x6ed9eba1), 11);
            B = rotintlft((B + H(C, D, A) + d[15] + 0x6ed9eba1), 15);

        }

    }

    /**
     * Cryptography support - HMACMD5 - algorithmically based on various web
     * resources by Karl Wright
     */
    static class HMACMD5 {
        protected byte[] ipad;
        protected byte[] opad;
        protected MessageDigest md5;

        HMACMD5(byte[] key) throws NTLMEngineException {
            try {
                md5 = MessageDigest.getInstance("MD5");
            } catch (Exception ex) {
                // Umm, the algorithm doesn't exist - throw an
                // NTLMEngineException!
                throw new NTLMEngineException(
                        "Error getting md5 message digest implementation: " + ex.getMessage(), ex);
            }

            // Initialize the pad buffers with the key
            ipad = new byte[64];
            opad = new byte[64];

            int keyLength = key.length;
            if (keyLength > 64) {
                // Use MD5 of the key instead, as described in RFC 2104
                md5.update(key);
                key = md5.digest();
                keyLength = key.length;
            }
            int i = 0;
            while (i < keyLength) {
                ipad[i] = (byte) (key[i] ^ (byte) 0x36);
                opad[i] = (byte) (key[i] ^ (byte) 0x5c);
                i++;
            }
            while (i < 64) {
                ipad[i] = (byte) 0x36;
                opad[i] = (byte) 0x5c;
                i++;
            }

            // Very important: update the digest with the ipad buffer
            md5.reset();
            md5.update(ipad);

        }

        /** Grab the current digest. This is the "answer". */
        byte[] getOutput() {
            byte[] digest = md5.digest();
            md5.update(opad);
            return md5.digest(digest);
        }

        /** Update by adding a complete array */
        void update(byte[] input) {
            md5.update(input);
        }

        /** Update the algorithm */
        void update(byte[] input, int offset, int length) {
            md5.update(input, offset, length);
        }

    }

    public String generateType1Msg(
            final String domain,
            final String workstation) throws NTLMEngineException {
        return getType1Message(workstation, domain);
    }

    public String generateType3Msg(
            final String username,
            final String password,
            final String domain,
            final String workstation,
            final String challenge) throws NTLMEngineException {
        Type2Message t2m = new Type2Message(challenge);
        return getType3Message(
                username,
                password,
                workstation,
                domain,
                t2m.getChallenge(),
                t2m.getFlags(),
                t2m.getTarget(),
                t2m.getTargetInfo());
    }

}
