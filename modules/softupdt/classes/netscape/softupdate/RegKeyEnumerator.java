/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package netscape.softupdate;

import java.util.Enumeration;
import java.util.NoSuchElementException;
import netscape.security.PrivilegeManager;

/**
 * Helper class used to enumerate Registry subkeys
 * @see RegistryNode#subkeys
 */
final class RegKeyEnumerator implements Enumeration {
    private String       path = "";
    private Registry     reg = null;
    private int          key = 0;
    private int          state = 0;
    private int          style = 0;
    private String       target;

    RegKeyEnumerator(Registry registry, int startKey, int enumStyle, String targ)
    {
        reg   = registry;
        key   = startKey;
        style = enumStyle;
        target = targ;
    }

    public boolean hasMoreElements() {
        PrivilegeManager.checkPrivilegeEnabled( target );
        String  tmp = regNext(false);

        return (tmp != null);
    }

    public Object nextElement() {
        PrivilegeManager.checkPrivilegeEnabled( target );
        path = regNext(true);
        if (path == null) {
            throw new NoSuchElementException("RegKeyEnumerator");
        }
        return path;
    }

    private native String regNext(boolean skip);
}


