/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import android.content.ContentValues;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import org.mozilla.gecko.db.DBUtils;

import java.io.File;
import java.io.IOException;

public class testDBUtils extends BaseTest {
    public void testDBUtils() throws IOException {
        final File cacheDir = getInstrumentation().getContext().getCacheDir();
        final File dbFile = File.createTempFile("testDBUtils", ".db", cacheDir);
        final SQLiteDatabase db = SQLiteDatabase.openOrCreateDatabase(dbFile, null);
        try {
            mAsserter.ok(db != null, "Created DB.", null);
            db.execSQL("CREATE TABLE foo (x INTEGER NOT NULL DEFAULT 0, y TEXT)");
            final ContentValues v = new ContentValues();
            v.put("x", 5);
            v.put("y", "a");
            db.insert("foo", null, v);
            v.put("x", 2);
            v.putNull("y");
            db.insert("foo", null, v);
            v.put("x", 3);
            v.put("y", "z");
            db.insert("foo", null, v);

            DBUtils.UpdateOperation[] ops = {DBUtils.UpdateOperation.BITWISE_OR, DBUtils.UpdateOperation.ASSIGN};
            ContentValues[] values = {new ContentValues(), new ContentValues()};
            values[0].put("x", 0xff);
            values[1].put("y", "hello");

            final int updated = DBUtils.updateArrays(db, "foo", values, ops, "x >= 3", null);

            mAsserter.ok(updated == 2, "Updated two rows.", null);
            final Cursor out = db.query("foo", new String[]{"x", "y"}, null, null, null, null, "x");
            try {
                mAsserter.ok(out.moveToNext(), "Has first result.", null);
                mAsserter.ok(2 == out.getInt(0), "1: First column was untouched.", null);
                mAsserter.ok(out.isNull(1), "1: Second column was untouched.", null);

                mAsserter.ok(out.moveToNext(), "Has second result.", null);
                mAsserter.ok((0xff | 3) == out.getInt(0), "2: First column was ORed correctly.", null);
                mAsserter.ok("hello".equals(out.getString(1)), "2: Second column was assigned correctly.", null);

                mAsserter.ok(out.moveToNext(), "Has third result.", null);
                mAsserter.ok((0xff | 5) == out.getInt(0), "3: First column was ORed correctly.", null);
                mAsserter.ok("hello".equals(out.getString(1)), "3: Second column was assigned correctly.", null);

                mAsserter.ok(!out.moveToNext(), "No more results.", null);
            } finally {
                out.close();
            }

        } finally {
            try {
                db.close();
            } catch (Exception e) {
            }
            dbFile.delete();
        }
    }
}
