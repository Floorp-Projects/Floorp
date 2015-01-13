/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import java.io.File;

import org.mozilla.gecko.db.BrowserContract.Passwords;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;

/**
 * A basic password contentprovider test.
 * - inserts a password when the database is not yet set up
 * - inserts a password
 * - updates a password
 * - deletes a password
 */
public class testPasswordProvider extends BaseTest {
    private static final String DB_NAME = "signons.sqlite";

    public void testPasswordProvider() {
        Context context = (Context)getActivity();
        ContentResolver cr = context.getContentResolver();
        ContentValues[] cvs = new ContentValues[1];
        cvs[0] = new ContentValues();
  
        blockForGeckoReady();
  
        cvs[0].put("hostname", "http://www.example.com");
        cvs[0].put("httpRealm", "http://www.example.com");
        cvs[0].put("formSubmitURL", "http://www.example.com");
        cvs[0].put("usernameField", "usernameField");
        cvs[0].put("passwordField", "passwordField");
        cvs[0].put("encryptedUsername", "username");
        cvs[0].put("encryptedPassword", "password");
        cvs[0].put("encType", "1");

        // Attempt to insert into the db
        Uri passwordUri = Passwords.CONTENT_URI;
        Uri.Builder builder = passwordUri.buildUpon();
        passwordUri = builder.appendQueryParameter("profilePath", mProfile).build();

        Uri uri = cr.insert(passwordUri, cvs[0]);
        Uri expectedUri = passwordUri.buildUpon().appendPath("1").build();
        mAsserter.is(uri.toString(), expectedUri.toString(), "Insert returned correct uri");
        Cursor c = cr.query(passwordUri, null, null, null, null);
        SqliteCompare(c, cvs);
  
        cvs[0].put("usernameField", "usernameField2");
        cvs[0].put("passwordField", "passwordField2");
  
        int numUpdated = cr.update(passwordUri, cvs[0], null, null);
        mAsserter.is(1, numUpdated, "Correct number updated");
        c = cr.query(passwordUri, null, null, null, null);
        SqliteCompare(c, cvs);
  
        int numDeleted = cr.delete(passwordUri, null, null);
        mAsserter.is(1, numDeleted, "Correct number deleted");
        cvs = new ContentValues[0];
        c = cr.query(passwordUri, null, null, null, null);
        SqliteCompare(c, cvs);
    }

    @Override
    public void tearDown() throws Exception {
        // remove the entire signons.sqlite file
        File profile = new File(mProfile);
        File db = new File(profile, "signons.sqlite");
        if (db.delete()) {
            mAsserter.dumpLog("tearDown deleted "+db.toString());
        } else {
            mAsserter.dumpLog("tearDown did not delete "+db.toString());
        }

        super.tearDown();
    }
}
