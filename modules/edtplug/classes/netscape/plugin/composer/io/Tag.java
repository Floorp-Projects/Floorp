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

import java.io.PrintStream;
import java.io.IOException;
import java.util.Hashtable;
import java.util.Enumeration;

/** A HTML Tag. A html tag has a name, an open or close property,
 * and zero or more attributes.
 * <p>
 * Attributes have a name and a value. If the value is the empty string,
 * then the attribute is printed as NAME, otherwise it is printed as
 * NAME=VALUE.
 */
public class Tag extends Token {
  private String name;

  private boolean open;
  private Hashtable attributes;

  private static String emptyString = "";

  /** Create a new 'open' Tag object. This always creates an open tag. Use the
   * constructor that takes a second, boolean, parameter to create close
   * tags.
   * @param name the name of the tag. Automaticly converted to upper case.
   */
  public Tag(String name) {
    this(name, true);
  }
  /** Create a new Tag object.
   * @param name the name of the tag. Automaticly converted to upper case.
   * @param open true if this is an open tag.
   */
  public Tag(String name, boolean open) {
    this.name = name.toUpperCase();
    this.open = open;
  }

  /** Clone the tag.
   */
    public Object clone() {
        Tag result = null;
        try {
            result = (Tag) super.clone();
        } catch (CloneNotSupportedException e){
            e.printStackTrace();
        }
        if ( result.attributes != null ) {
            result.attributes = (Hashtable) result.attributes.clone();
        }
        return result;
    }

  /** Get the name.
   * @return the name of the tag. Will always be in upper case.
   */
  public String getName() {
    return name;
  }

  /** Get the open/close state of the tag.
   * @return true if this is an open tag.
   */
  public boolean isOpen() {
    return open;
  }

  /** Get the open/close state of the tag.
   * @return true if this is a close tag.
   */
  public boolean isClose() {
    return !open;
  }

  /** Clear the existing attributes.
   */

  public void clearAttributes() {
    if ( attributes != null ) {
        attributes.clear();
    }
  }

  /** Add an attribute name and value pair to the tag. If the attribute
   * already exists, addAttribute replaces (and returns) the old value.
   * @param name the name of the attribute. The name is
   * always converted to upper case.
   * @param value the value of the attribute.
   */
  public String addAttribute(String name, String value) {
    name = name.toUpperCase();
    if (attributes == null) {
      attributes = new Hashtable(7);
    }
    if ((value == null) || (value.length() == 0)) {
      value = emptyString;
    }
    return (String) attributes.put(name, value);
  }

  /** Remove an attribute name and value pair from the tag.
   * @param name the name of the attribute. The name must
   * be in upper case.
   * @return the value of the attribute, or null if the
   * attribute was not part of the tag.
   */
  public String removeAttribute(String name) {
    if (attributes == null) {
      return null;
    }
    return (String) attributes.remove(name);
  }

  /** Look up the value associated with a given tag attribute.
   * @param name The attribute to look up. Must be upper case.
   * @return the value of the attribute, or null if the attribute
   * does not exist.
   */

  public String lookupAttribute(String name) {
    if (attributes == null) {
      return null;
    }
    return (String) attributes.get(name);
  }

  /** Test if this tag has a particular attribute.
   * @param name the attribute you're searching for. Must be upper case.
   * @return true if the tag has an attribute present with the given name.
   */
  public boolean containsAttribute(String name) {
    if (attributes == null) {
      return false;
    }
    return attributes.get(name) != null;
  }

  /**
   * @return an enumeration of the attributes of this tag.
   */
  public Enumeration getAttributes() {
    if ( attributes == null ) {
        attributes = new Hashtable(); // Hack
    }
    return attributes.keys();
  }

  /** Test if a tag has attributes.
   * @return an iterator over the attributes.
   */
  public boolean hasAttributes() {
    if (attributes == null) {
      return false;
    }
    return !attributes.isEmpty();
  }

  /** Translate the tag object into a string representation.
   * @return the html string representation of the tag.
   */
  public String toString() {
    FooStringBuffer sbuf = new FooStringBuffer();
    sbuf.append('<');
    if (!open) sbuf.append('/');
    sbuf.append(name.toString());
    if (attributes != null) {
      for (Enumeration e = attributes.keys(); e.hasMoreElements(); ) {
    String name = (String) e.nextElement();
    sbuf.append(' ');
    sbuf.append(name.toString());
    String value = (String) attributes.get(name);
    if (value != emptyString) {
      sbuf.append('=');
      sbuf.append(QuoteValue(value));
    }
      }
    }
    sbuf.append('>');
    return sbuf.toString();
  }

  /** Compute a hashcode.
   * @return the hashcode for the tag.
   */
  public int hashCode() {
    int code = name.hashCode() + (open ? 1 : 0);
    if (attributes != null) {
      for (Enumeration e = attributes.keys(); e.hasMoreElements(); ) {
    String name = (String) e.nextElement();
    String value = (String) attributes.get(name);
    code = code ^ (name.hashCode() << 3) ^ (value.hashCode() << 13);
      }
    }
    return code;
  }

  /** See if this tag equals another tag, including the
   * name and the attributes.
   * @param other the object to be compared.
   * @return true if other is a tag and other equals this.
   */
  public boolean equals(Object other) {
    if ((other != null) && (other instanceof Tag)) {
      Tag t = (Tag) other;
      if (!name.equals(t.name)) return false;
      if (((attributes != null) && (t.attributes == null)) ||
      ((attributes == null) && (t.attributes != null))) {
    return false;
      }
      if (attributes == null) return true;
      if (attributes.size() != t.attributes.size()) return false;
      for (Enumeration e = attributes.keys(); e.hasMoreElements(); ) {
    String name = (String) e.nextElement();
    String value1 = (String) attributes.get(name);
    String value2 = (String) t.attributes.get(name);
    if (value1 != null) {
      if (!value1.equals(value2)) {
        return false;
      }
    } else {
      if (value2 != null) return false;
    }
      }
      return true;
    }
    return false;
  }

  // XXX if the values are normalized to unicode then this routines should
  // XXX go back to a particular character set, right?
  /** Quote a string assuming that it will be presented in HTML as a
     tag's parameter value in HTML form
   */
  private static String QuoteValue(String value) {
    return "\"" + value + "\"";
  }
}
