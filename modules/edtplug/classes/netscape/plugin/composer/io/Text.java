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

/** A token for the text of an html document.
 */
public class Text extends Token {
    /** Create a Text object from a given string.
     * @param text the text.
     */
    public Text(String text){
        this.text = text;
    }
    /** Set the text.
     * @param text the text.
     */
    public void setText(String text){
        this.text = text;
    }
    /** Get the text.
     * @return the text.
     */
    public String getText(){
        return text;
    }
    /** Convert the text to its html representation.
     * @return the html representation of the text.
     */
    public String toString() {
        return text;
    }
    /** Returns true if this text is a solitary newline
     * character.
     */
    public boolean isNewline() {
        return text.equals(NEWLINE);
    }
    /** Calculate the hash code for this object.
     * @return the hash code for this object.
     */
    public int hashCode() {
        return text.hashCode();
    }
    /** Check for equality.
     * @param other the object to compare.
     * @return true if other has the same text as this.
     */

    public boolean equals(Object other) {
        if ((other != null) && (other instanceof Text)) {
            return text.equals(((Text)other).text);
        }
        return false;
    }

    private String text;
    private static final String NEWLINE = new String("\n");
}
