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


abstract public class IconCheckBox extends Canvas {
  public IconCheckBox(IconCheckBoxGroup g,boolean s,OkCallback cb) {
    state = s;
    grp = g;
    g.add(this);
    okCB = cb;
  }

  public boolean getState() {
    return state;
  }

  public Dimension preferredSize() {
    return new Dimension(WIDTH,HEIGHT);
  }

  public Dimension minimumSize() {
    return new Dimension(WIDTH,HEIGHT);
  }

  public void setState(boolean val) {
    if (val != state) {
      state = val;
      if (state) {
        // call callback.
        okCB.ok();
      }
      if (grp != null) {
        grp.stateChanged(this,val);
      }
      repaint();
    }
  }

  public boolean mouseDown(Event evt, int x, int y) {
    setState(!state);
    return true;
  }

  private boolean state;
  private IconCheckBoxGroup grp;
  private OkCallback okCB;

  protected final static int WIDTH = 20;
  protected final static int HEIGHT = 20;
}
