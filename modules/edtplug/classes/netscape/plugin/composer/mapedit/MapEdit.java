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
import java.applet.*;
import java.awt.image.*;

import java.net.URL;
import java.io.*;
import netscape.plugin.composer.*;
import netscape.plugin.composer.io.*;
import java.util.*; // For ResourceBundle, etc.

import netscape.security.PrivilegeManager;



/* ****************** The Plugin *************************************************/
public class MapEdit extends Plugin {
  public static void main(String []args) {
    Debug.println("in MapEdit.main()" + args);

    netscape.test.plugin.composer.Test.perform(args,new MapEdit());
  }

  /** Returns whether resources are now loaded. */
  boolean loadResources() {
    if (!resLoaded) {
      // Load resources. international.
      try {
        Debug.println("Loading resources");

// This method not supported in the Navigator yet.
//        res = ResourceBundle.getResourceBundle("netscape.test.plugin.composer.MapEditResource",null);
        res = new MapEditResource();
        resLoaded = true;
      }
      catch (MissingResourceException e) {
      }
    }
    return resLoaded;
  }

  private boolean resLoaded = false;
  private ResourceBundle res = null;


  public String getName() {
    if (loadResources())
      return res.getString("image_map_editor");
    else
      return "Image Map Editor";
  }

  public String getCategory() {
    if (loadResources())
      return res.getString("html_tools");
    else
      return "HTML Tools";
  }

  public String getHint() {
    if (loadResources())
      return res.getString("the_hint");
    else
      return super.getHint();
  }

  public boolean perform(Document document) throws IOException {
    ////// --------------------------------////
    Debug.println("Image Map Editor test 8");
    ////// --------------------------------////

    if (!loadResources()) {
      System.err.println("Map Editor could not load resources.");
      return false;
    }
    MapEditApp app = new MapEditApp(document,res);

    // Catch any errors in init().
    if (app.initSuccessful()) {
      app.show();
    }

    boolean ret = app.waitForExit();

    // Let Cafe users see the output before the window goes away.
    if (false) {
//    if (Debug.debug()) {
      try {
        if (true) {
          Debug.println("Hit return");
          System.in.read();
        }
        else {
          Debug.println("SLEEP for a few seconds");
          Thread.sleep(10000);
        }
      }
      catch (Exception e) {}
    }

    return ret;
  }
}



/* ********************* The actual Application ***************/
class MapEditApp extends Frame implements OkCallback {
  private boolean initOk = false;
  public boolean initSuccessful() {return initOk;}

  public MapEditApp(Document d,ResourceBundle rB) {
    super(rB.getString("frame_title"));
    res = rB;
    document = d;
    areas = new Vector();

    if (document == null) {
      failure(res.getString("doc_is_null"));
      return;
    }

    // Try to find the first image in the selection.
    if (!parseDoc(true)) {
      // Next, look for any image in the document.
      if (!parseDoc(false)) {
        // Give up, no images.
        failure(res.getString("no_images_in_document"));
        return;
      }
    }
    // imageName, imageLocation, and mapName should now be valid.


    // Security stuff
    PrivilegeManager.getPrivilegeManager().enablePrivilege("UniversalFileAccess");
    PrivilegeManager.getPrivilegeManager().enablePrivilege("UniversalConnect");

    Debug.println("got privileges");

    // Load image
    URL imageURL = null;
    try {
        imageURL = new URL(document.getBase(),imageName);
    } catch (java.net.MalformedURLException e) {
      failure(e.toString());
      return;
    }

    Debug.println("created dummy URL");

    try {
        image = Toolkit.getDefaultToolkit().getImage(imageURL);
    }
    catch (Exception e) {
        failure(e.toString());
        return;
    }
    Debug.println("start image load");

    // Force image loading.
    MediaTracker tracker = new MediaTracker(this);
    tracker.addImage(image,0);
    try {
      tracker.waitForID(0);
    } catch (InterruptedException e) {
       image = null;
    }
    if (image != null) {
      imageDim = new Dimension(image.getWidth(this),image.getHeight(this));
      imageRect = new Rectangle(imageDim);
    }
    if (image == null || imageDim.width < 0 || imageDim.height < 0) {
      failure(Format.formatLookup(res,"could_not_load_image",imageName));
      return;
    }

    Debug.println("finish image load");

    createUI();
    syncAreaList();
    resize(MapEditApp.APP_WIDTH,MapEditApp.APP_HEIGHT);
    initOk = true;

    // Flag so we can stop running the app.
    isRunning = true;
  }


  synchronized void failure(String message) {
    if (failureDialog == null)
      failureDialog = new InfoDialog(this,res,"Map Editor Error",message,this);
    if (!failureDialog.isVisible())
      failureDialog.show();

    Debug.println("FAILURE:" + message);
  }
  private InfoDialog failureDialog = null;



  // Callback from the failure InfoDialog.  Quit the application.
  public synchronized void ok() {
    Debug.println("ok callback called");
    if (isRunning) {
     // success will be false.
     stopRunning();
    }
  }


  synchronized public boolean waitForExit() {
    while ( isRunning ) {
      try {
          Debug.println("MapEditApp.waitForExit is wait()ing.");
          wait();
      } catch ( InterruptedException e){
      }
    }
    hide();
    return success;
  }


  synchronized void stopRunning() {
    Debug.println("stop running");
    isRunning = false;
    notifyAll();
  }





  /*********************** PARSING/UNPARSING CODE *********************/

  // Find first image in selection or in document.
  // Sets imageName, mapName, imageLocation.
  // Returns whether an image was found.
  private boolean parseDoc(boolean onlyInSelection) {
    // Start from beginning of document.
    LexicalStream inStream = null;
    try {
      // Read entire document into a string and use the string
      // for the LexicalStream.
      Reader in = document.getInput();
      StringBuffer b = new StringBuffer();
      int bufSize = 1000;
      char[] buf = new char[bufSize];
      for(;;) {
        int moved = in.read(buf);
        if ( moved < 0 )
            break;
        b.append(buf, 0, moved);
      }
      in.close();
      inStream = new LexicalStream(b.toString());
    } catch (IOException e) {
      failure(e.toString());
    }
    imageLocation = -1;

    boolean inSelection = false;

    Token token;
    try {
      while ((token = inStream.next()) != null) {
        imageLocation++; // 0 for the first token.

        // Keep track of selection start/end.
        if (token instanceof Comment) {
          Comment comment = (Comment)token;
          if (!inSelection && comment.isSelectionStart())
            inSelection = true;
          if (inSelection && comment.isSelectionEnd())
            inSelection = false;
        }

        // Find first IMG tag.
        if ((!onlyInSelection || inSelection) &&
            token instanceof Tag) {
          Tag tag = (Tag)token;
          if (tag.getName().equals("IMG")) {
            // Image found.

            if (!tag.isOpen() || !tag.containsAttribute("SRC")) {
              failure(res.getString("first_img_invalid"));
            }

            String value = tag.lookupAttribute("USEMAP");
            // If image has an image map, read in it's <area> tags and
            // use its name.
            if (value != null && value.length() > 0) {
              mapName = StripPound(value);
              parseMap();
            }
            // Else, a new map, generate a map name.
            else {
              // NSMAP + 6 digits.
              mapName = "NSMAP" + (int)Math.floor((Math.random() * 999999));
            }

            imageName = tag.lookupAttribute("SRC");
            if (imageName == null || imageName.length() == 0) {
//              failure(new MessageFormat(res.getString("invalid_img_tag")).format(args));
              failure(Format.formatLookup(res,"invalid_img_tag",imageName));
            }

            return true;
          } // if "IMG"
        } // if Tag
      } // while
    } catch (IOException e) {
      failure(e.toString());
    }

    return false;
  }


  /** Search from beginning of doc, find first map with name mapName. Use it
     to construct the Vector areas. */
  private void parseMap() {
    // Note: Starting from the beginning.
    LexicalStream stream = null;
    try {
      stream = new LexicalStream(document.getInput());
    } catch (IOException e) {
      failure(e.toString());
    }
    Token t;
    try {
      while ((t = stream.next()) != null) {
        if (t instanceof Tag) {
          Tag tag = (Tag)t;
          if (tag.getName().equals("MAP") && tag.isOpen() &&
            mapName.equals(tag.lookupAttribute("NAME"))) {
            parseAreas(stream);
          }
        }
      }
    } catch (IOException e) {
      failure(e.toString());
    }

    // If we don't find an image map corresponding to mapName, areas will just
    // be an empty Vector.
  }


  /** Read in all <AREA> tags until a closing </map> tag is hit. */
  private void parseAreas(LexicalStream stream) {
    try {
      Token token;
      while ((token = stream.next()) != null) {
        if (token instanceof Tag) {
          Tag tag = (Tag)token;
          if (tag.getName().equals("MAP") && tag.isClose())
            return;

          // Give each Area class a chance to parse the tag.
          Area area;
          if ((area = DefaultArea.fromTag(tag,document.getBase())) != null) {
            // We could make the default area a real area, for now just store the
            // default URL.
            defaultURL.str = area.getURL();
            continue;
          }
          if ((area = RectArea.fromTag(tag,document.getBase())) != null) {
//            area.clip(imageRect);
            areas.addElement(area);
            continue;
          }
          if ((area = CircleArea.fromTag(tag,document.getBase())) != null) {
//            area.clip(imageRect);
            areas.addElement(area);
            continue;
          }
          if ((area = PolyArea.fromTag(tag,document.getBase())) != null) {
//            area.clip(imageRect);
            areas.addElement(area);
            continue;
          }
        } // if
      } // while
    } catch (IOException e) {
      failure(e.toString());
    }
  }


  /** Strips leading '#' if found. */
  private String StripPound(String in) {
    if (in.startsWith("#"))
      return in.substring(1);
    else
      return in;
  }


  /** Write document to the Document output stream. */
  private void writeDoc() {
    // Start from beginning of document.
    LexicalStream inStream = null;
    Writer outStream = null;
    try {
      inStream = new LexicalStream(document.getInput());
      outStream = document.getOutput();
    } catch (IOException e) {
      failure(e.toString());
    }

    // Between <map> and </map> of the <map> corresponding to the first image.
    boolean insideMap = false;

    int location = -1; // so is 0 for first token.

    Token token;
    try {
      while ((token = inStream.next()) != null) {
        // At top so gets incremented even if we "continue" inside body of "while".
        location++;

        if (token instanceof Tag) {
          Tag tag = (Tag)token;

          // Change IMG tag to use new map.  May just set it to the prev value.
          if (location == imageLocation) {

            if (!tag.getName().equals("IMG")) {
              failure(res.getString("error_reading_twice"));
            }

            // Only write out if at least one area or the default area.
            if (!areas.isEmpty() || defaultURL.notEmpty()) {
              tag.addAttribute("USEMAP","#" + mapName);
              outStream.write(tag.toString() + "\n");
              // Write the image map right after the image.
              writeMap(outStream);
            }
            // If no areas, don't output image map.
            else {
              // Remove USEMAP if it was there.
              tag.removeAttribute("USEMAP");
              outStream.write(tag.toString());
            }
            continue;
          }

          // Eliminate beginning <map> corresponding to the first image.
          if (!insideMap &&
              tag.getName().equals("MAP") &&
              tag.isOpen() &&
              mapName.equals(tag.lookupAttribute("NAME"))) {
            insideMap = true;
            continue;
          }

          // Eliminate trailing </map>
          if (insideMap &&
              tag.getName().equals("MAP") &&
              tag.isClose()) {
            insideMap = false;
            continue;
          }

          // Kill all <area> tags between <map> and </map>
          if (insideMap &&
              tag.getName().equals("AREA")) {
            continue;
          }
        } // Tag

        outStream.write(token.toString());
      } // while

      outStream.close();
    } catch (IOException e) {
      if (outStream != null)
        try {outStream.close();} catch (IOException e2) {}
      failure(e.toString());
    }
  }


  private void writeMap(Writer out) throws IOException {
      Tag map = new Tag("MAP");
      map.addAttribute("NAME",mapName);

      out.write(map.toString() + "\n");
        Enumeration e = areas.elements();
        while (e.hasMoreElements()) {
            Tag area = ((Area)e.nextElement()).toTag();
            out.write(area.toString() + "\n");
        }
      // Write default area last.
      if (defaultURL.notEmpty()) {
        out.write(new DefaultArea(defaultURL.str).toTag().toString() + "\n");
      }
      out.write(new Tag("MAP",false).toString() + "\n");
  }


  /***************** END PARSING/UNPARSING CODE ******************************/


  /************************** UI CODE ************************************/

  static private final boolean areaListOnDefault = false;

  private void createUI() {
    setLayout(new BorderLayout());

    header = new Panel();
    header.setBackground(Color.lightGray);
    IconCheckBoxGroup grp = new IconCheckBoxGroup();
    header.setLayout(new FlowLayout(FlowLayout.LEFT,3,4));

    header.add(arrowCheckBox =
                new ArrowCheckBox(grp,mode == SELECT,
                new OkCallback() {public void ok() { select();} }));
    header.add(new RectCheckBox(grp,mode == CREATE_RECT_AREA,
                new OkCallback() {public void ok() { createArea(CREATE_RECT_AREA);} }));
    header.add(new CircleCheckBox(grp,mode == CREATE_CIRCLE_AREA,
                new OkCallback() {public void ok() { createArea(CREATE_CIRCLE_AREA);} }));
    header.add(new PolyCheckBox(grp,mode == CREATE_POLY_AREA,
                new OkCallback() {public void ok() { createArea(CREATE_POLY_AREA);} }));

    Label l = new Label(res.getString("url"),Label.RIGHT);
    header.add(l);
    urlComponent = new TextField(40);
    urlComponent.setEditable(true);
    urlComponent.setText(new String(defaultURL.str));
    header.add(urlComponent);
    header.add(areaListButton = new Checkbox(res.getString("area_list"),null,areaListOnDefault));
    add("North",header);

    canvas = new BlankCanvas(this);
    canvas.setBackground(Color.white);
    add("Center",canvas);

    areaList = new SizedList();
    add("East",areaList);
    if (!areaListOnDefault) {
      areaList.hide();
    }

    doneButton = new Button(res.getString("done"));
    cancelButton = new Button(res.getString("cancel"));
    Panel footer = new Panel();
    footer.setBackground(Color.lightGray);
    footer.setLayout(new FlowLayout(FlowLayout.CENTER, 30, 9));
    footer.add(doneButton);
    footer.add(cancelButton);
    add("South",footer);
  }
  // offset is vector from canvas origin to image origin.
  private Dimension offset = new Dimension(0,0);

  private BlankCanvas canvas;
  private Panel header;
  private TextComponent urlComponent;
  private Button doneButton;
  private Button cancelButton;
  private Checkbox areaListButton;
  private List areaList;
  private IconCheckBox arrowCheckBox;


  // Keep image centered if canvas is bigger than image.
  private static int reshapeHelper(int canvasSize,int imageSize,int offset) {
    int offsetNew;
    if (canvasSize > imageSize) {
      offsetNew = (canvasSize - imageSize) / 2;
    }
    else {
      offsetNew = Math.min(offset,0);
    }
    return offsetNew;
  }

  /** The image canvas has been reshaped to this width and height. */
  void reshapeCanvas(int width,int height) {
    offset.width = reshapeHelper(width,imageDim.width,offset.width);
    offset.height = reshapeHelper(height,imageDim.height,offset.height);
  }

  public synchronized void paintMe(Graphics g) {
//    Debug.println("painting");

    g.drawImage(image,offset.width,offset.height,this);

    // Draw region for default.
    DefaultArea.draw(g,offset,imageRect,selected == -1);

    // Draw in reverse order, so that lowest Area is on top.
    // First area in list takes precedence for selection.
    for (int index = areas.size() - 1; index >= 0; index--) {
      Area a = (Area)areas.elementAt(index);
      a.draw(g,offset,index == selected);
    }
  }

  // Use <unspecified> instead of empty or null string.
  private String listString(Object o) {
    String url = ((Area)o).getURL();
    if (url != null && url.length() > 0) {
      return url;
    }
    else {
      return res.getString("unspecified_area");
    }
  }

  private void syncAreaList() {
    areaList.clear();
    Enumeration e = areas.elements();
    while (e.hasMoreElements()) {
      areaList.addItem(listString(e.nextElement()));
    }
    if (defaultURL.notEmpty()) {
      areaList.addItem("> " + defaultURL.str);
    }
    selectAreaList(selected);
  }


  private void selectAreaList(int val) {
    // Just to be sure, unselect all.  MAC won't do it for you.
    int current = areaList.getSelectedIndex();
    if (current != -1) {
       areaList.deselect(current);
    }


    if (val == -1) {
      if (defaultURL.notEmpty()) {
        Debug.assert(areas.size() + 1 == areaList.countItems(),"list not synced");

        // select last item.
        areaList.select(areas.size());
      }
      else {
        Debug.assert(areas.size() == areaList.countItems(),"list not synced");
      }
    }
    else {
      areaList.select(val);
    }
    Debug.println("selectAreaList: val was " + val + " List had" + current + " List is now " + areaList.getSelectedIndex());
  }



  /*  ************** Helper code for dealing with areas ************** */

  private void setSelected(int n) {
    setSelected(n,false);
  }

  // fromAreaList prevents recursion with the areaList
  private void setSelected(int n,boolean fromAreaList) {
    Debug.println("setSelected " + n);

    if (selected < -1 || selected >= areas.size()) {
      Debug.println("Attempting to select invalid index.");
      return;
    }

    // first copy current value from the UI text element to the internal
    // representation.
    flushURLComponent();

    if (completing()) {
      Debug.println("MapEditApp.select() when not done with prev operation.");
      // Don't allow changing the selection while completing an area.
      return;
    }

    selected = n;


    // Update UI.
    if (n == -1)
      urlComponent.setText(new String(defaultURL.str));
    else
      urlComponent.setText(((Area)areas.elementAt(n)).getURL().toString());

    // Make areaList match current selection.
    if (!fromAreaList) {
      selectAreaList(selected);
    }
  }

  /** Are we in the process of completing an area. */
  private boolean completing() {
      // Something is selected and it is not fully created.
      return (selected != -1 && !((Area)areas.elementAt(selected)).completed());
  }

  private void deleteSelected() {
    deleteSelected(false);
  }

  private void deleteSelected(boolean fromAreaList) {
    Debug.println("Deleted selected = " + selected);

    if (selected < -1 || selected >= areas.size()) {
      Debug.println("Attempting to delete with invalid index.");
      return;
    }

    if (selected == -1) {
      if (defaultURL.notEmpty()) {
        // Just clear out the current string.
        defaultURL.str = "";
      }
    }
    else {
      // This should remove the currently selected element.
      areas.removeElementAt(selected);
    }
    selected = -1;
    urlComponent.setText(new String(defaultURL.str));
    syncAreaList();
  }


  // Copy current value from the UI text element to the internal
  // representation.
  private void flushURLComponent() {
    String url = urlComponent.getText();
    Debug.println("FLUSH, component has <" +
    ((url == null) ? "-NULL-" : url) + ">");

    if (selected != -1) {
      Area area = (Area)areas.elementAt(selected);
      // Only change if necessary.
      if (!area.getURL().equals(url)) {
        ((Area)areas.elementAt(selected)).setURL(new String(url));
        syncAreaList();
      }
    }
    else {
      // Set default url
      if (!defaultURL.str.equals(url)) {
        defaultURL.str = new String(url);
        syncAreaList();
      }
    }
  }


  /* *************** End helper code ******************** */

  // Translate an index from an AWT event to the index for the
  // Vector.  -1 means the extraString was selected.
  public int translateEventIndex(int val) {
    if (val >= areas.size()) {
      return -1;
    }
    return val;
  }

  public synchronized boolean handleEvent(Event e) {
    if (e.id == Event.WINDOW_DESTROY) {
      cancel();
      return true;
    }
    else if (e.target == doneButton) {
      if (e.id == Event.ACTION_EVENT) {
        done();
        return true;
      }
    }
    else if (e.target == cancelButton) {
      if (e.id == Event.ACTION_EVENT) {
        cancel();
        return true;
      }
    }
    // Show/hide areaList.
    else if (e.target == areaListButton) {
      if (e.id == Event.ACTION_EVENT) {
        boolean val = ((Boolean)e.arg).booleanValue();
        if (val) {
          areaList.show();
        }
        else {
          areaList.hide();
        }
        validate();
        return true;
      }
    }
    else if (e.target == areaList) {
    // Select according to the areaList.
      if (e.id == Event.LIST_SELECT) {
        int val = ((Integer)e.arg).intValue();
        int translated = translateEventIndex(val);

        Debug.println("LIST_SELECT event received, evt " + val + ", translate " + translated);

        setSelected(translated,true);

        canvas.repaint();
        return true;
      }
      // Delete selected area.
      if (e.id == Event.KEY_PRESS &&
          (e.key == Event.BACK_SPACE)) {
      // Not Event.DELETE, because it produces "?" on the MAC.

        Debug.println("DELETE in area list event");

        deleteSelected(true);
        canvas.repaint();
        return true;
      }
    }
    // Flush the text field into the area.
    else if (e.target == urlComponent) {
      if (e.id == Event.ACTION_EVENT || e.id == Event.LOST_FOCUS) {
        flushURLComponent();
      }
    }
    else if (e.target == this && (e.id == Event.KEY_PRESS || e.id == Event.KEY_ACTION)) {
      // Cafe 1.53 sends key events here.
      return keyDown(e);
    }

    return super.handleEvent(e);
  }


  synchronized boolean keyDown(Event e) {
    Debug.println("keyDown: " + e.key);
    checkKillDoubleClick(e);

    // Move/delete selected
    if (selected != -1) {
      if (e.key == Event.BACK_SPACE) {
      // Not Event.DELETE, because it produces "?" on the MAC.
          deleteSelected();
          canvas.repaint();
          return true;
      }

      boolean arrow = false;
      int dx = 0;
      int dy = 0;
      if (e.key == Event.UP) {
          dy = -1;
          arrow = true;
      }
      else if (e.key == Event.RIGHT) {
          dx = 1;
          arrow = true;
      }
      else if (e.key == Event.DOWN) {
          dy = 1;
          arrow = true;
      }
      else if (e.key == Event.LEFT) {
          dx = -1;
          arrow = true;
      }
      if (arrow) {
        if (e.modifiers != 0) {
          // Don't care about return value.
          ((Area)areas.elementAt(selected)).resizeBy(dx,dy,imageRect);
        }
        else {
          // Don't care about return value.
          ((Area)areas.elementAt(selected)).moveBy(dx,dy,imageRect);
        }
        canvas.repaint();
        return true;
      }
    }

    // Force completion, delete area if failure.
    if (e.key == ' ') {
      if (completing()) {
        if (! ((Area)areas.elementAt(selected)).forceCompletion() ) {
          deleteSelected();
          canvas.repaint();
          return true;
        }
        // Finished completing an area, go back to selection mode.
        if (((Area)areas.elementAt(selected)).completed()) {
          // This will call MapEditApp.select()
          arrowCheckBox.setState(true);
        }
      }
    }

    return false;
  }

  // p is in image coordinates
  private int areaAt(Point p) {
    // Find first area that contains p.
    for (int n = 0; n < areas.size(); n++) {
        if (((Area)areas.elementAt(n)).containsPoint(p.x,p.y)) {
            return n;
        }
    }
    return -1;
  }

/* Clip p (in image coords) to the image rectangle. */
  private void clipImage(Point p) {
    p.x = Math.max(p.x,0);
    p.y = Math.max(p.y,0);
    p.x = Math.min(p.x,imageDim.width);
    p.y = Math.min(p.y,imageDim.height);
  }

  synchronized boolean mouseUp(Event e) {
    Debug.println("mouseUp");
    Point pImage = new Point(e.x - offset.width,e.y - offset.height);


    if (completing()) {
      clipImage(pImage);
      // In the process of completing an area, let the area handle it.
      ((Area)areas.elementAt(selected)).mouseUp(pImage,imageRect);

      // Finished completing an area, go back to selection mode.
      if (((Area)areas.elementAt(selected)).completed()) {
        // This will call MapEditApp.select()
        arrowCheckBox.setState(true);
      }
    }

    canvas.repaint();
    return true;
  }


  Point doubleClick = null;

  synchronized boolean mouseDown(Event e) {
    Debug.println("mouseDown");

    // Just for the fun of it.
    requestFocus();

    Point pImage = new Point(e.x - offset.width,e.y - offset.height);

    // Select
    if (mode == SELECT) {
        int n = areaAt(pImage);
        // If n == -1, we select the background.
        setSelected(n);

        if (n != -1) {
            // Get point in the Area's coordinate system
           // Used for dragging selected areas.
            Rectangle r = ((Area)areas.elementAt(selected)).boundingBox();
            dragPoint.x = pImage.x - r.x;
            dragPoint.y = pImage.y - r.y;
        }
    }

    else {
      clipImage(pImage);

      if (completing()) {
        if (doubleClick != null) {
          // On double click, try to force completion
          if (!((Area)areas.elementAt(selected)).forceCompletion()) {
            deleteSelected();
          }
          // Finished completing an area, go back to selection mode.
          if (((Area)areas.elementAt(selected)).completed()) {
            // This will call MapEditApp.select()
            arrowCheckBox.setState(true);
          }
        }
        else {
          // In the process of completing an area, let the area handle it.
          ((Area)areas.elementAt(selected)).mouseDown(pImage,imageRect);
        }
      }
      else {
        // Start creating a new area.
        switch (mode) {
          case CREATE_RECT_AREA: {
            Debug.println("calling RectArea first mouse down");
            areas.addElement(RectArea.firstMouseDown(pImage));
            syncAreaList();
          }
          break;

          case CREATE_CIRCLE_AREA: {
            areas.addElement(CircleArea.firstMouseDown(pImage));
            syncAreaList();
          }
          break;

          case CREATE_POLY_AREA: {
            areas.addElement(PolyArea.firstMouseDown(pImage));
            syncAreaList();
          }
          break;

          default:
            Debug.println("invalid mode");
        }
        setSelected(areas.size() - 1); // Last element.
      }
    }

    canvas.repaint();
    doubleClick = new Point(e.x,e.y);
    return true;
  }

  void checkKillDoubleClick(Event e) {
    // Don't kill doubleClick if event (x,y) hasn't changed.
    if (doubleClick != null &&
        (doubleClick.x != e.x || doubleClick.y != e.y)) {
      doubleClick = null;
    }
  }

  synchronized boolean mouseDragged(Event e) {
    Debug.println("mouseDragged");
    checkKillDoubleClick(e);

    Point pImage = new Point(e.x - offset.width,e.y - offset.height);
    boolean dirty = false;

    // Move region with pointer
    if (mode == SELECT && selected != -1) {
      // Move so that pImage is a dragPoint with respect to the Area.
      Area area = (Area)areas.elementAt(selected);
      Rectangle r = area.boundingBox();
      int dx = pImage.x - r.x - dragPoint.x;
      int dy = pImage.y - r.y - dragPoint.y;
      if (area.moveBy(dx,dy,imageRect)) {
        dirty = true;
      }
    }

    if (completing()) {
      clipImage(pImage);
      // In the process of completing an area, let the area handle it.
      ((Area)areas.elementAt(selected)).mouseDragged(pImage,imageRect);
      dirty = true;
    }

    if (dirty) {
      canvas.repaint(REPAINT_TIME);
    }
    return true;
  }

  synchronized boolean mouseMoved(Event e) {
    Debug.println("mouseMoved");
    checkKillDoubleClick(e);
    Point pImage = new Point(e.x - offset.width,e.y - offset.height);

    // In the process of completing an area, let the area handle it.
    if (completing()) {
      clipImage(pImage);
      ((Area)areas.elementAt(selected)).mouseMoved(pImage,imageRect);
      canvas.repaint(REPAINT_TIME);
    }

    return true;
  }

  private void checkAreas() {
    Enumeration e = areas.elements();
    while (e.hasMoreElements()) {
      Area a = (Area)e.nextElement();
      if (a.getURL() == null || a.getURL().length() == 0) {
        // TODO: Need to warn the user at this point.
        Debug.println("Found an area with invalid URL.");
      }
    }
  }

    private void done() {
    flushURLComponent();
    if (completing()) {
      deleteSelected();
    }
    // Don't setSelected(-1) because that won't do anything if completing() is true.

    checkAreas();
    writeDoc();
    success = true;
        stopRunning();
    }

  private void cancel() {
    success = false; // redundant
    stopRunning();
  }

  // public so can be called by callbacks.
  public void select() {
    mode = SELECT;

    // Unselect everything.
    flushURLComponent();
    if (completing()) {
      deleteSelected();
    }

// May want to edit just created area.
//    setSelected(-1);

    canvas.repaint();
  }

  // public so can be called by callbacks.
  public void createArea(int newMode) {
    mode = newMode;

    // Unselect everything.
    flushURLComponent();
    if (completing()) {
      deleteSelected();
    }
    setSelected(-1);

    canvas.repaint();
  }


  public void setDocument(Document doc) {
      document = doc;
  }



  private boolean isRunning = false; // Are we inside Application.run().

  private String imageName;
  private int imageLocation; // In tokens, starting with 0.
  private String mapName;
  private Document document;
  private Image image;
  private Dimension imageDim;
  private Rectangle imageRect; // For convenience, (0,0,imageDim.width,imageDim.height)
  public static final int APP_WIDTH = 600;
  public static final int APP_HEIGHT = 400;
  private final static int REPAINT_TIME = 50;


  // All international strings.
  private ResourceBundle res = null;

  /** List of all areas. */
  private Vector areas;
  // Index into areas, -1 means none selected.
  private int selected = -1;
  private StringRef defaultURL = new StringRef();

  // What the user is doing.
  private final static int SELECT = 0;
  private final static int CREATE_RECT_AREA = 1;
  private final static int CREATE_CIRCLE_AREA = 2;
  private final static int CREATE_POLY_AREA = 3;
  private int mode = SELECT;

  // Used for dragging selected areas.
  private Point dragPoint = new Point(0,0);

  // Did plugin succeed.
  private boolean success = false;
}


/** Double buffered, asks the MapEditApp to redraw for it. */
class BlankCanvas extends Canvas {
    BlankCanvas(MapEditApp mEdtApp) {
      mapEditApp = mEdtApp;
    }

    // Just send it to the MapEditApp
    synchronized public boolean handleEvent(Event e) {
      if (e.target == this) {
        switch (e.id) {
          case Event.KEY_PRESS:
          case Event.KEY_ACTION:
            return mapEditApp.keyDown(e);
          case Event.MOUSE_DOWN:
            return mapEditApp.mouseDown(e);
          case Event.MOUSE_UP:
            return mapEditApp.mouseUp(e);
          case Event.MOUSE_MOVE:
            return mapEditApp.mouseMoved(e);
          case Event.MOUSE_DRAG:
            return mapEditApp.mouseDragged(e);
        }
      }
      return false;
    }

    public synchronized void reshape(int x, int y, int width, int height) {
      // do the real stuff
      super.reshape(x, y, width, height);
      mapEditApp.reshapeCanvas(width,height);

      // Create offscreen image for double buffer.
      offscreen = createImage(width,height);
      offDim.width = width;
      offDim.height = height;
      repaint();
    }

    synchronized public void paint(Graphics g) {
      mapEditApp.paintMe(g);
    }

    public synchronized void update(Graphics g) {
      if (offscreen != null) {
        Debug.println("BlankCanvas.paint");
        Graphics gOff = offscreen.getGraphics();
        gOff.setColor(getBackground());
        gOff.fillRect(0,0,offDim.width,offDim.height);
        paint(gOff);
        g.drawImage(offscreen,0,0,mapEditApp);
      }

/*
        g.setColor(getBackground());
        g.fillRect(0,0,offDim.width,offDim.height);
        paint(g);
*/
    }

    private Image offscreen = null;
    private Dimension offDim = new Dimension();
    private MapEditApp mapEditApp;
}
