/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package netscape.test.plugin.composer;

import java.util.*;

public class IFCTestBundle extends ListResourceBundle {

    /**
     * Overrides ListResourceBundle
     */
    public Object[][] getContents() {
        return contents;
    }

    static final Object[][] contents = {
        // Non-Localizable Key                  // localize these strings
        {"name", "Edit HTML (IFC)"},
        {"category", "HTML Tools"},
        {"hint", "Edit the HTML text of the document. Implemented using IFC."},
        {"title", "Edit HTML (using Internet Foundation Classes)"},

        {"ok", "OK"},
        {"cancel", "Cancel"},
        {"apply", "Apply"}
        // stop localizing
    };
}
