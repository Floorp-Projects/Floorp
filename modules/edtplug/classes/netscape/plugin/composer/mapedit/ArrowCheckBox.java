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
