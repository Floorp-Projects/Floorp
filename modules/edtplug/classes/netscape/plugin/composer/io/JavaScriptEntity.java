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

package netscape.plugin.composer.io;

/** A JavaScript entity is a html expression of the form "&amp;{" javascript "};".
 * It is used to provide run-time evaluation of the value of parameters.
 * At design time (i.e. in the editor) the value of a JavaScript entity is
 * not known.
 */

public class JavaScriptEntity extends Token {
  private String text;

  public JavaScriptEntity(StringBuffer buf) {
    text = buf.toString();
  }

  JavaScriptEntity(FooStringBuffer buf) {
    text = buf.toString();
  }

  public String getScript() {
    return text;
  }

  public String toString() {
    return "&{" + text + "}";
  }

  public int hashCode() {
    return text.hashCode() + 1;
  }

  /** Note: Equality means that the expression string is identical
   * not that the result of the expressions are equal.
   */
  public boolean equals(Object other) {
    if ((other != null) && (other instanceof JavaScriptEntity)) {
      JavaScriptEntity o = (JavaScriptEntity) other;
      return text.equals(o.text);
    }
    return false;
  }
}
