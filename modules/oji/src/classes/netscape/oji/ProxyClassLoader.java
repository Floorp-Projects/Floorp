/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):  Patrick Beard <beard@netscape.com>
 */

package netscape.oji;

import java.net.*;
import java.io.*;

public class ProxyClassLoader extends ClassLoader {
    private URL mCodebaseURL;

    /**
     * Creates a class loader for the specified document URL.
     */
    private ProxyClassLoader(String documentURL) throws MalformedURLException {
        mCodebaseURL = new URL(documentURL);
    }
    
    private byte[] download(InputStream input) throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream(16 * 1024);
        byte[] buffer = new byte[4096];
        int count = input.read(buffer);
        while (count > 0) {
            output.write(buffer, 0, count);
            count = input.read(buffer);
        }
        input.close();
        return output.toByteArray();
    }
    
    protected  Class loadClass(String name, boolean resolve) throws ClassNotFoundException {
		Class c = findLoadedClass(name);
	    if (c == null) {
            try {
                String path = name.replace('.', '/') + ".class";
                URL classURL = new URL(mCodebaseURL, path);
                byte[] data = download(classURL.openStream());
                c = defineClass(data, 0, data.length);
            } catch (MalformedURLException mfue) {
            } catch (IOException ioe) {
            }
            if (c == null)
				c = findSystemClass(name);
        }
        if (resolve)
            resolveClass(c);
        return c;
    }
}
