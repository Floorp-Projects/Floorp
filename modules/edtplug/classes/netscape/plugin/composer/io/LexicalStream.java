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

import java.io.*;

/** An HTML lexical stream. Takes a Reader and breaks it
 * up into lexical tokens.
 * @see Reader
 * @see Comment
 * @see JavaScriptEntity
 * @see Entity
 * @see Tag
 * @see Text
 * @see Token
 */

public class LexicalStream {
  private SlidingBuffer in;
  private FooStringBuffer buffer;
  private final static String NEWLINE = new String("\n");
  private boolean bHaveClosedStream;

  /** Create a lexical stream from a unicode string.
   * @param in the input string.
   */

  public LexicalStream(String in) {
    this(new CharArrayReader(in.toCharArray()));
  }

  /** Create a lexical stream from a Reader. The
   * stream's close() method will be called automaticly
   * the first time next() returns null. (i.e. when the
   * iterator finishes delivering tokens.)
   * @param in the input stream.
   */

  public LexicalStream(Reader in) {
    this.in = new SlidingBuffer(in);
  }

  private int read() throws IOException {
    return in.read();
  }

  private boolean lookAhead(char c) throws IOException {
    return in.lookAhead(c);
  }

  private boolean lookAhead(String s) throws IOException {
    return in.lookAhead(s);
  }

  private boolean lookAhead(String s, boolean ignoreCase) throws IOException {
    return in.lookAhead(s, ignoreCase);
  }

  private boolean eatNewline() throws IOException {
    return in.eatNewline();
  }

  private boolean eatWhiteSpace() throws IOException {
    return in.eatWhiteSpace();
  }

  /** Return the next token in an HTML input stream. \r\n's are
   * considered their own token (though we get rid of the \r).
   * Returns null if the input stream has run out of tokens.
   * @return the next token in the stream, or null if the stream is
   * out of tokens.
   */
  public Token next() throws IOException {
    for (;;) {
      int c = read();
      if (c < 0) break;
      if (c == '&') {
        if (buffer != null) {
          in.unread(1);
          break;
        }
    FooStringBuffer buf = new FooStringBuffer();
    /* Don't allow javascript entities outside of parameter values.
     if (in.lookAhead('{')) {
      parseJavaScriptEntity(buf);
      return new JavaScriptEntity(buf);
    }
    */
    parseEntity(buf);
    return new Entity(buf);
      } else if (c == '<') {
    if (buffer != null) {
      in.unread(1);
      break;
    }
    if (in.lookAhead('/')) {
      return parseTag(false);
    } else if (in.lookAhead('!')) {
      return parseComment();
    }
    return parseTag(true);
      }
      if (c == '\r') {
    if (buffer != null) {
      in.unread(1);
      break;
    }
    in.lookAhead('\n');
    return new Text(NEWLINE);
      }
      if (c == '\n') {
    if (buffer != null) {
      in.unread(1);
      break;
    }
    return new Text(NEWLINE);
      }
      if (buffer == null) {
    buffer = new FooStringBuffer();
      }
      buffer.append((char)c);
    }
    if (buffer != null) {
      String rv = buffer.toString();
      buffer = null;
      return new Text(rv);
    }
    if ( ! bHaveClosedStream ) {
        in.close();
        bHaveClosedStream = true;
    }
    return null;
  }

  private boolean isWhitespace(char c){
    /* JDK 1.1 return Character.isWhitespace(c); */
    return Character.isSpace(c);
  }
  private Token parseTag(boolean open) throws IOException {
    // Capture tag name
    FooStringBuffer name = new FooStringBuffer();
    int c;
    for (;;) {
      c = read();
      if (c < 0) break;
      if ((c == '>') || isWhitespace((char)c)) break;
      name.append((char) c);
    }
    if (name.length() == 0) {
      name.append('<');
      if (!open) name.append('/');
      if (c >= 0) {
    name.append((char) c);
      }
      return new Text(name.toString());
    }
    Tag tag = new Tag(name.toString(), open);
    if (c == '>') return tag;

    // Now process tag attributes
    for (;;) {
      c = read();
      if ((c < 0) || (c == '>')) break;
      if (isWhitespace((char)c)) continue;
      in.unread(1);
      parseTagAttribute(tag);
    }
    return tag;
  }

  private void parseTagAttribute(Tag tag) throws IOException {
    // First get attribute name
    FooStringBuffer name = new FooStringBuffer();
    int c;
    for (;;) {
      c = read();
      if (c < 0) break;
      if ((c == '>') || (c == '=')) {
    in.unread(1);
    break;
      }
      if (isWhitespace((char)c)) {
    break;
      }
      name.append((char) c);
    }
    if (name.length() == 0) {
      return;
    }

    // Allow for whitespace between the attribute name and value
    eatWhiteSpace();
    c = read();

    FooStringBuffer value = null;
    if (c != '=') {
      // No attribute value follows the attribute name
      in.unread(1);
    } else {
      // Allow for whitespace between the '=' and the attribute value
      eatWhiteSpace();

      // Possibly an attribute value follows the attribute name
      c = read();
      if (c < 0) {
    // No attribute value follows the attribute name
      } else if (c == '>') {
    // No attribute value follows the attribute name. This
    // is a syntax error within the tag
    in.unread(1);
      } else {
    // Grab attribute value
    if ((c == '\'') || (c == '"')) {
      value = parseQuotedString(c);
    } else {
      value = new FooStringBuffer();
      value.append((char) c);
      for (;;) {
        c = read();
        if (c < 0) break;
        if (c == '>') {
          in.unread(1);
          break;
        }
        if (isWhitespace((char)c)) break;
        // XXX allow for concatenated quotes?
        value.append((char) c);
      }
    }
      }
    }
    tag.addAttribute(name.toString(), (value!=null) ? value.toString() : null);
  }

  private FooStringBuffer parseQuotedString(int stop) throws IOException {
    FooStringBuffer out = new FooStringBuffer();
    for (;;) {
      int c = read();
      if (c < 0) {
    break;
      }
      if (c == '&') {
    // Entities can be embedded in html quoted strings; they will be
    // reparsed later when the attribute value is evaluated
    if (in.peek() == '{') {
      read();
      parseJavaScriptEntity(out);
    } else {
      parseEntity(out);
    }
      } else {
    if (c == stop) {
      break;
    }
    out.append((char) c);
      }
    }
    return out;
  }

  /* Process an HTML comment */
  private Comment parseComment() throws IOException {
    FooStringBuffer out = new FooStringBuffer();
    boolean fancyTerminator = false;
    if (in.lookAhead('-')) {
      if (in.lookAhead('-')) {
    // This comment started with "<!--"; therefore we will look for
    // its terminator which is "-->"
    fancyTerminator = true;
      } else {
    out.append('-');
      }
    }

    // Gobble up data that lives in the comment until we find the
    // comment terminator (which is either ">" or "-->")
    for (;;) {
      int c = read();
      if (c < 0) {
        break;
      }
      if (fancyTerminator) {
        if (c == '-') {
          if (in.lookAhead('-')) {
            if (in.lookAhead('>')) {
              break;
            } else {
              out.append("--");
            }
          } else {
            // the minus sign will be put out by the out.append((char) c); below.
          }
        }
      } else if (c == '>') {
        break;
      }
      out.append((char)c);
    }
    return fancyTerminator ? new Comment("--" + out + "--") : new Comment(out);
  }

  /* Process an HTML entity */
  private void parseEntity(FooStringBuffer out) throws IOException {
    for (;;) {
      int c = read();
      if (c < 0) {
    break;
      }
      if (c == ';') {
    break;
      }
      // Ending an entity with a space is a Netscape-ism we support
      if (isWhitespace((char)c)) {
    in.unread(1);
    break;
      }
      out.append((char)c);
    }
  }

  /* Process an HTML script entity */
  private void parseJavaScriptEntity(FooStringBuffer out) throws IOException {
    int count = 1;
    for (;;) {
      int c = read();
      if (c < 0) break;
      if ((c == '\'') || (c == '"')) {
    parseJavaScriptQuotedString(out, c);
      } else if (c == '{') {
    out.append((char) c);
    count++;
      } else if (c == '}') {
    if (--count == 0) {
      in.lookAhead(';'); // eat trailing ";" that we don't care about
      return;
    }
    out.append((char) c);
      } else if (c == '/') {
    c = read();
    if (c < 0) break;
    if (c == '*') {
      parseCComment(out);
    } else if (c == '/') {
      parseEOLComment(out);
    } else {
      out.append('/');
      out.append((char) c);
    }
      } else {
    out.append((char) c);
      }
    }
  }

  private void parseJavaScriptQuotedString(FooStringBuffer out, int stop) throws IOException {
    out.append((char) stop);
    for (;;) {
      int c = read();
      if (c < 0) {
    break;
      }
      out.append((char) c);
      if (c == '\\') {
    c = read();
    if (c < 0) {
      break;
    }
    out.append((char) c);
    continue;
      }
      if (c == stop) {
    break;
      }
    }
  }

  private void parseCComment(FooStringBuffer out) throws IOException {
    out.append("/*");
    for (;;) {
      int c = read();
      if (c < 0) {
    break;
      }
      out.append((char) c);
      if (c == '*') {
    c = read();
    if (c < 0) {
      break;
    }
    out.append((char) c);
    if (c == '/') {
      break;
    }
      }
    }
  }

  private void parseEOLComment(FooStringBuffer out) throws IOException {
    out.append("//");
    for (;;) {
      int c = read();
      if (c < 0) {
    break;
      }
      out.append((char) c);
      if ((c == '\n') || (c == '\r')) {
    out.append((char) c);
    break;
      }
    }
  }
}
