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

package netscape.plugin.composer.frameEdit;

import java.util.*;

/** The localizable strings for the FrameEditor Composer Plug-in.
 * These are the menus and some miscelaneous strings. The dialog strings are
 * in the Constructor .plan file.
 */

public class FrameEditorResourceBundle extends ListResourceBundle {

    /**
     * Overrides ListResourceBundle
     */
    public Object[][] getContents() {
        return contents;
    }

    static final Object[][] contents = {
        // Non-Localizable Key                  // localize these strings
        // FrameEdit
        {"font name", "Helvetica"},

        // FrameEditApp
        {"main title", "Frame Editor"},
        {"document title", "Document"},
        {"file", "File"},
        {"save", "OK"},
        {"cancel", "Cancel"},
        {"apply", "Apply"},
        {"edit", "Edit"},
        {"undo", "Undo"},
        {"split -", "Split -"},
        {"split |", "Split |"},
        {"delete", "Delete"},
        {"navigate", "Select"},
        {"navRoot", "Top"},
        {"navParent", "Parent"},
        {"navPrev", "Previous sibling"},
        {"navNext", "Next sibling"},
        {"navChild", "First child"},

        {"helpMenu", "Help"},
        {"help", "About FrameEdit..."},
        {"help title", "About FrameEdit"},

        // FrameModel
        {"new frame name", "Frame "},
        // PlainDocumentView
        {"no frames", "No frames in this document."},

        // FrameEdit
        {"name", "Frame Editor"},
        {"category", "HTML Tools"},
        {"hint", "Edit the frames of a document."},
        {"about", "A sample composer plugin."},

        // stop localizing
    };
}
