/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

package netscape.plugin.composer.mapedit;


import netscape.plugin.composer.io.Tag;
import java.awt.*;
import java.util.*;
import java.net.URL;


class DefaultArea extends Area {
  DefaultArea(String url) {super(url);}


  void draw(Graphics g,Dimension offset,boolean selected) {}

  // Return a new Tag with just the url.
  Tag toTag() {
    Tag tag = new Tag("area");
    tag.addAttribute("shape","default");
    tag.addAttribute("href",getURL().toString());
    return tag;
  }

  boolean containsPoint(int x,int y) {return false;}
  Rectangle boundingBox() {return null;}

  void mouseUp(Point pImage,Rectangle clp) {}
  void mouseDown(Point pImage,Rectangle clp) {}
  void mouseMoved(Point pImage,Rectangle clp) {}
  void mouseDragged(Point pImage,Rectangle clp) {}

  boolean forceCompletion() {return true;}

  boolean completed() {return true;}

  void clip(Rectangle r) {}

  boolean moveBy(int dx,int dy,Rectangle r) {return false;}

  boolean resizeBy(int dx,int dy,Rectangle r) {return false;}

  static DefaultArea fromTag(Tag tag,URL base) {
    if (tag.getName().equals("AREA") &&
        tag.containsAttribute("HREF")) {

      // If ANYTHING goes wrong in parsing, we just ignore the tag.
      try {
        String shape = tag.lookupAttribute("SHAPE");

        if (shape.equalsIgnoreCase("DEFAULT")) {
          URL href = new URL(base,
                             tag.lookupAttribute("HREF"));
          // a completed polygon.
          return new DefaultArea(href.toString());
        }
      }
      catch (Throwable t) {}
    }
    return null;
  }

  static void draw(Graphics g,Dimension off,Rectangle rectangle,boolean selected) {
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
}
