/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

package netscape.test.plugin.composer;

import java.util.*;

public class DocInfoResources extends ListResourceBundle {

    /**
     * Overrides ListResourceBundle
     */
    public Object[][] getContents() {
        return contents;
    }

    static final Object[][] contents = {
        // Non-Localizable Key                  // localize these strings
        {"title", "Document Statistics"},
        {"base", "Base URL: ^0\n"},
        {"workDir", "Working Directory: ^0\n"},
        {"workDirURL", "Working Directory URL: ^0\n"},
        {"eventName", "Event Name: ^0\n"},
        {"documentURL", "Document URL: ^0\n"},
        {"chars", "Number of characters in the document: ^0\n"},
        {"words", "Number of words in the document: ^0\n"},
        {"images", "Number of images in the document: ^0\n"},
        {"totalImageBytes", "Total size in bytes of the images: ^0\n"},
        {"totalBytes", "Total size in bytes of the whole document: ^0\n"},
        {"min14400", "Minimum time to download at 14.4 Kbaud: ^0 seconds\n"},
        {"name", "Document Info"},
        {"category", "HTML Tools"},
        {"hint", "Display information about the document."},
        {"ok", "OK"}
        // stop localizing
    };
}
