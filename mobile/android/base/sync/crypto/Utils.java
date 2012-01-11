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
 * The Original Code is Android Sync Client.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Jason Voll
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

package org.mozilla.gecko.sync.crypto;

import java.io.UnsupportedEncodingException;
import java.util.Arrays;

import org.mozilla.apache.commons.codec.binary.Base32;
import org.mozilla.apache.commons.codec.binary.Base64;

public class Utils {

    /*
     * Helper to convert Hex String to Byte Array
     * Input: Hex string
     * Output: byte[] version of hex string
     */
    public static byte[] hex2Byte(String str)
    {
        if (str.length() % 2 == 1) {
            str = "0" + str;
        }

        byte[] bytes = new byte[str.length() / 2];
        for (int i = 0; i < bytes.length; i++)
        {
            bytes[i] = (byte) Integer
                .parseInt(str.substring(2 * i, 2 * i + 2), 16);
        }
        return bytes;
    }

    /*
     * Helper to convert Byte Array to a Hex String
     * Input: byte[] array
     * Output: Hex string
     */
    public static String byte2hex(byte[] b) {

        // String Buffer can be used instead
        String hs = "";
        String stmp = "";

        for (int n = 0; n < b.length; n++) {
            stmp = (java.lang.Integer.toHexString(b[n] & 0XFF));

            if (stmp.length() == 1) {
                hs = hs + "0" + stmp;
            } else {
                hs = hs + stmp;
            }

            if (n < b.length - 1) {
                hs = hs + "";
            }
        }

        return hs;
    }

    /*
     * Helper for array concatenation.
     * Input: At least two byte[]
     * Output: A concatenated version of them
     */
    public static byte[] concatAll(byte[] first, byte[]... rest) {
        int totalLength = first.length;
        for (byte[] array : rest) {
            totalLength += array.length;
        }

        byte[] result = Arrays.copyOf(first, totalLength);
        int offset = first.length;

        for (byte[] array : rest) {
            System.arraycopy(array, 0, result, offset, array.length);
            offset += array.length;
        }
        return result;
    }

    /*
     * Decode a friendly base32 string.
     */
    public static byte[] decodeFriendlyBase32(String base32) {
        Base32 converter = new Base32();
        return converter.decode(base32.replace('8', 'l').replace('9', 'o')
                .toUpperCase());
    }

    /**
     * Utility for Base64 decoding. Should ensure that the correct
     * Apache Commons version is used.
     *
     * @param base64
     *        An input string. Will be decoded as UTF-8.
     * @return
     *        A byte array of decoded values.
     * @throws UnsupportedEncodingException
     *         Should not occur.
     */
    public static byte[] decodeBase64(String base64) throws UnsupportedEncodingException {
        return Base64.decodeBase64(base64.getBytes("UTF-8"));
    }
}
