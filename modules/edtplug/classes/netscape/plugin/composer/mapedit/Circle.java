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


