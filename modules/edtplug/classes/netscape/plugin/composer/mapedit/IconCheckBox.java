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
