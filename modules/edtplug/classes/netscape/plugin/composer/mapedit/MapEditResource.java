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

// Internationalization.
package netscape.plugin.composer.mapedit;

import java.util.*;

/*class MapEditLoader extends ResourceLoader {
    public Object handleLoad(String name) {
        try {
            return Class.forName(name).newInstance();
        } catch (Exception e) {
            return null;
        }
    }
}*/



// Default, English.
public class MapEditResource extends ListResourceBundle {
  public Object[][]getContents() {
    return contents;
  }

  static final Object[][]contents= {
    // Non-Localizable Key                          // localize these strings
    {"frame_title",                         "Image Map Editor"},
    {"no_images_in_document",               "No images in this document."},
    {"ok",                                  "Ok"},
    {"map_editor_error",                    "Map Editor Error"},
    {"first_img_invalid", "First <IMG> tag in document is invalid."},
    {"invalid_img_tag", "Invalid or no SRC specified for image %0."},  // A MessageFormat
    {"error_reading_twice",    "Internal editor plugin error:\nreading Document twice gives different token sequence."},
    {"could_not_load_image",  "Could not load image %0."}, // A MessageFormat
    {"done", "Done"},
    {"cancel","Cancel"},
    {"select", "Select"},
    {"rectangle","Rectangle"},
    {"circle", "Circle"},
    {"polygon", "Polygon"},
    {"html_tools", "HTML Tools"},
    {"image_map_editor", "Image Map Editor"},
    {"the_hint", "Edit first image map in selection or in document"},
    {"url","URL:"},
    {"doc_is_null","Internal error: document is null."},
    {"area_list","Area List"},
    {"unspecified_area","<unspecified>"},
// stop localizing
  };
}


