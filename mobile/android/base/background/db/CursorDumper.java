/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.db;

import android.database.Cursor;

/**
 * A utility for dumping a cursor the debug log.
 * <p>
 * <b>For debugging only!</p>
 */
public class CursorDumper {
  protected static String fixedWidth(int width, String s) {
    if (s == null) {
      return spaces(width);
    }
    int length = s.length();
    if (width == length) {
      return s;
    }
    if (width > length) {
      return s + spaces(width - length);
    }
    return s.substring(0, width);
  }

  protected static String spaces(int i) {
    return "                                     ".substring(0, i);
  }

  protected static String dashes(int i) {
    return "-------------------------------------".substring(0, i);
  }

  /**
   * Dump a cursor to the debug log, ignoring any log level settings.
   * <p>
   * The position in the cursor is maintained. Caller is responsible for opening
   * and closing cursor.
   *
   * @param cursor
   *          to dump.
   */
  public static void dumpCursor(Cursor cursor) {
    dumpCursor(cursor, 18, "records");
  }

  /**
   * Dump a cursor to the debug log, ignoring any log level settings.
   * <p>
   * The position in the cursor is maintained. Caller is responsible for opening
   * and closing cursor.
   *
   * @param cursor
   *          to dump.
   * @param columnWidth
   *          how many characters per cursor column.
   * @param tags
   *          a descriptor, printed like "(10 tags)", in the header row.
   */
  protected static void dumpCursor(Cursor cursor, int columnWidth, String tags) {
    int originalPosition = cursor.getPosition();
    try {
      String[] columnNames = cursor.getColumnNames();
      int columnCount      = cursor.getColumnCount();

      for (int i = 0; i < columnCount; ++i) {
        System.out.print(fixedWidth(columnWidth, columnNames[i]) + " | ");
      }
      System.out.println("(" + cursor.getCount() + " " + tags + ")");
      for (int i = 0; i < columnCount; ++i) {
        System.out.print(dashes(columnWidth) + " | ");
      }
      System.out.println("");
      if (!cursor.moveToFirst()) {
        System.out.println("EMPTY");
        return;
      }

      cursor.moveToFirst();
      while (!cursor.isAfterLast()) {
        for (int i = 0; i < columnCount; ++i) {
          System.out.print(fixedWidth(columnWidth, cursor.getString(i)) + " | ");
        }
        System.out.println("");
        cursor.moveToNext();
      }
      for (int i = 0; i < columnCount-1; ++i) {
        System.out.print(dashes(columnWidth + 3));
      }
      System.out.print(dashes(columnWidth + 3 - 1));
      System.out.println("");
    } finally {
      cursor.moveToPosition(originalPosition);
    }
  }
}
