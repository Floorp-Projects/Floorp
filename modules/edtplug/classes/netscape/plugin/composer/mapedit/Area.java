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
import java.net.URL;
import netscape.plugin.composer.io.Tag;

/** Everything for one <AREA> tag */
abstract class Area {
    Area(String u) {url = u;}

    String getURL() {return url;}
    void setURL(String s) {url = s;}

    abstract void draw(Graphics g,Dimension offset,boolean selected);
    abstract Tag toTag();
    abstract boolean containsPoint(int x,int y);

    /** Minimum rectangle that will contain the Area. */
    abstract Rectangle boundingBox();

    /* For creating an area. */
    /* Send in a clipped Point in image coordinates. */
    abstract void mouseUp(Point pImage,Rectangle clp);
    abstract void mouseDown(Point pImage,Rectangle clp);
    abstract void mouseMoved(Point pImage,Rectangle clp);
    abstract void mouseDragged(Point pImage,Rectangle clp);
    /** Force area to be completed in current state. Return whether
    successful or not. */
    abstract boolean forceCompletion();

    abstract boolean completed();

    /** Clip the area to be in given rectangle. */
    /* Need to do something if completely clipped away. */
    abstract void clip(Rectangle r);

    /** Move the area by (dx,dy) pixels, staying within rectangle.
    true if successful, false and don't change the Area if not possible. */
    abstract boolean moveBy(int dx,int dy,Rectangle r);

    /** Resize the area by (dx,dy) pixels, staying within given rectangle
        true if successful, false and don't change
        the Area if not possible.
        Only one of {dx,dy} will be non-zero, different Areas will interpret
        this differently. */
    abstract boolean resizeBy(int dx,int dy,Rectangle r);


        static protected String get_href(Tag tag,URL base) {
      String hrefAttr = tag.lookupAttribute("HREF");
      if (hrefAttr == null) {
        return "";
      }
      try {
        URL hrefURL = new URL(base,hrefAttr);
        return hrefURL.toString();
      } catch (Exception e) {
        return "";
      }
        }

    static Color getInner(boolean selected) {
        return selected ? Color.yellow : Color.white;
    }

    static Color getOuter(boolean selected) {
        //return selected ? Color.red : Color.black;
        return selected ? new Color(255,0,0) : new Color(0,0,0);
    }

  private String url;
}


