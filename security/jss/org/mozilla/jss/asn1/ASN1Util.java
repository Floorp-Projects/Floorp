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

import java.io.*;
import org.mozilla.jss.asn1.InvalidBERException;
import org.mozilla.jss.util.Assert;

public class ASN1Util {

    public static byte[] encode(ASN1Value val) {
        return encode(val.getTag(), val);
    }

    public static byte[] encode(Tag implicitTag, ASN1Value val)
    {
      try {

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        val.encode(implicitTag, bos);
        return bos.toByteArray();

      } catch( IOException e ) {
        Assert.notReached("Encoding to byte array gave IOException");
        return null;
      }
    }

    public static ASN1Value decode(ASN1Template template, byte[] encoded)
        throws InvalidBERException
    {
      try {

        ByteArrayInputStream bis = new ByteArrayInputStream(encoded);
        return template.decode(bis);

      } catch( IOException e ) {
        Assert.notReached("Decoding from byte array gave IOException");
        return null;
      }
    }
    
    public static ASN1Value decode(Tag implicitTag, ASN1Template template,
                            byte[] encoded)
        throws InvalidBERException
    {
      try {

        ByteArrayInputStream bis = new ByteArrayInputStream(encoded);
        return template.decode(implicitTag, bis);

      } catch( IOException e ) {
        Assert.notReached("Decoding from byte array gave IOException");
        return null;
      }
    }



    /**
     * Fills a byte array with bytes from an input stream.  This method
     * keeps reading until the array is filled, an IOException occurs, or EOF
     * is reached.  The byte array will be completely filled unless an
     * exception is thrown.
     *
     * @param bytes A byte array which will be filled up.
     * @param istream The input stream from which to read the bytes.
     * @exception IOException If an IOException occurs reading from the
     *      stream, or EOF is reached before the byte array is filled.
     */
    public static void readFully(byte[] bytes, InputStream istream)
        throws IOException
    {

        int numRead=0;
        while(numRead < bytes.length) {
            int nr = istream.read(bytes, numRead, bytes.length-numRead);
            if( nr == -1 ) {
                throw new EOFException();
            }
            numRead += nr;
        }
    }
}
