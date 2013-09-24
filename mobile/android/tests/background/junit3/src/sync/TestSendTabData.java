/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync;

import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;
import org.mozilla.gecko.sync.setup.activities.SendTabData;

import android.content.Intent;

/**
 * These tests are on device because the Intent, Pattern, and Matcher APIs are
 * stubs on desktop.
 */
public class TestSendTabData extends AndroidSyncTestCase {
  protected static Intent makeShareIntent(String text, String subject, String title) {
    Intent intent = new Intent();

    intent.putExtra(Intent.EXTRA_TEXT, text);
    intent.putExtra(Intent.EXTRA_SUBJECT, subject);
    intent.putExtra(Intent.EXTRA_TITLE, title);

    return intent;
  }

  //  From Fennec:
  //
  //  I/FxSync  ( 7420): fennec :: SendTabActivity :: Send was clicked.
  //  I/FxSync  ( 7420): fennec :: SendTabActivity :: android.intent.extra.TEXT -> http://www.reddit.com/
  //  I/FxSync  ( 7420): fennec :: SendTabActivity :: android.intent.extra.SUBJECT -> reddit: the front page of the internet
  public void testFennecBrowser() {
    Intent shareIntent = makeShareIntent("http://www.reddit.com/",
        "reddit: the front page of the internet",
        null);
    SendTabData fromIntent = SendTabData.fromIntent(shareIntent);

    assertEquals("reddit: the front page of the internet", fromIntent.title);
    assertEquals("http://www.reddit.com/", fromIntent.uri);
  }

  //  From Android Browser:
  //
  //  I/FxSync  ( 7420): fennec :: SendTabActivity :: Send was clicked.
  //  I/FxSync  ( 7420): fennec :: SendTabActivity :: android.intent.extra.TEXT -> http://bl176w.blu176.mail.live.com/m/messages.m/?mid=m95277577-e5a5-11e1-bfeb-00237de49bb0&mts=2012-08-14T00%3a18%3a44.390Z&fid=00000000-0000-0000-0000-000000000001&iru=%2fm%2ffolders.m%2f&pmid=m173216c1-e5ea-11e1-bac7-002264c17c66&pmts=2012-08-14T08%3a29%3a01.057Z&nmid=m0e0a4a3a-e511-11e1-bfe5-00237de3362a&nmts=2012-08-13T06%3a44%3a51.910Z
  //  I/FxSync  ( 7420): fennec :: SendTabActivity :: android.intent.extra.SUBJECT -> Hotmail: ONLY SIX PERFORMANCES LEFT! SPECIAL SECOND SHOW OFFER - GET $
  public void testAndroidBrowser() {
    Intent shareIntent = makeShareIntent("http://www.reddit.com/",
        "reddit: the front page of the internet",
        null);
    SendTabData fromIntent = SendTabData.fromIntent(shareIntent);

    assertEquals("reddit: the front page of the internet", fromIntent.title);
    assertEquals("http://www.reddit.com/", fromIntent.uri);
  }

  //  From Pocket:
  //
  //  I/FxSync  ( 7420): fennec :: SendTabActivity :: Send was clicked.
  //  I/FxSync  ( 7420): fennec :: SendTabActivity :: android.intent.extra.TEXT -> http://t.co/bfsbM2oV
  //  I/FxSync  ( 7420): fennec :: SendTabActivity :: android.intent.extra.SUBJECT -> Launching the Canadian OGP Civil Society Discussion Group
  //  I/FxSync  ( 7420): fennec :: SendTabActivity :: android.intent.extra.TITLE -> Launching the Canadian OGP Civil Society Discussion Group
  public void testPocket() {
    Intent shareIntent = makeShareIntent("http://t.co/bfsbM2oV",
        "Launching the Canadian OGP Civil Society Discussion Group",
        "Launching the Canadian OGP Civil Society Discussion Group");
    SendTabData fromIntent = SendTabData.fromIntent(shareIntent);

    assertEquals("Launching the Canadian OGP Civil Society Discussion Group", fromIntent.title);
    assertEquals("http://t.co/bfsbM2oV", fromIntent.uri);
  }

  //  A couple of examples from Twitter App:
  //
  //  I/FxSync  ( 7420): fennec :: SendTabActivity :: Send was clicked.
  //  I/FxSync  (17610): fennec :: SendTabActivity :: android.intent.extra.TEXT = Cory Doctorow (@doctorow) tweeted at 11:21 AM on Sat, Jan 12, 2013:
  //  I/FxSync  (17610): Pls RT: @lessig on the DoJ's vindictive prosecution of Aaron Swartz http://t.co/qNalE70n #aaronsw
  //  I/FxSync  (17610): (https://twitter.com/doctorow/status/290176681065451520)
  //  I/FxSync  (17610):
  //  I/FxSync  (17610): Get the official Twitter app at https://twitter.com/download
  //  I/FxSync  (17610): fennec :: SendTabActivity :: android.intent.extra.SUBJECT = Tweet from Cory Doctorow (@doctorow)
  //
  //  I/FxSync  ( 7420): fennec :: SendTabActivity :: Send was clicked.
  //  I/FxSync  ( 7420): fennec :: SendTabActivity :: android.intent.extra.TEXT -> David Eaves (@daeaves) tweeted at 0:08 PM on Fri, Jan 11, 2013:
  //  I/FxSync  ( 7420): New on eaves.ca: Launching the Canadian OGP Civil Society Discussion Group http://t.co/bfsbM2oV
  //  I/FxSync  ( 7420): (https://twitter.com/daeaves/status/289826143723466752)
  //  I/FxSync  ( 7420):
  //  I/FxSync  ( 7420): Get the official Twitter app at https://twitter.com/download
  //  I/FxSync  ( 7420): fennec :: SendTabActivity :: android.intent.extra.SUBJECT -> Tweet from David Eaves (@daeaves)
  public void testTwitter() {
    Intent shareIntent1 = makeShareIntent("Cory Doctorow (@doctorow) tweeted at 11:21 AM on Sat, Jan 12, 2013:\n" +
        "Pls RT: @lessig on the DoJ's vindictive prosecution of Aaron Swartz http://t.co/qNalE70n #aaronsw\n" +
        "(https://twitter.com/doctorow/status/290176681065451520)\n" +
        "\n" +
        "Get the official Twitter app at https://twitter.com/download",
        "Tweet from Cory Doctorow (@doctorow)",
        null);
    SendTabData fromIntent1 = SendTabData.fromIntent(shareIntent1);

    assertEquals("Tweet from Cory Doctorow (@doctorow)", fromIntent1.title);
    assertEquals("http://t.co/qNalE70n", fromIntent1.uri);

    Intent shareIntent2 = makeShareIntent("David Eaves (@daeaves) tweeted at 0:08 PM on Fri, Jan 11, 2013:\n" +
        "New on eaves.ca: Launching the Canadian OGP Civil Society Discussion Group http://t.co/bfsbM2oV\n" +
        "(https://twitter.com/daeaves/status/289826143723466752)\n" +
        "\n" +
        "Get the official Twitter app at https://twitter.com/download",
        "Tweet from David Eaves (@daeaves)",
        null);
    SendTabData fromIntent2 = SendTabData.fromIntent(shareIntent2);

    assertEquals("Tweet from David Eaves (@daeaves)", fromIntent2.title);
    assertEquals("http://t.co/bfsbM2oV", fromIntent2.uri);
  }
}
