/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
