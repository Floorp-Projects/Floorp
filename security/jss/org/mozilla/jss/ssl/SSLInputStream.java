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
 * Portions created by the Initial Developer are Copyright (C) 2001
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

package org.mozilla.jss.ssl;

import java.io.IOException;

class SSLInputStream extends java.io.InputStream {

    SSLInputStream(SSLSocket sock) {
        this.sock = sock;
    }

    public int available() throws IOException {
        return sock.socketAvailable();
    }

    public void close() throws IOException {
        sock.close();
    }

    public int read() throws IOException {
        byte[] b = new byte[1];
        int nread = read(b, 0, 1);
        if( nread == -1 ) {
            return nread;
        } else {
            return ((int) b[0]) & (0xff);
        }
    }

    public int read(byte[] b) throws IOException {
        return read(b, 0, b.length);
    }

    public int read(byte[] b, int off, int len) throws IOException {
        return sock.read(b, off, len);
    }

    public long skip(long n) throws IOException {
        long numSkipped = 0;

        int size = (int) (n < 2048 ? n : 2048);
        byte[] trash = new byte[size];
        while( n > 0) {
            size = (int) (n < 2048 ? n : 2048);
            int nread = read(trash, 0, size);
            if( nread <= 0 ) {
                break;
            }
            numSkipped += nread;
            n -= nread;
        }
        return numSkipped;
    }

    private SSLSocket sock;
}
