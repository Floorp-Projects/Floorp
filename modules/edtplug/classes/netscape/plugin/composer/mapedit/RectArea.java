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

public class RectArea extends Area {
    static RectArea firstMouseDown(Point pImage) {
        // start out uncompleted
        return new RectArea(new Rectangle(pImage.x,pImage.y,0,0),pImage,"");
    }

    private RectArea(Rectangle r,Point pPrev,String u) {
        // Create a completed RectArea.
        super(u);

        rectangle = r;
        if (pPrev != null) {
          // copy it.
          prev = new Point(pPrev.x,pPrev.y);
        }
        else {
          prev = null;
        }
    }

    public Rectangle boundingBox() {
      // Return a copy.
      return new Rectangle(rectangle.x,rectangle.y,rectangle.width,rectangle.height);
    }

    void mouseUp(Point pImage,Rectangle clp) {
      if (completed())
        return;
      update(prev,pImage);

      // Complete the RectArea
      prev = null;
    }

    void mouseDown(Point pImage,Rectangle clp) {
      if (completed())
        return;

      update(prev,pImage);
    }

    void mouseMoved(Point pImage,Rectangle clp) {
      if (completed())
        return;

      update(prev,pImage);
    }

    void mouseDragged(Point pImage,Rectangle clp) {
      if (completed())
        return;

      update(prev,pImage);
    }

    void clip(Rectangle c) {
      Rectangle r = rectangle;

      int right = r.x + r.width;
      int bottom = r.y + r.height;

      r.x = Math.max(c.x,r.x);
      r.y = Math.max(c.y,r.y);
      right = Math.min(right,c.x + c.width);
      bottom = Math.min(bottom,c.y + c.height);
      r.width = right - r.x;
      r.height = bottom - r.y;
      if (r.width <= 0 || r.height <= 0) {
        Debug.println("RectArea.clip(): invalid rect");
      }
    }

    boolean moveBy(int dx,int dy,Rectangle clp) {
      if (rectangle.x + dx < clp.x) {
        dx = Math.min(clp.x - rectangle.x,0);
      }
      else if (rectangle.x + dx + rectangle.width > clp.x + clp.width) {
        dx = Math.max(clp.x + clp.width - (rectangle.x + rectangle.width),0);
      }
      if (rectangle.y + dy < clp.y) {
        dy = Math.min(clp.y - rectangle.y,0);
      }
      else if (rectangle.y + dy + rectangle.height > clp.y + clp.height) {
        dy = Math.max(clp.y + clp.height - (rectangle.y + rectangle.height),0);
      }


      if (dx != 0 || dy !=0) {
        rectangle.translate(dx,dy);
        return true;
      }

      return false;
    }

    boolean resizeBy(int dx,int dy,Rectangle clp) {
      if (rectangle.width + dx > 0 &&
          rectangle.height + dy > 0 &&
          rectangle.x + rectangle.width + dx <= clp.x + clp.width &&
          rectangle.y + rectangle.height + dy <= clp.y + clp.height) {
        rectangle.width += dx;
        rectangle.height += dy;
        return true;
      }
      else {
        return false;
      }
    }

    private void update(Point p1,Point p2) {
      rectangle.x = Math.min(p1.x,p2.x);
      rectangle.y = Math.min(p1.y,p2.y);
      rectangle.width = Math.abs(p2.x - p1.x) + 1;
      rectangle.height = Math.abs(p2.y - p1.y) + 1;
    }

    boolean forceCompletion() {
        prev = null;
        return true;
    }

    boolean completed() {return prev == null;}

    boolean containsPoint(int x,int y) {
      return rectangle.inside(x,y);  // jdk 1.1
    }

    void draw(Graphics g,Dimension off,boolean selected) {
        // rectangle at the real location in the color, "inner".
        g.setColor(getInner(selected));
        g.drawRect(rectangle.x+off.width,rectangle.y+off.height,
                   rectangle.width,rectangle.height);
        // Two rectangles, one pixel inside and outside, respectively, in the color,
        // "outer".
        g.setColor(getOuter(selected));
        g.drawRect(rectangle.x-1+off.width,rectangle.y-1+off.height,
                  rectangle.width+2,rectangle.height+2);
        g.drawRect(rectangle.x+1+off.width,rectangle.y+1+off.height,
                  rectangle.width-2,rectangle.height-2);
    }

    // Return a new Tag with all the information in the RectArea.
    Tag toTag() {
        Tag tag = new Tag("area");
        tag.addAttribute("shape","rect");
        tag.addAttribute("coords",
                "" + rectangle.x + "," + rectangle.y + "," +
                (rectangle.x + rectangle.width - 1) + "," +
                (rectangle.y + rectangle.height - 1));
        String href = getURL().toString();
        if (href.length() > 0) {
          tag.addAttribute("href",href);
        }
        return tag;
    }


  static RectArea fromTag(Tag tag,URL base) {
    if (tag.getName().equals("AREA") &&
//        tag.containsAttribute("HREF") &&
        tag.containsAttribute("COORDS")) {

      // If ANYTHING goes wrong in parsing, we just ignore the tag.
      try {
        String shape = tag.lookupAttribute("SHAPE");

        if (shape == null || shape.equalsIgnoreCase("RECT")) {
          String coords = tag.lookupAttribute("COORDS");
          StringTokenizer tok = new StringTokenizer(coords," ,");
          int x1 = Integer.parseInt(tok.nextToken());
          int y1 = Integer.parseInt(tok.nextToken());
          int x2 = Integer.parseInt(tok.nextToken());
          int y2 = Integer.parseInt(tok.nextToken());
          Rectangle rectangle = new Rectangle(Math.min(x1,x2),
                    Math.min(y1,y2),
                    Math.abs(x2 - x1) + 1,
                    Math.abs(y2 - y1) + 1);
          return new RectArea(rectangle,null,get_href(tag,base));
        }
      }
      catch (Exception e) {}
    }
    return null;
  }

  /** If non-null, used for creation. */
  private Point prev;

  /** The actual size/location of the RectArea. */
  private Rectangle rectangle;
}


