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

package netscape.plugin.composer.io;
import java.util.Enumeration;

/** Entities are multi-character combinations that start with '&amp;', and end
 * in ';'. They are used for two purposes:
 * <ol>
 * <li> To escape '&amp;', and '&lt' in html.
 * <li> To enable the representation of unusual, or hard-to-input characters,
 * such as &amp;nbsp;, which is the entity for a non-breaking space character.
 * </ol>
 *
 * @see netscape.plugin.composer.io.JavaScriptEntity
 */

public class Entity extends Token {
  private String name;
  private int ch;

  /** Create an Entity from a string buffer.
   * Such entities have a value of -1 during
   * the time the design plugin is run.
   */
  public Entity(StringBuffer s) {
    name = s.toString();
    ch = -1;
  }

  /** Enumerate the known entites.
   * Note that there may be several entities with the same value.
   * @return an enumeration of the known standard entities.
   */
  public static Enumeration entities() {
    return new EntityEnumeration(knownEntities);
  }

  /** Create an Entity from a string buffer.
   * Such entities have a value of -1 during
   * the time the design plugin is run.
   */
  Entity(FooStringBuffer s) {
    name = s.toString();
    ch = -1;
  }

  /** Create an entity from a string and a value.
   * Used to build up an internal list of
   * legal entities.
   */
  public Entity(String s, char c) {
    name = s;
    ch = (int) c;
  }

  /** Get the value for the entity. This is the
   * unicode value of the entity, or else -1 if
   * the entity is unknown.
   */
  public int getValue() {
    if (ch == -1) {
      ch = evaluate();
    }
    return ch;
  }

  /** Internal function that actually evaluates the
   * the entity. Returns -1 if the entity is unknown.
   */

  protected int evaluate() {
    try {
      if (name.charAt(0) == '#') {
    int value = 0;
    int n = name.length();
    for (int i = 1; i < n; i++) {
      char c = name.charAt(i);
      if ((c < '0') || (c > '9')) {
        break;
      }
      value = value * 10 + (c - '0');
    }
    return value;
      } else {
    for (int i = 0; i < knownEntities.length; i++) {
      if (knownEntities[i].name.equals(name)) {
        return knownEntities[i].ch;
      }
    }
      }
    } catch (ArrayIndexOutOfBoundsException exc) {
    }
    return -1;
  }

  /** @return the html string representation of the entity.
   */
  public String toString() {
    return "&" + name + ";";
  }

  /** @return the hash code the entity.
   */
  public int hashCode() {
    if (ch == -1) {
      ch = evaluate();
    }
    return name.hashCode() ^ ch;
  }

  /** @param other the object to test for equality.
   * @return true if other is the same entity as this object.
   */
  public boolean equals(Object other) {
    if ((other != null) && (other instanceof Entity)) {
      Entity o = (Entity) other;
      return name.equals(o.name);
    }
    return false;
  }

  static private Entity knownEntities[] = new Entity[107];
  static {
    int i = 0;

    knownEntities[i++] = new Entity("lt", '<');
    knownEntities[i++] = new Entity("LT", '<');
    knownEntities[i++] = new Entity("gt", '>');
    knownEntities[i++] = new Entity("GT", '>');
    knownEntities[i++] = new Entity("amp", '&');

    knownEntities[i++] = new Entity("AMP", '&');
    knownEntities[i++] = new Entity("quot", '\"');
    knownEntities[i++] = new Entity("QUOT", '\"');
    knownEntities[i++] = new Entity("nbsp", '\240');
    knownEntities[i++] = new Entity("reg", '\256');

    knownEntities[i++] = new Entity("REG", '\256');
    knownEntities[i++] = new Entity("copy", '\251');
    knownEntities[i++] = new Entity("COPY", '\251');
    knownEntities[i++] = new Entity("iexcl", '\241');
    knownEntities[i++] = new Entity("cent", '\242');

    knownEntities[i++] = new Entity("pound", '\243');
    knownEntities[i++] = new Entity("curren", '\244');
    knownEntities[i++] = new Entity("yen", '\245');
    knownEntities[i++] = new Entity("brvbar", '\246');
    knownEntities[i++] = new Entity("sect", '\247');

    knownEntities[i++] = new Entity("uml", '\250');
    knownEntities[i++] = new Entity("ordf", '\252');
    knownEntities[i++] = new Entity("laquo", '\253');
    knownEntities[i++] = new Entity("not", '\254');
    knownEntities[i++] = new Entity("shy", '\255');

    knownEntities[i++] = new Entity("macr", '\257');
    knownEntities[i++] = new Entity("deg", '\260');
    knownEntities[i++] = new Entity("plusmn", '\261');
    knownEntities[i++] = new Entity("sup2", '\262');
    knownEntities[i++] = new Entity("sup3", '\263');

    knownEntities[i++] = new Entity("acute", '\264');
    knownEntities[i++] = new Entity("micro", '\265');
    knownEntities[i++] = new Entity("para", '\266');
    knownEntities[i++] = new Entity("middot", '\267');
    knownEntities[i++] = new Entity("cedil", '\270');

    knownEntities[i++] = new Entity("sup1", '\271');
    knownEntities[i++] = new Entity("ordm", '\272');
    knownEntities[i++] = new Entity("raquo", '\273');
    knownEntities[i++] = new Entity("frac14", '\274');
    knownEntities[i++] = new Entity("frac12", '\275');

    knownEntities[i++] = new Entity("frac34", '\276');
    knownEntities[i++] = new Entity("iquest", '\277');
    knownEntities[i++] = new Entity("Agrave", '\300');
    knownEntities[i++] = new Entity("Aacute", '\301');
    knownEntities[i++] = new Entity("Acirc", '\302');

    knownEntities[i++] = new Entity("Atilde", '\303');
    knownEntities[i++] = new Entity("Auml", '\304');
    knownEntities[i++] = new Entity("Aring", '\305');
    knownEntities[i++] = new Entity("AElig", '\306');
    knownEntities[i++] = new Entity("Ccedil", '\307');

    knownEntities[i++] = new Entity("Egrave", '\310');
    knownEntities[i++] = new Entity("Eacute", '\311');
    knownEntities[i++] = new Entity("Ecirc", '\312');
    knownEntities[i++] = new Entity("Euml", '\313');
    knownEntities[i++] = new Entity("Igrave", '\314');

    knownEntities[i++] = new Entity("Iacute", '\315');
    knownEntities[i++] = new Entity("Icirc", '\316');
    knownEntities[i++] = new Entity("Iuml", '\317');
    knownEntities[i++] = new Entity("ETH", '\320');
    knownEntities[i++] = new Entity("Ntilde", '\321');

    knownEntities[i++] = new Entity("Ograve", '\322');
    knownEntities[i++] = new Entity("Oacute", '\323');
    knownEntities[i++] = new Entity("Ocirc", '\324');
    knownEntities[i++] = new Entity("Otilde", '\325');
    knownEntities[i++] = new Entity("Ouml", '\326');

    knownEntities[i++] = new Entity("times", '\327');
    knownEntities[i++] = new Entity("Oslash", '\330');
    knownEntities[i++] = new Entity("Ugrave", '\331');
    knownEntities[i++] = new Entity("Uacute", '\332');
    knownEntities[i++] = new Entity("Ucirc", '\333');

    knownEntities[i++] = new Entity("Uuml", '\334');
    knownEntities[i++] = new Entity("Yacute", '\335');
    knownEntities[i++] = new Entity("THORN", '\336');
    knownEntities[i++] = new Entity("szlig", '\337');
    knownEntities[i++] = new Entity("agrave", '\340');

    knownEntities[i++] = new Entity("aacute", '\341');
    knownEntities[i++] = new Entity("acirc", '\342');
    knownEntities[i++] = new Entity("atilde", '\343');
    knownEntities[i++] = new Entity("auml", '\344');
    knownEntities[i++] = new Entity("aring", '\345');

    knownEntities[i++] = new Entity("aelig", '\346');
    knownEntities[i++] = new Entity("ccedil", '\347');
    knownEntities[i++] = new Entity("egrave", '\350');
    knownEntities[i++] = new Entity("eacute", '\351');
    knownEntities[i++] = new Entity("ecirc", '\352');

    knownEntities[i++] = new Entity("euml", '\353');
    knownEntities[i++] = new Entity("igrave", '\354');
    knownEntities[i++] = new Entity("iacute", '\355');
    knownEntities[i++] = new Entity("icirc", '\356');
    knownEntities[i++] = new Entity("iuml", '\357');

    knownEntities[i++] = new Entity("eth", '\360');
    knownEntities[i++] = new Entity("ntilde", '\361');
    knownEntities[i++] = new Entity("ograve", '\362');
    knownEntities[i++] = new Entity("oacute", '\363');
    knownEntities[i++] = new Entity("ocirc", '\364');

    knownEntities[i++] = new Entity("otilde", '\365');
    knownEntities[i++] = new Entity("ouml", '\366');
    knownEntities[i++] = new Entity("divide", '\367');
    knownEntities[i++] = new Entity("oslash", '\370');
    knownEntities[i++] = new Entity("ugrave", '\371');

    knownEntities[i++] = new Entity("uacute", '\372');
    knownEntities[i++] = new Entity("ucirc", '\373');
    knownEntities[i++] = new Entity("uuml", '\374');
    knownEntities[i++] = new Entity("yacute", '\375');
    knownEntities[i++] = new Entity("thorn", '\376');

    knownEntities[i++] = new Entity("yuml", '\377');
	
	// the euro currency character
    knownEntities[i++] = new Entity("euro", '\u20AC');
  }
}

class EntityEnumeration implements Enumeration {
    public EntityEnumeration(Entity[] entities){
        this.entities = entities;
    }
    public boolean hasMoreElements(){
        return index < entities.length;
    }
    public Object nextElement() {
        if ( index >= entities.length ) {
            throw new java.util.NoSuchElementException();
        }
        return entities[index++];
    }
    private Entity[] entities;
    private int index;
}
