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

/** The token type for html comments. HTML comments, for our purposes, have this form:
 * '&lt;!' + text + '>'.
 */

public class Comment extends Token {
  String text;

  /** Create a comment from a string buffer.
   * @param buf The text of the comment. Doesn't include the '!' at the start of the
   * comment.
   */
  public Comment(StringBuffer buf) {
    text = buf.toString();
  }

  /** Create a comment from a string buffer.
   * @param buf The text of the comment. Doesn't include the '!' at the start of the
   * comment.
   */
  Comment(FooStringBuffer buf) {
    text = buf.toString();
  }

  /** Create a comment from a string.
   * @param buf The text of the comment. Doesn't include the '!' at the start of the
   * comment.
   */
  public Comment(String text) {
    this.text = text;
  }

  /** The text of the comment. Doesn't include the '!' at the start of the
   * comment.
   */
  public String getText() {
    return text;
  }

  /** Return the full html representation of the comment, including
   * the "&lt;!" and the ">".
   */
  public String toString() {
    return "<!" + text + ">";
  }

  /** Compute the hash code for the comment.
   * @return the hash code of the comment.
   */

  public int hashCode() {
    return text.hashCode();
  }

  /** Equality test.
   * @param other the objcet to test for equality with this object.
   * @return true if the text of this comment equals the text of
   * the other comment.
   */
  public boolean equals(Object other) {
    if ((other != null) && (other instanceof Comment)) {
      return text.equals(((Comment)other).text);
    }
    return false;
  }

  /** Create a selection start comment. Equivalent to
   * createSelectionStart(false);
   * @return a canonical selection start comment.
   */

  public static Comment createSelectionStart(){
    return createSelectionStart(false);
  }

  /** Create a selection start comment.
   * @param stickyAfter is true if the selection start sticks to the next token.
   * @return a selection start comment.
   */

  public static Comment createSelectionStart(boolean stickyAfter){
    return new Comment(stickyAfter ? SELECTION_START_PLUS : SELECTION_START);
  }
  /** Create a selection end comment. Equivalent to
   * createSelectionEnd(false);
   * @return a canonical selection end comment.
   */

  public static Comment createSelectionEnd(){
    return createSelectionEnd(false);
  }

  /** Create a selection end comment.
   * @param stickyAfter is true if the selection end sticks to the next token.
   * @return a selection end comment.
   */

  public static Comment createSelectionEnd(boolean stickyAfter){
    return new Comment(stickyAfter ? SELECTION_END_PLUS : SELECTION_END);
  }

  /** Check if this comment is a selection comment.
   * @return true if this comment is a selection comment.
   */
  public boolean isSelection() {
    return isSelectionStart() || isSelectionEnd();
  }

  /** Check if this comment is a selection start comment.
   * @return true if this comment is a selection start comment.
   */
  public boolean isSelectionStart() {
    return text.equals(SELECTION_START) || text.equals(SELECTION_START_PLUS);
  }

  /** Check if this comment is a selection end comment.
   * @return true if this comment is a selection end comment.
   */
  public boolean isSelectionEnd() {
    return text.equals(SELECTION_END) || text.equals(SELECTION_END_PLUS);
  }

  /** Check if this comment is a sticky-after selection comment.
   * @return true if this comment is a selection comment and is a sticky-after comment.
   */
  public boolean isSelectionStickyAfter() {
    return text.equals(SELECTION_START_PLUS) || text.equals(SELECTION_END_PLUS);
  }

  private static final String SELECTION_START = new String("-- selection start --");
  private static final String SELECTION_END = new String("-- selection end --");
  private static final String SELECTION_START_PLUS = new String("-- selection start+ --");
  private static final String SELECTION_END_PLUS = new String("-- selection end+ --");
}
