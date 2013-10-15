// Mozilla has modified this file - see http://hg.mozilla.org/ for details.
/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.mozilla.apache.commons.codec.binary;

import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * Abstract superclass for Base-N input streams.
 * 
 * @since 1.5
 */
public class BaseNCodecInputStream extends FilterInputStream {

    private final boolean doEncode;

    private final BaseNCodec baseNCodec;

    private final byte[] singleByte = new byte[1];

    protected BaseNCodecInputStream(InputStream in, BaseNCodec baseNCodec, boolean doEncode) {
        super(in);
        this.doEncode = doEncode;
        this.baseNCodec = baseNCodec;
    }

    /**
     * Reads one <code>byte</code> from this input stream.
     * 
     * @return the byte as an integer in the range 0 to 255. Returns -1 if EOF has been reached.
     * @throws IOException
     *             if an I/O error occurs.
     */
    public int read() throws IOException {
        int r = read(singleByte, 0, 1);
        while (r == 0) {
            r = read(singleByte, 0, 1);
        }
        if (r > 0) {
            return singleByte[0] < 0 ? 256 + singleByte[0] : singleByte[0];
        }
        return -1;
    }

    /**
     * Attempts to read <code>len</code> bytes into the specified <code>b</code> array starting at <code>offset</code>
     * from this InputStream.
     * 
     * @param b
     *            destination byte array
     * @param offset
     *            where to start writing the bytes
     * @param len
     *            maximum number of bytes to read
     * 
     * @return number of bytes read
     * @throws IOException
     *             if an I/O error occurs.
     * @throws NullPointerException
     *             if the byte array parameter is null
     * @throws IndexOutOfBoundsException
     *             if offset, len or buffer size are invalid
     */
    public int read(byte b[], int offset, int len) throws IOException {
        if (b == null) {
            throw new NullPointerException();
        } else if (offset < 0 || len < 0) {
            throw new IndexOutOfBoundsException();
        } else if (offset > b.length || offset + len > b.length) {
            throw new IndexOutOfBoundsException();
        } else if (len == 0) {
            return 0;
        } else {
            int readLen = 0;
            /*
             Rationale for while-loop on (readLen == 0):
             -----
             Base32.readResults() usually returns > 0 or EOF (-1).  In the
             rare case where it returns 0, we just keep trying.

             This is essentially an undocumented contract for InputStream
             implementors that want their code to work properly with
             java.io.InputStreamReader, since the latter hates it when
             InputStream.read(byte[]) returns a zero.  Unfortunately our
             readResults() call must return 0 if a large amount of the data
             being decoded was non-base32, so this while-loop enables proper
             interop with InputStreamReader for that scenario.
             -----
             This is a fix for CODEC-101
            */
            while (readLen == 0) {
                if (!baseNCodec.hasData()) {
                    byte[] buf = new byte[doEncode ? 4096 : 8192];
                    int c = in.read(buf);
                    if (doEncode) {
                        baseNCodec.encode(buf, 0, c);
                    } else {
                        baseNCodec.decode(buf, 0, c);
                    }
                }
                readLen = baseNCodec.readResults(b, offset, len);
            }
            return readLen;
        }
    }
    /**
     * {@inheritDoc}
     * 
     * @return false
     */
    public boolean markSupported() {
        return false; // not an easy job to support marks
    }

}
