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
 * The Original Code is Network Security Services for Java.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
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

package org.mozilla.jss.util;


import java.io.*;
import org.mozilla.jss.asn1.ASN1Util;
import java.util.Arrays;


/**
 * Reads in base-64 encoded input and spits out the raw binary decoding.
 */
public class Base64InputStream extends FilterInputStream {

    private static final int WOULD_BLOCK = -2;

    //
    // decoding table
    //
    private static int[] table = new int[256];

    // one-time initialization of decoding table
    static {
        int i;
        for( i=0; i < 256; ++i) {
            table[i] = -1;
        }
        int c;
        for( c = 'A', i=0; c <= 'Z'; ++c, ++i) {
            table[c] = i;
        }
        for( c = 'a'; c <= 'z'; ++c, ++i) {
            table[c] = i;
        }
        for( c='0'; c <= '9'; ++c, ++i) {
            table[c] = i;
        }
        table['+'] = 62;
        table['/'] = 63;
    }

    // prev is the previous significant character read from the in stream.
    // Significant characters are those that are part of the encoded data,
    // as opposed to whitespace.
    private int prev, savedPrev;

    // state is the current state of our state machine. The states are 1-4,
    // indicating which character of the current 4-character block we
    // are looking for. After state 4 we wrap back to state 1. The state
    // is not advanced when we read an insignificant character (such as
    // whitespace).
    private int state = 1, savedState;

    public Base64InputStream(InputStream in) {
        super(in);
    }

    public long skip(long n) throws IOException {
        long count = 0;
        while( (count < n) && (read() != -1) ) {
            ++count;
        }
        return count;
    }

    /**
     * param block Whether or not to block waiting for input.
     */
    private int read(boolean block) throws IOException {
        int cur, ret=0;
        boolean done = false;
        while(!done) {
            if( in.available() < 1 && !block) {
                return WOULD_BLOCK;
            }
            cur = in.read();
            switch(state) {
              case 1:
                if( cur == -1 ) {
                    // end of file
                    return -1;
                }
                if( cur == '=' ) {
                    throw new IOException("Invalid pad character");
                }
                if( table[cur] != -1 ) {
                    prev = cur;
                    state = 2;
                }
                break;
              case 2:
                if( cur == -1 ) {
                    throw new EOFException("Unexpected end-of-file");
                }
                if( cur == '=' ) {
                    throw new IOException("Invalid pad character");
                }
                if( table[cur] != -1 ) {
                    ret = (table[prev]<<2) | ((table[cur]&0x30)>>4);
                    prev = cur;
                    state = 3;
                    done = true;
                }
                break;
              case 3:
                if( cur == -1 ) {
                    throw new EOFException("Unexpected end-of-file");
                }
                if( cur == '=' ) {
                    // pad character
                    return -1;
                }
                if( table[cur] != -1 ) {
                    ret = ((table[prev]&0x0f)<<4) | ((table[cur]&0x3c)>>2);
                    prev = cur;
                    state = 4;
                    done = true;
                }
                break;
              case 4:
                if( cur == -1 ) {
                    throw new EOFException("Unexpected end-of-file");
                }
                if( cur == '=' ) {
                    // pad character
                    return -1;
                }
                if( table[cur] != -1 ) {
                    ret = ((table[prev]&0x03)<<6) | table[cur];
                    state = 1;
                    done = true;
                }
                break;
              default:
                Assert._assert(false);
                break;
            }
        }
        return ret;
    }

    public int read() throws IOException {
        return read(true);
    }

    public int read(byte[] b, int off, int len) throws IOException {
        int count = 0;

        if( len < 0 ) {
            throw new IndexOutOfBoundsException("len is negative");
        }
        if( off < 0 ) {
            throw new IndexOutOfBoundsException("off is negative");
        }

        while( count < len ) {
            int cur = read(count==0);
            if( cur == -1 ) {
                // end-of-file
                if( count == 0 ) {
                    return -1;
                } else {
                    return count;
                }
            }
            if( cur == WOULD_BLOCK ) {
                Assert._assert(count>0);
                return count;
            }
            Assert._assert( cur >= 0 && cur <= 255);
            b[off+(count++)] = (byte) cur;
        }
        return count;
    }

    public int available() throws IOException {
        // We really don't know how much is left. in.available() could all
        // be whitespace.
        return 0;
    }

    public boolean markSupported() {
        return in.markSupported();
    }

    public void mark(int readlimit) {
        in.mark(readlimit);
        savedPrev = prev;
        savedState = state;
    }

    public void close() throws IOException {
        in.close();
    }

    public void reset() throws IOException {
        in.reset();
        prev = savedPrev;
        state = savedState;
    }

    public static void main(String args[]) throws Exception {

        String infile = args[0];
        String b64file = infile.concat(".b64");
        String newfile = infile.concat(".recov");

        FileInputStream fis = new FileInputStream(infile);
        ByteArrayOutputStream origStream = new ByteArrayOutputStream();
        ByteArrayOutputStream b64OStream = new ByteArrayOutputStream();

        Base64OutputStream b64Stream = new Base64OutputStream(
            new PrintStream(b64Ostream), 18);

        int numread;
        byte []data = new byte[1024];
        while( (numread = fis.read(data, 0, 1024)) != -1 ) {
            origStream.write(data, 0, numread);
            b64Stream.write(data, 0, numread);
        }

        b64Stream.close();
        origStream.close();

        Base64InputStream bis = new Base64InputStream(
            new ByteArrayInputStream(b64OStream.toByteArray()));

        ByteArrayOutputStream newStream = new ByteArrayOutputStream();

        while( (numread = bis.read(data, 0, 1024)) != -1 ) {
            newStream.write(data, 0, numread);
        }

        newStream.close();
        if( ! Arrays.equals(origStream.toByteArray(), newStream.toByteArray())){
            throw new Exception("Did not recover original data");
        }
    }
}
