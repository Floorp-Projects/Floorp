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
import java.util.*;

public class InfoDialog extends Dialog {
    protected Button button;
    protected MultiLineLabel label;
    protected OkCallback okCB;

    public InfoDialog(Frame parent, ResourceBundle res, String title, String message, OkCallback ok)
    {
        // Create a dialog with the specified title
        super(parent, title, true);
        Debug.println("InfoDialog.InfoDialog()");

        okCB = ok;

        // Create and use a BorderLayout manager with specified margins
        this.setLayout(new BorderLayout(15, 15));

        // Create the message component and add it to the window
        label = new MultiLineLabel(message, 20, 20);
        this.add("Center", label);

        // Create an Okay button in a Panel; add the Panel to the window
        // Use a FlowLayout to center the button and give it margins.
        button = new Button(res.getString("ok"));
        Panel p = new Panel();
        p.setLayout(new FlowLayout(FlowLayout.CENTER, 15, 15));
        p.add(button);
        this.add("South", p);

        // Resize the window to the preferred size of its components
        this.pack();
//        resize(300,200);
    }

    // Pop down the window when the button is clicked.
    public boolean action(Event e, Object arg)
    {
      Debug.println("InfoDialog.action(): " + e);
      if (e.target == button) {
            this.hide();
//            this.dispose();
            if (okCB != null)
              okCB.ok(); // Call callback.
            return true;
        }
        else return false;
    }

    // When the window gets the keyboard focus, give it to the button.
    // This allows keyboard shortcuts to pop down the dialog.
    public boolean gotFocus(Event e, Object arg) {
        button.requestFocus();
        return true;
    }
}

class MultiLineLabel extends Canvas {
    public static final int LEFT = 0; // Alignment constants
    public static final int CENTER = 1;
    public static final int RIGHT = 2;
    protected String[] lines;         // The lines of text to display
    protected int num_lines;          // The number of lines
    protected int margin_width;       // Left and right margins
    protected int margin_height;      // Top and bottom margins
    protected int line_height;        // Total height of the font
    protected int line_ascent;        // Font height above baseline
    protected int[] line_widths;      // How wide each line is
    protected int max_width;          // The width of the widest line
    protected int alignment = LEFT;   // The alignment of the text.

    // This method breaks a specified label up into an array of lines.
    // It uses the StringTokenizer utility class.
    protected void newLabel(String label) {
        StringTokenizer t = new StringTokenizer(label, "\n");
        num_lines = t.countTokens();
        lines = new String[num_lines];
        line_widths = new int[num_lines];
        for(int i = 0; i < num_lines; i++) lines[i] = t.nextToken();
    }

    // This method figures out how the font is, and how wide each
    // line of the label is, and how wide the widest line is.
    protected void measure() {
        FontMetrics fm = this.getFontMetrics(this.getFont());
        // If we don't have font metrics yet, just return.
        if (fm == null) return;

        line_height = fm.getHeight();
        line_ascent = fm.getAscent();
        max_width = 0;
        for(int i = 0; i < num_lines; i++) {
            line_widths[i] = fm.stringWidth(lines[i]);
            if (line_widths[i] > max_width) max_width = line_widths[i];
        }
    }

    // Here are four versions of the cosntrutor.
    // Break the label up into separate lines, and save the other info.
    public MultiLineLabel(String label, int margin_width, int margin_height,
                  int alignment) {
        newLabel(label);
        this.margin_width = margin_width;
        this.margin_height = margin_height;
        this.alignment = alignment;
    }
    public MultiLineLabel(String label, int margin_width, int margin_height) {
        this(label, margin_width, margin_height, LEFT);
    }
    public MultiLineLabel(String label, int alignment) {
        this(label, 10, 10, alignment);
    }
    public MultiLineLabel(String label) {
        this(label, 10, 10, LEFT);
    }

    // Methods to set the various attributes of the component
    public void setLabel(String label) {
        newLabel(label);
        measure();
        repaint();
    }
    public void setFont(Font f) {
        super.setFont(f);
        measure();
        repaint();
    }
    public void setForeground(Color c) {
        super.setForeground(c);
        repaint();
    }
    public void setAlignment(int a) { alignment = a; repaint(); }
    public void setMarginWidth(int mw) { margin_width = mw; repaint(); }
    public void setMarginHeight(int mh) { margin_height = mh; repaint(); }
    public int getAlignment() { return alignment; }
    public int getMarginWidth() { return margin_width; }
    public int getMarginHeight() { return margin_height; }

    // This method is invoked after our Canvas is first created
    // but before it can actually be displayed.  After we've
    // invoked our superclass's addNotify() method, we have font
    // metrics and can successfully call measure() to figure out
    // how big the label is.
    public void addNotify() { super.addNotify(); measure(); }

    // This method is called by a layout manager when it wants to
    // know how big we'd like to be.
    public Dimension preferredSize() {
        return new Dimension(max_width + 2*margin_width,
                     num_lines * line_height + 2*margin_height);
    }

    // This method is called when the layout manager wants to know
    // the bare minimum amount of space we need to get by.
    public Dimension minimumSize() {
        return new Dimension(max_width, num_lines * line_height);
    }

    // This method draws the label (applets use the same method).
    // Note that it handles the margins and the alignment, but that
    // it doesn't have to worry about the color or font--the superclass
    // takes care of setting those in the Graphics object we're passed.
    public void paint(Graphics g) {
        int x, y;
        Dimension d = this.size();
        y = line_ascent + (d.height - num_lines * line_height)/2;
        for(int i = 0; i < num_lines; i++, y += line_height) {
            switch(alignment) {
            case LEFT:
                x = margin_width; break;
            case CENTER:
            default:
                x = (d.width - line_widths[i])/2; break;
            case RIGHT:
                x = d.width - margin_width - line_widths[i]; break;
            }
            g.drawString(lines[i], x, y);
        }
    }
}
