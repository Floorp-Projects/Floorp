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
 * Contributor(s):
 *   Patrick C. Beard <beard@netscape.com>
 *   Xiaobin Lu <Xiaobin.Lu@eng.Sun.com>
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

import java.net.URLClassLoader;
import java.net.URL;
import java.net.MalformedURLException;

import java.io.*;
import java.util.zip.*;
import java.util.WeakHashMap;

import java.security.CodeSource;

public abstract class ProxyClassLoaderFactory {
    static void debug(String message) {
        System.out.println("<<< " + message + " >>>");
    }

    /**
     * Loads specified class file as an array of bytes from ${netscape.oji.plugin.home}/MRJPlugin.jar.
     */
    private static byte[] getMRJPluginClassFile(String path) {
        try {
    		String homeDir = System.getProperty("netscape.oji.plugin.home");
    		ZipFile jarFile = new ZipFile(new File(homeDir, "MRJPlugin.jar"));
    		ZipEntry classEntry = jarFile.getEntry(path);
    		int size = (int) classEntry.getSize();
    		if (size > 0) {
    			byte[] data = new byte[size];
    			DataInputStream input = new DataInputStream(jarFile.getInputStream(classEntry));
    			input.readFully(data);
    			input.close();
    			jarFile.close();
    			return data;
    		}
        } catch (IOException ioe) {
        }
		return null;
    }
    
    /**
     * Trivial subclass of URLClassLoader that predefines the class netscape.oji.LiveConnectProxy
     * as if it were loaded from the specified codebase URL passed to the constructor.
     * @see        netscape.oji.LiveConnectProxy
     */
    private static class ProxyClassLoader extends URLClassLoader {
        private static byte[] data = getMRJPluginClassFile("netscape/oji/LiveConnectProxy.class");
        ProxyClassLoader(URL[] documentURLs) {
            super(documentURLs);
            if (data != null) {
                Class proxyClass = defineClass("netscape.oji.LiveConnectProxy",
                                               data, 0, data.length,
                                               new CodeSource(documentURLs[0], null));
                debug("ProxyClassLoader: defined LiveConnectProxy class.");
                debug("Here're the permisssions you've got:");
                debug(proxyClass.getProtectionDomain().getPermissions().toString());
            } else {
                debug("ProxyClassLoader: failed to define LiveConnectProxy class.");
            }
        }
    }
    
    // XXX  Should this be a weak table of some sort?
    private static WeakHashMap mClassLoaders = new WeakHashMap();

    public static ClassLoader createClassLoader(final String documentURL) throws MalformedURLException {
        ClassLoader loader = (ClassLoader) mClassLoaders.get(documentURL);
        if (loader == null) {
            try {
                URL[] documentURLs = new URL[] { new URL(documentURL) };
                loader = new ProxyClassLoader(documentURLs);
                mClassLoaders.put(documentURL, loader);
    		} catch (MalformedURLException e) {
            }
        }
        return loader;
    }
    
    public static void destroyClassLoader(final String documentURL) {
        mClassLoaders.remove(documentURL);
    }
}
