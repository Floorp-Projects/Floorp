/* ----- BEGIN LICENSE BLOCK -----
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
 * The Original Code is the MRJ Carbon OJI Plugin.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):  Patrick C. Beard <beard@netscape.com>
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
 * ----- END LICENSE BLOCK ----- */

package netscape.oji;

import java.io.*;
import java.awt.*;
import java.awt.event.*;
import java.util.Date;
import java.util.Properties;
import java.net.URL;

/**
 * Performs startup actions on behalf of the MRJ plugin.
 * 1. Sends System.out/err to a specified disk file.
 * 2. Installs an appropriate security manager for
 *    integrating with the Netscape 6 security system.
 */
public class MRJSession {
	// Save primordial System streams.
	private static PrintStream out;
	private static PrintStream err;
	private static PrintStream console;

    private static Properties loadProperties(String pluginHome) {
        Properties props = new Properties();
        try {
            InputStream propsStream = new FileInputStream(pluginHome + "/MRJPlugin.properties");
            props.load(propsStream);
            propsStream.close();
        } catch (IOException ex) {
        }
        return props;
    }

    public static void open(String consolePath) throws IOException {
        String pluginHome = System.getProperty("netscape.oji.plugin.home");
        Properties props = loadProperties(pluginHome);
        boolean append = Boolean.valueOf(props.getProperty("netscape.oji.plugin.console.append")).booleanValue();
    
        // redirect I/O to specified file.
		MRJSession.out = System.out;
		MRJSession.err = System.err;
        console = new PrintStream(new FileOutputStream(consolePath, append));
		System.setOut(console);
		System.setErr(console);

        Date date = new Date();
        String version = props.getProperty("netscape.oji.plugin.version");
        System.out.println("MRJ Plugin for Mac OS X v" + version);
        System.out.println("[starting up Java Applet Security @ " + date + "]");

        // bring up MRJ Applet Security.
        if (System.getSecurityManager() == null) {
            try {
                // make sure that the classes in MRJPlugin.jar are granted all permissions.
                // see p. 117 of "Inside Java 2 Platform Security" for more information.
                System.setProperty("java.security.policy", "file:" + pluginHome + "/MRJPlugin.policy");
                String name = props.getProperty("netscape.oji.plugin.security");
                SecurityManager securityManager = (SecurityManager) Class.forName(name).newInstance();
                System.setSecurityManager(securityManager);
            } catch (Exception ex) {
            }
        }
    }

    public static void close() throws IOException {
		System.setOut(MRJSession.out);
		System.setErr(MRJSession.err);
        console.close();
    }
}
