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


