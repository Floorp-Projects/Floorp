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

package netscape.plugin.composer.mapedit;


import java.awt.*;
import java.util.Vector;


public class IconCheckBoxGroup {
  void stateChanged(IconCheckBox box,boolean state) {
    // Prevent recursion.
    if (changingState) {
      return;
    }
    changingState = true;

    int index = items.indexOf(box);
    Debug.assert(index != -1,"bad index");

    if (index == active) {
      // Active box is trying to turn itself off.
      // I don't think so...
      if (state == false) {
        box.setState(true);
      }
    }
    else if (state == true) {
      // Some other box is turned on, turn off formerly active one.
      try {
        IconCheckBox activeBox = (IconCheckBox)items.elementAt(active);
        activeBox.setState(false);
        active = index;
      } catch (ArrayIndexOutOfBoundsException e) {}
    }

    changingState = false;
  }


  // Called by the IconCheckBox when it is created.
  void add(IconCheckBox box) {
    items.addElement(box);
    if (box.getState()) {
      Debug.assert(active == -1,"more than one active box");
      active = items.size() - 1; // The box just added.
    }
  }


  // Index of the active CheckBox.
  int active = -1;
  Vector items = new Vector(); // of IconCheckBox

  boolean changingState = false;
}
