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

/*
 * $Header: /home/hwine/cvs_conversion/cvsroot/mozilla/modules/edtplug/classes/netscape/plugin/composer/io/SlidingBuffer.java,v 3.1 1998/03/28 03:33:50 ltabb Exp $
 *
 * $Log: SlidingBuffer.java,v $
 * Revision 3.1  1998/03/28 03:33:50  ltabb
 * bump rev to 3.1
 *
 * Revision 1.1  1998/03/28 02:39:09  ltabb
 * Free the lizard
 *
 * Revision 2.1  1998/03/11 23:56:37  anthonyd
 * Bump revision number to 2
 *
 * Revision 1.5  1998/03/06 04:19:44  jwz
 * added NPL and copyright
 *
 * Revision 1.4  1998/02/16 22:11:51  anthonyd
 * Sanitized Files.
 *
 * Revision 1.3  1996/12/18 02:32:54  palevich
 * Version 13.
 *
 * Revision 1.2  1996/12/07 04:30:04  palevich
 * Updated composer plugin API
 *
 * Revision 1.1  1996/11/20 03:19:21  palevich
 * Added Kipp's tokenizer.
 *
 * Revision 1.1  1996/10/28 18:31:03  kipp
 * I fixed some problems in lexical analysis by writing SlidingBuffer.java and
 * then switching the LexicalStream to use it instead of the LookAheadUnicodeInputStream
 * class (which doesn't quite work).
 *
 */
package netscape.plugin.composer.io;

import java.io.*;

/**
 * A sliding buffer that moves over a Reader. The buffer
 * is designed to support lexical analyzers.
 */
class SlidingBuffer extends Reader {
  static final int DEFAULT_BUFFER_LENGTH = 4096;

  // The underlying input stream
  protected Reader in;

  // The data buffer. buf[offset] is the next character to read.
  // end is the offset of the last piece of data in the buffer, plus one
  // end - offset is the number of characters in the buffer
  protected char buffer[];
  protected int offset;
  protected int end;

  public SlidingBuffer(Reader in) {
    this.in = in;
    buffer = new char[DEFAULT_BUFFER_LENGTH];
  }

  public SlidingBuffer(Reader in, int buflen) {
    this.in = in;
    if (buflen < DEFAULT_BUFFER_LENGTH) buflen = DEFAULT_BUFFER_LENGTH;
    buffer = new char[buflen];
  }

  /**
   * Reads a character of data. This method will block if no input is
   * available.
   * @return    the byte read, or -1 if the end of the
   *        stream is reached.
   * @exception IOException If an I/O error has occurred.
   */
  public int read() throws IOException {
    int amount = end - offset;
    if (amount == 0) {
      if (!fill()) return -1;
    }
    char c = buffer[offset++];
    return c;
  }

  /**
   * Reads into an array of characters.  This method will
   * block until some input is available.
   * @param c   the buffer into which the data is read
   * @return  the actual number of characters read, -1 is
   *        returned when the end of the stream is reached.
   * @exception IOException If an I/O error has occurred.
   */
  public int read(char c[]) throws IOException {
    int amount = end - offset;
    if (amount == 0) {
      if (!fill()) return -1;
    }
    amount = end - offset;
    if (amount > c.length) amount = c.length;
    System.arraycopy(buffer, offset, c, 0, amount);
    offset += amount;
    return amount;
  }

  /**
   * Reads into an array of characters.  This method will
   * block until some input is available.
   * @param c   the buffer into which the data is read
   * @param off the start offset of the data
   * @param len the maximum number of bytes read
   * @return  the actual number of characters read, -1 is
   *        returned when the end of the stream is reached.
   * @exception IOException If an I/O error has occurred.
   */
  public int read(char c[], int off, int len) throws IOException {
    int amount = end - offset;
    if (amount == 0) {
      if (!fill()) return -1;
    }
    amount = end - offset;
    if (amount > len) amount = len;
    System.arraycopy(buffer, offset, c, off, amount);
    offset += amount;
    return amount;
  }

  /**
   * Skips n bytes of input.
   * @param n the number of characters to be skipped
   * @return    the actual number of characters skipped.
   * @exception IOException If an I/O error has occurred.
   */
  public long skip(long n) throws IOException {
    int amount = end - offset;
    if (amount >= n) {
      offset += n;
      return n;
    }
    offset = 0;
    end = 0;
    return amount + in.skip(n - amount);
  }

  ///**
  // * Returns the number of characters that can be read
  // * without blocking.
  // * @return the number of available characters.
  // */
  //public int available() throws IOException {
  //  return (end - offset) + in.available();
  //}

  /************************************************************************/

  public int peek() throws IOException {
    int amount = end - offset;
    if (amount == 0) {
      if (!fill()) return -1;
    }
    int c = buffer[offset];
    return c;
  }

  /**
   * Unread some characters. This will throw an IOException if the count
   * being unread is larger than the amount of previously read data
   * in the buffer.
   */
  public void unread(int count) throws IOException {
    if (offset < count) {
      throw new IOException("invalid unread; offset=" + offset
                + " count=" + count);
    }
    offset -= count;
  }

  /**
   * Look ahead one character. Return true if the next character matches
   * (and consume the character), otherwise return false and leave the
   * input stream unaffected.
   */
  public boolean lookAhead(char c) throws IOException {
    int amount = end - offset;
    if (amount == 0) {
      if (!fill()) return false;
    }
    if (buffer[offset] == c) {
      offset++;
      return true;
    }
    return false;
  }

  /**
   * Look ahead for a string of characters. Each character in the string
   * must match exactly otherwise false is returned (and the input stream
   * is left unaffected).
   */
  public boolean lookAhead(String s) throws IOException {
    return lookAhead(s, false);
  }

  /**
   * Look ahead for a string of characters allowing for case to not
   * matter.
   */
  public boolean lookAhead(String s, boolean ignoreCase) throws IOException {
    int slen = s.length();
    int amount = end - offset;
    if (amount < slen) {
      if (!fillForCapacity(slen)) return false;
    }
    for (int i = 0; i < slen; i++) {
      char c1 = buffer[offset+i];
      char c2 = s.charAt(i);
      if (ignoreCase) {
    c1 = Character.toLowerCase(c1);
    c2 = Character.toLowerCase(c2);
      }
      if (c1 != c2) {
    return false;
      }
    }
    offset += slen;
    return true;
  }

  /**
   * Eat up any whitespace in the buffer. White space is defined as space,
   * tab, return, newline, form feed. If any was eaten up return
   * true, otherwise return false.
   */
  /* XXX we can add an interface to specify a bitmap that defines
     which characters we want to treat as whitespace... */
  public boolean eatWhiteSpace() throws IOException {
    boolean eaten = false;
    for (;;) {
      for (int i = offset; i < end; i++) {
    char c = buffer[i];
    if ((c == ' ') || (c == '\t') || (c == '\r') || (c == '\n') ||
        (c == '\f')) {
      eaten = true;
      continue;
    }
    offset = i;
    return eaten;
      }
      if (!fill()) break;
    }
    return eaten;
  }

  /**
   * Eat up one newline if present in the buffer. If a newline
   * was eaten up return true otherwise false. This will handle
   * mac (\r), unix (\n) and windows (\r\n) style of newlines
   * transparently.
   */
  public boolean eatNewline() throws IOException {
    boolean eaten = false;
    int amount = end - offset;
    if (amount < 2) {
      if (!fillForCapacity(2)) {
    // Couldn't get two characters
    if (end - offset == 0) {
      return false;
    }
    if ((buffer[offset] == '\r') || (buffer[offset] == '\n')) {
      offset++;
      return true;
    }
    return false;
      }
    }
    if (buffer[offset] == '\r') {
      offset++;
      eaten = true;
    }
    if (buffer[offset] == '\n') {
      eaten = true;
      offset++;
    }
    return eaten;
  }

  /************************************************************************/

  protected boolean fill() throws IOException {
    if (end - offset != 0) {
      throw new IOException("fill of non-empty buffer");
    }
    offset = 0;
    end = in.read(buffer, 0, buffer.length);
    if (end < 0) {
      end = 0;
      return false;
    }
    return true;
  }

  /**
   * Fill the buffer, keeping whatever is unread in the buffer and
   * ensuring that "capacity" characters of total filled buffer
   * space is available. Return false if the final amount of data
   * in the buffer is less than capacity, otherwise return true.
   */
  protected boolean fillForCapacity(int capacity) throws IOException {
    int amount = end - offset;
    if (amount >= capacity) {
      // The buffer already holds enough data to satisfy capacity
      return true;
    }

    // The buffer needs more data. See if the buffer itself must be
    // enlarged to statisfy capacity.
    if (capacity >= buffer.length) {
      // Grow the buffer to hold enough space
      int newBufLen = buffer.length * 2;
      if (newBufLen < capacity) newBufLen = capacity;
      char newbuf[] = new char[newBufLen];
      System.arraycopy(buffer, offset, newbuf, 0, amount);
      offset = 0;
      end = amount;
      buffer = newbuf;
    } else {
      if (amount != 0) {
    // Slide the data that is currently present in the buffer down
    // to the start of the buffer
    System.arraycopy(buffer, offset, buffer, 0, amount);
    offset = 0;
    end = amount;
      }
    }

    // Fill up the remainder of the buffer
    int mustRead = capacity - amount;
    int nb = in.read(buffer, offset, buffer.length - offset);
    if (nb < mustRead) {
      if (nb > 0) {
    end += nb;
      }
      return false;
    }
    end += nb;
    return true;
  }

  public void close() throws IOException {
    in.close();
  }
}
