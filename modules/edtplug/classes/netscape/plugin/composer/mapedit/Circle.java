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

public class Circle {
    Circle(int xx,int yy,int rr) {x = xx; y = yy; radius = rr;}

    Rectangle boundingBox() {
        // Rectangle centered at (x,y).
        Rectangle r = new Rectangle(x - radius,y - radius,2 * radius + 1,2 * radius + 1);
        return r;
    }

    /** Circle acts like a closed disk.  I.e. boundary is included. */
    boolean containsPoint(int xx,int yy) {
        return dist(x,y,xx,yy) <= radius;
    }

    /** Standard Euclidean distance. */
    static double dist(Point p1,Point p2) {
        return dist(p1.x,p1.y,p2.x,p2.y);
    }

    void moveBy(int dx,int dy) {
        x += dx;
        y += dy;
    }

    static double dist(int x1,int y1,int x2,int y2) {
        int dx = x2 - x1;
        int dy = y2 - y1;
        return Math.sqrt(dx * dx + dy * dy);
    }

    // Center.
    int x;
    int y;
    int radius;
}


