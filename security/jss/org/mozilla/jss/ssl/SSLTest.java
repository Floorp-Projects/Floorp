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

import java.net.*;
import java.io.*;
import org.mozilla.jss.CryptoManager;
import java.util.*;

public class SSLTest {

    public static void main(String[] args) {
        new SSLTest(args);
    }

    private Hashtable params = new Hashtable();

    private String[] defaults = {
        "port", "443",
        "host", "www.amazon.com",
        "remotehost", "www.amazon.com"
    };

    private void initParams() {
        processArgs(defaults);
    }

    private void processArgs(String[] args) {
        int i;

        for(i=0; i < args.length; i+=2) {
            System.out.flush();
            params.put(args[i], args[i+1]);
        }
    }

    private void dumpParams() {
        Enumeration _enum = params.keys();
        System.out.println("Parameters:");
        while (_enum.hasMoreElements() ) {
            String key = (String) _enum.nextElement();
            System.out.println(key + "=" + (String)params.get(key));
        }
    }

    public SSLTest(String[] args) {
      try {

        initParams();
        processArgs(args);
        dumpParams();
        CryptoManager.initialize(".");

        int port = (new Integer( (String) params.get("port") )).intValue();

        Socket s = new Socket((String)params.get("host"), port);

        SSLSocket ss = new SSLSocket(s, (String)params.get("remotehost"),
            null, null);

        ss.setSoTimeout(5000);

        OutputStream os = ss.getOutputStream();
        String writeString = "GET / HTTP/1.0\n\n";
        byte[] writeBytes = writeString.getBytes("8859_1");
        os.write(writeBytes);

        InputStream is = ss.getInputStream();
        int numRead = 0;
        byte[] inbuf = new byte[256];
        while( (numRead = is.read(inbuf)) != -1 ) {
            System.out.print( new String(inbuf, 0, numRead, "UTF-8"));
        }

        ss.setKeepAlive(true);
        ss.setReceiveBufferSize(32000);
        ss.setSendBufferSize(8000);
        ss.setSoLinger(true, 10);
        ss.setTcpNoDelay(true);

        System.out.println("remote addr is " + ss.getInetAddress().toString());
        System.out.println("remote port is " + ss.getPort());
        System.out.println("local addr is " + ss.getLocalAddress().toString());
        System.out.println("local port is " + ss.getLocalPort());
        System.out.println("keepalive is " + ss.getKeepAlive());
        System.out.println("receive buffer size is " + ss.getReceiveBufferSize());
        System.out.println("send buffer size is " + ss.getSendBufferSize());
        System.out.println("solinger is " + ss.getSoLinger());
        System.out.println("sotimeout is " + ss.getSoTimeout());
        System.out.println("tcpNoDelay is " + ss.getTcpNoDelay());

        ss.shutdownInput();
        ss.shutdownOutput();

        ss.close();

      } catch(Exception e) {
            e.printStackTrace();
      }
      try {
        Runtime.getRuntime().gc();
      }catch(Exception e) {
        e.printStackTrace();
      }
    }
}
