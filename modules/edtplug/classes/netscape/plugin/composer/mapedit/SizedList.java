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

/** List with our desired sizes */
class SizedList extends List {
  public Dimension minimumSize() {
    return new Dimension(WIDTH,HEIGHT);
  }

  public Dimension minimumSize(int rows) {
    return new Dimension(WIDTH,HEIGHT);
  }

  public Dimension preferredSize() {
    return new Dimension(WIDTH,HEIGHT);
  }

  public Dimension preferredSize(int rows) {
    return new Dimension(WIDTH,HEIGHT);
  }

  private final static int WIDTH = 200;
  private final static int HEIGHT = 10; // Does this matter?
}
