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

/**
 * This class keeps track of the number of bytes that have been read from
 * a stream. It will be incremented by the number of bytes read or skipped.
 * If the stream is marked and then reset, the number of bytes read will
 * be reset as well.
 */
class CountingStream extends InputStream {

    private int count=0;
    private int markpos;
    private InputStream source;

    private static final boolean DEBUG = false;

    private CountingStream() { }

    public CountingStream(InputStream source) {
        this.source = source;
    }

    public int available() throws IOException {
        return source.available();
    }

    public void mark(int readlimit) {
        source.mark(readlimit);
        markpos = count;
        if(DEBUG) {
            System.out.println("Marked at position "+markpos);
        }
    }

    public boolean markSupported() {
        return source.markSupported();
    }

    public int read() throws IOException {
        int n = source.read();
        if( n != -1 ) {
            count++;
            if(DEBUG) {
                System.out.println("read() 1 byte, count="+count);
            }
        }
        return n;
    }

    public int read(byte[] buffer) throws IOException {
        int n = source.read(buffer);
        if( n != -1 ) {
            count += n;
        }
        if(DEBUG) {
            System.out.println("read([]) "+n+" bytes, count="+count);
        }
        return n;
    }

    public int read(byte[] buffer, int offset, int count) throws IOException {
        int n = source.read(buffer, offset, count);
        if( n != -1 ) {
            this.count += n;
        }
        if(DEBUG) {
            System.out.println("read(...) "+n+" bytes, count="+this.count);
        }
        return n;
    }

    public void reset() throws IOException {
        source.reset();
        if(DEBUG) {
            System.out.println("reset from "+count+" to "+markpos);
        }
        count = markpos;
    }

    public long skip(long count) throws IOException {
        this.count += count;
        if(DEBUG) {
            System.out.println("skipped "+count+", now at "+this.count);
        }
        return source.skip(count);
    }

    public int getNumRead() {
        return count;
    }

    public void resetNumRead() {
        count = 0;
        markpos = 0;
        if(DEBUG) {
            System.out.println("resetting count to 0");
        }
    }
}
