/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is part of the mozilla.org LDAP XPCOM SDK.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Xiaobin Lu <Xiaobin.Lu@eng.Sun.com>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

package netscape.oji;
import java.net.URLClassLoader;
import java.net.URL;
import java.net.MalformedURLException;

public abstract class ProxyClassLoaderFactory {

    public static ClassLoader createClassLoader(final String documentURL) 
                  throws MalformedURLException {
        URL[] documentURLArray = new URL[1];
        int lastIndx = documentURL.lastIndexOf("/");
        String urlPath = documentURL.substring(0, lastIndx+1);
        try {
            documentURLArray[0] = new URL(urlPath);
		}
        catch (MalformedURLException e) {
            System.out.println("MalformedURLException was caught");
            return null;
        }
        return new URLClassLoader(documentURLArray);
    }
}
