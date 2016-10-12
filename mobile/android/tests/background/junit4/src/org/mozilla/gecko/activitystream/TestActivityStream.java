/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.activitystream;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import static org.junit.Assert.assertEquals;

@RunWith(TestRunner.class)
public class TestActivityStream {
    /**
     * Unit tests for ActivityStream.extractLabel().
     *
     * Most test cases are based on this list:
     * https://gist.github.com/nchapman/36502ad115e8825d522a66549971a3f0
     */
    @Test
    public void testExtractLabel() {
        // Empty values

        assertEquals("", ActivityStream.extractLabel(""));
        assertEquals("", ActivityStream.extractLabel(null));

        // Without path

        assertEquals("news.ycombinator",
                ActivityStream.extractLabel("https://news.ycombinator.com/"));

        assertEquals("sql.telemetry.mozilla",
                ActivityStream.extractLabel("https://sql.telemetry.mozilla.org/"));

        assertEquals("sso.mozilla",
                ActivityStream.extractLabel("http://sso.mozilla.com/"));

        assertEquals("youtube",
                ActivityStream.extractLabel("http://youtube.com/"));

        assertEquals("images.google",
                ActivityStream.extractLabel("http://images.google.com/"));

        assertEquals("smile.amazon",
                ActivityStream.extractLabel("http://smile.amazon.com/"));

        assertEquals("localhost",
                ActivityStream.extractLabel("http://localhost:5000/"));

        assertEquals("independent",
                ActivityStream.extractLabel("http://www.independent.co.uk/"));

        // With path

        assertEquals("firefox",
                ActivityStream.extractLabel("https://addons.mozilla.org/en-US/firefox/"));

        assertEquals("activity-stream",
                ActivityStream.extractLabel("https://trello.com/b/KX3hV8XS/activity-stream"));

        assertEquals("activity-stream",
                ActivityStream.extractLabel("https://github.com/mozilla/activity-stream"));

        assertEquals("sidekiq",
                ActivityStream.extractLabel("https://dispatch-news.herokuapp.com/sidekiq"));

        assertEquals("nchapman",
                ActivityStream.extractLabel("https://github.com/nchapman/"));

        // Unusable paths

        assertEquals("phonebook.mozilla", // instead of "login"
                ActivityStream.extractLabel("https://phonebook.mozilla.org/mellon/login?ReturnTo=https%3A%2F%2Fphonebook.mozilla.org%2F&IdP=http%3A%2F%2Fwww.okta.com"));

        assertEquals("ipay.adp", // instead of "index.jsf"
                ActivityStream.extractLabel("https://ipay.adp.com/iPay/index.jsf"));

        assertEquals("calendar.google", // instead of "render"
                ActivityStream.extractLabel("https://calendar.google.com/calendar/render?pli=1#main_7"));

        assertEquals("myworkday", // instead of "home.htmld"
                ActivityStream.extractLabel("https://www.myworkday.com/vhr_mozilla/d/home.htmld"));

        assertEquals("mail.google", // instead of "1"
                ActivityStream.extractLabel("https://mail.google.com/mail/u/1/#inbox"));

        assertEquals("docs.google", // instead of "edit"
                ActivityStream.extractLabel("https://docs.google.com/presentation/d/11cyrcwhKTmBdEBIZ3szLO0-_Imrx2CGV2B9_LZHDrds/edit#slide=id.g15d41bb0f3_0_82"));

        // Special cases

        assertEquals("irccloud.mozilla",
                ActivityStream.extractLabel("https://irccloud.mozilla.com/#!/ircs://irc1.dmz.scl3.mozilla.com:6697/%23universal-search"));
    }
}
