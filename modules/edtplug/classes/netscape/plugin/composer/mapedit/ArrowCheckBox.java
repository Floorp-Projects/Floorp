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
import java.util.*; // For ResourceBundle, etc.

public class ArrowCheckBox extends IconCheckBox {
  public ArrowCheckBox(IconCheckBoxGroup grp,boolean state,OkCallback cb) {
    super(grp,state,cb);
  }

  public void paint(Graphics g) {
    Rectangle r = g.getClipRect();
    g.setColor(Color.lightGray);
    g.fill3DRect(r.x,r.y,r.width,r.height,!getState());

    Polygon p = new Polygon();
    p.addPoint(6,2);
    p.addPoint(6,13);
    p.addPoint(9,10);
    p.addPoint(9,11);
    p.addPoint(11,16);
    p.addPoint(12,16);
    p.addPoint(10,11);
    p.addPoint(10,10);
    p.addPoint(13,10);
    p.addPoint(13,9);
    p.addPoint(6,2);

    g.setColor(Color.blue);
    g.fillPolygon(p);
  }
}
