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

public class CircleArea extends Area {
  static CircleArea firstMouseDown(Point pImage) {
      // start out uncompleted
      return new CircleArea(new Circle(pImage.x,pImage.y,0),false,"");
  }

  private CircleArea(Circle c,boolean compl,String u) {
      super(u);
      circle = c;
      isComplete = compl;
  }

  public Rectangle boundingBox() {
    return new Rectangle(circle.x - circle.radius,circle.y - circle.radius,
                         2 * circle.radius + 1,2 * circle.radius + 1);
  }

  void mouseUp(Point pImage,Rectangle clp) {
    if (completed())
      return;
    update(pImage,clp);

    // Complete the CircleArea
    isComplete = true;
  }

  void mouseDown(Point pImage,Rectangle clp) {
    if (completed())
      return;

    update(pImage,clp);
  }

  void mouseMoved(Point pImage,Rectangle clp) {
    if (completed())
      return;

    update(pImage,clp);
  }

  void mouseDragged(Point pImage,Rectangle clp) {
    if (completed())
      return;

    update(pImage,clp);
  }

  boolean forceCompletion() {
    isComplete = true;
    return true;
  }

  boolean completed() {return isComplete;}

  /** Change radius of circle to intersect p. */
  private void update(Point p,Rectangle clp) {
    int newRadius = (int)Circle.dist(p.x,p.y,circle.x,circle.y);
    int maxRadius = Math.min(Math.min(circle.x - clp.x,circle.y - clp.y),
                             Math.min(clp.x + clp.width - circle.x,
                                      clp.y + clp.height - circle.y));
    // Should >= 0.
    maxRadius = Math.max(maxRadius,0);
    circle.radius = Math.min(newRadius,maxRadius);
  }


  boolean containsPoint(int x,int y) {return circle.containsPoint(x,y);}

  void clip(Rectangle clp) {
    /// Need to implement this for circle.
  }

  boolean moveBy(int dx,int dy,Rectangle clp) {
    if (circle.x + dx - circle.radius < clp.x) {
      dx = Math.min(clp.x - (circle.x - circle.radius),0);
    }
    else if (circle.x + dx + circle.radius >= clp.x + clp.width) {
      dx = Math.max(clp.x + clp.width - (circle.x + circle.radius + 1),0);
    }
    if (circle.y + dy - circle.radius < clp.y) {
      dy = Math.min(clp.y - (circle.y - circle.radius),0);
    }
    else if (circle.y + dy + circle.radius >= clp.y + clp.height) {
      dy = Math.max(clp.y + clp.height - (circle.y + circle.radius + 1),0);
    }

    if (dx != 0 || dy != 0) {
      circle.moveBy(dx,dy);
      return true;
    }

    return false;
  }

  boolean resizeBy(int dx,int dy,Rectangle clp) {
    int rDelta = dx + dy;

    if (circle.x - (circle.radius + rDelta) >= clp.x &&
        circle.y - (circle.radius + rDelta) >= clp.y &&
        circle.x + circle.radius + rDelta < clp.x + clp.width &&
        circle.y + circle.radius + rDelta < clp.y + clp.height) {
      circle.radius += rDelta;

      // Clamp radius >= 1
      circle.radius = Math.max(circle.radius,1);
      return true;
    }
    else {
      return false;
    }
  }

  void draw(Graphics g,Dimension off,boolean selected) {
      // Two Circles, one pixel inside and outside, respectively, in the color,
      // "outer".
      Rectangle rect = circle.boundingBox();
      g.setColor(getOuter(selected));
      g.drawOval(rect.x-1+off.width,rect.y-1+off.height,
                  rect.width+2,rect.height+2);
      g.drawOval(rect.x+1+off.width,rect.y+1+off.height,
                  rect.width-2,rect.height-2);
      // Circle at the real location in the color, "inner".
      // Draw last so on top.
      g.setColor(getInner(selected));
      g.drawOval(rect.x+off.width,rect.y+off.height,
                  rect.width,rect.height);
  }


  // Return a new Tag with all the information in the CircleArea.
  Tag toTag() {
      Tag tag = new Tag("area");
      tag.addAttribute("shape","circle");
      tag.addAttribute("coords",
              "" + circle.x + "," + circle.y + "," + circle.radius);
      String href = getURL().toString();
      if (href.length() > 0) {
        tag.addAttribute("href",getURL().toString());
      }
      return tag;
  }


  static CircleArea fromTag(Tag tag,URL base) {
    if (tag.getName().equals("AREA") &&
//        tag.containsAttribute("HREF") &&
        tag.containsAttribute("COORDS")) {

      // If ANYTHING goes wrong in parsing, we just ignore the tag.
      try {
        String shape = tag.lookupAttribute("SHAPE");

        if (shape.equalsIgnoreCase("CIRCLE")) {
          String coords = tag.lookupAttribute("COORDS");
          StringTokenizer tok = new StringTokenizer(coords," ,");
          int x1 = Integer.parseInt(tok.nextToken());
          int y1 = Integer.parseInt(tok.nextToken());
          int r = Integer.parseInt(tok.nextToken());
          Circle circle = new Circle(x1,y1,r);
          // starts out completed
          return new CircleArea(circle,true,get_href(tag,base));
        }
      }
      catch (Throwable t) {}
    }
    return null;
  }

  /** If non-null, used for creation. */
  private boolean isComplete;

  /** The actual size/location of the CircleArea. */
  private Circle circle;
}
