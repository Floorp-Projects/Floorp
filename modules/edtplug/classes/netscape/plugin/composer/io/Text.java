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
