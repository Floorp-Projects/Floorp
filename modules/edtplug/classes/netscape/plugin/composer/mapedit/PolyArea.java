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


import netscape.plugin.composer.io.Tag;
import java.awt.*;
import java.util.*;
import java.net.URL;


class PolyArea extends Area {

  /** Should be given a closed polygon, first point is the same as last. */
  private PolyArea(Polygon p,boolean compl,String u) {
      super(u);
      polygon = p;
      isComplete = compl;
      if (!(p.xpoints[p.npoints - 1] == p.xpoints[0] &&
                      p.ypoints[p.npoints - 1] == p.ypoints[0]))
        Debug.println("Internal error: invalid polygon.");
  }

  public Rectangle boundingBox() {
    if (!completed()) {
      Debug.assert(false,"rect not completed");
      return null;
    }

    // upper-left and lower-right points.
    Point ul = new Point(Integer.MAX_VALUE,Integer.MAX_VALUE);
    Point lr = new Point(Integer.MIN_VALUE,Integer.MIN_VALUE);
    for (int n = 0; n < polygon.npoints; n++) {
      ul.x = Math.min(ul.x,polygon.xpoints[n]);
      ul.y = Math.min(ul.y,polygon.ypoints[n]);
      lr.x = Math.max(lr.x,polygon.xpoints[n]);
      lr.y = Math.max(lr.y,polygon.ypoints[n]);
    }

    return new Rectangle(ul.x,ul.y,lr.x - ul.x + 1,lr.y - ul.y + 1);
  }


  static PolyArea firstMouseDown(Point pImage) {
    Polygon p = new Polygon();
    // Initial point.
    p.addPoint(pImage.x,pImage.y);
    // New moving point.
    p.addPoint(pImage.x,pImage.y);
    // Final point is same as initial point.
    p.addPoint(pImage.x,pImage.y);

    // There are always a least three points, i.e. two sides.
    // (p.npoints - 2) is the floating one.

    // not completed
    return new PolyArea(p,false,"");
  }

  void mouseUp(Point pImage,Rectangle clp) {
  }

  void mouseDown(Point pImage,Rectangle clp) {
    if (completed())
      return;

    update(pImage);

    // Duplicate the moving point and add another point.
    polygon.xpoints[polygon.npoints - 1] = polygon.xpoints[polygon.npoints - 2];
    polygon.ypoints[polygon.npoints - 1] = polygon.ypoints[polygon.npoints - 2];
    // New point is initial point to keep polygon closed.
    polygon.addPoint(polygon.xpoints[0],polygon.ypoints[0]);
  }

  void mouseMoved(Point pImage,Rectangle clp) {
    if (completed())
      return;

    update(pImage);
  }

  void mouseDragged(Point pImage,Rectangle clp) {
    if (completed())
      return;

    update(pImage);
  }

  private void update(Point pImage) {
    int n = polygon.npoints - 2;
    if (n < 1) {
      Debug.println("problem with polygon");
    }

    polygon.xpoints[n] = pImage.x;
    polygon.ypoints[n] = pImage.y;
  }

  boolean forceCompletion() {
    if (polygon.npoints > 3) {
      // At least 3 sides to polygon.
      isComplete = true;
      return true;
    }
    else {
      return false;
    }
  }

  boolean completed() {return isComplete;}

  boolean containsPoint(int x,int y) {
    Debug.println("Check point " + x + "," + y);
    Debug.println("polygon " + polygon);

    return polygon.inside(x,y);
  //  return polygon.containsPoint(x,y);
  }

  void draw(Graphics g,Dimension off,boolean selected) {
      drawPolygon(polygon,g,off,selected);
  }

  private static void drawPolygon(Polygon p,Graphics g,Dimension off,boolean selected) {
      // Four polygons, each shifted one pixel in a diagonal,
      // in the color "outer".
      g.setColor(getOuter(selected));
      g.drawPolygon(polygonShift(p,1+off.width,1+off.height));
      g.drawPolygon(polygonShift(p,1+off.width,-1+off.height));
      g.drawPolygon(polygonShift(p,-1+off.width,1+off.height));
      g.drawPolygon(polygonShift(p,-1+off.width,-1+off.height));
      // Polyangle at the real location in the color, "inner".
      g.setColor(getInner(selected));
      g.drawPolygon(polygonShift(p,+off.width,+off.height));
  }

  // Returns a new polygon with all coordinates shifted (dx,dy) from the original
  // polygon.
  private static Polygon polygonShift(Polygon p,int dx,int dy) {
      Polygon ret = new Polygon();
      for (int n = 0; n < p.npoints; n++) {
          ret.addPoint(p.xpoints[n] + dx,p.ypoints[n] + dy);
      }
      return ret;
  }

  void clip(Rectangle clp) {
    // Don't know if it is safe to change the points directly like this.

    for (int n = 0; n < polygon.npoints; n++) {
      polygon.xpoints[n] = Math.max(clp.x,polygon.xpoints[n]);
      polygon.ypoints[n] = Math.max(clp.y,polygon.ypoints[n]);
      polygon.xpoints[n] = Math.min(clp.x + clp.width,polygon.xpoints[n]);
      polygon.ypoints[n] = Math.min(clp.y + clp.height,polygon.ypoints[n]);
    }
  }

  boolean moveBy(int dx,int dy,Rectangle clp) {
    // Check to see if we can move the polygon.
    for (int n = 0; n < polygon.npoints; n++) {
      if (polygon.xpoints[n] + dx < clp.x) {
        dx = Math.min(clp.x - polygon.xpoints[n],0);
      }
      else if (polygon.xpoints[n] + dx >= clp.x + clp.width) {
        dx = Math.max(clp.x + clp.width - (polygon.xpoints[n] + 1),0);
      }

      if (polygon.ypoints[n] + dy < clp.y) {
        dy = Math.min(clp.y - polygon.ypoints[n],0);
      }
      else if (polygon.ypoints[n] + dy >= clp.y + clp.height) {
        dy = Math.max(clp.y + clp.height - (polygon.ypoints[n] + 1),0);
      }
    }

    if (dx != 0 || dy != 0) {
      polygon = polygonShift(polygon,dx,dy);
      return true;
    }

    // actually move the polygon
/*    for (int n = 0; n < polygon.npoints; n++) {
        polygon.xpoints[n] += dx;
        polygon.ypoints[n] += dy;
    } */
    // java.awt.Polygon doesn't seem to work right if you change the
    // points directly.  Polygon.containsPoint messes up.

    return false;
  }

  boolean resizeBy(int dx,int dy,Rectangle clp) {
    // We could do something like expand the entire polygon.
    // Just ignore it.
    return false;
  }

  // Return a new Tag with all the information in the PolyArea.
  Tag toTag() {
      Tag tag = new Tag("area");
      tag.addAttribute("shape","polygon");
      StringBuffer coords = new StringBuffer();
      // Ignore last point, it is the same as the first.
      for (int n = 0; n < polygon.npoints - 1; n++) {
          if (n > 0)
              coords.append(",");
          coords.append("" + polygon.xpoints[n] + ","
                          + polygon.ypoints[n]);
      }

      tag.addAttribute("coords",coords.toString());
      String href = getURL().toString();
      if (href.length() > 0) {
        tag.addAttribute("href",href);
      }
      return tag;
  }

  static PolyArea fromTag(Tag tag,URL base) {
    if (tag.getName().equals("AREA") &&
//        tag.containsAttribute("HREF") &&
        tag.containsAttribute("COORDS")) {

      // If ANYTHING goes wrong in parsing, we just ignore the tag.
      try {
        String shape = tag.lookupAttribute("SHAPE");

        if (shape.equalsIgnoreCase("POLY") ||
            shape.equalsIgnoreCase("POLYGON")) {
          String coords = tag.lookupAttribute("COORDS");

          StringTokenizer tok = new StringTokenizer(coords," ,");
          Polygon poly = new Polygon();
          // Add as many points to polygon as possible.
          try {
            while (true) {
              poly.addPoint(Integer.parseInt(tok.nextToken()),
                            Integer.parseInt(tok.nextToken()));
            }
          } catch (java.util.NoSuchElementException e) {}

          // Don't bother if not at least a triangle.
          if (poly.npoints < 3)
            return null;

          // Add trailing point that is same as starting point.
          poly.addPoint(poly.xpoints[0],poly.ypoints[0]);

          // a completed polygon.
          return new PolyArea(poly,true,get_href(tag,base));
        }
      }
      catch (Throwable t) {}
    }
    return null;
  }


  private boolean isComplete;

  /** The actual size/location of the PolyArea.
   A closed polygon, first point is same as last. */
  private Polygon polygon;
}
