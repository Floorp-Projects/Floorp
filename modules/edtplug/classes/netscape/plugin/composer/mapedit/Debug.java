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

public class Debug {
  static public void println(Object msg) {
    if (debugOn)
      System.out.println("MapEditDebug: " + msg);
  }

  static public void assert(Object o) {
    if (debugOn && o == null) {
      assert(false);
    }
  }

  static public void assert(Object o,String msg) {
    if (debugOn && o == null) {
      assert(false,msg);
    }
  }

  static public void assert(boolean val) {
    if (debugOn) {
      assert(val,null);
    }
  }

  static public void assert(boolean val,String msg) {
    if (debugOn && !val) {
      if (msg != null && msg.length() > 0) {
        System.out.println("Assertion failed:" + msg);
      }
      else {
        System.out.println("Assertion failed");
      }
      Thread.dumpStack();
    }
  }

  static public boolean debug() {return debugOn;}

  // Set/unset this to control debug mode.
  private static boolean debugOn = false;
}


