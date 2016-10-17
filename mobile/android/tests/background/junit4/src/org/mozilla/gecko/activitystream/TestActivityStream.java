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
    public void testExtractLabelWithPath() {
        // Empty values

        assertEquals("", ActivityStream.extractLabel("", true));
        assertEquals("", ActivityStream.extractLabel(null, true));

        // Without path

        assertEquals("news.ycombinator",
                ActivityStream.extractLabel("https://news.ycombinator.com/", true));

        assertEquals("sql.telemetry.mozilla",
                ActivityStream.extractLabel("https://sql.telemetry.mozilla.org/", true));

        assertEquals("sso.mozilla",
                ActivityStream.extractLabel("http://sso.mozilla.com/", true));

        assertEquals("youtube",
                ActivityStream.extractLabel("http://youtube.com/", true));

        assertEquals("images.google",
                ActivityStream.extractLabel("http://images.google.com/", true));

        assertEquals("smile.amazon",
                ActivityStream.extractLabel("http://smile.amazon.com/", true));

        assertEquals("localhost",
                ActivityStream.extractLabel("http://localhost:5000/", true));

        assertEquals("independent",
                ActivityStream.extractLabel("http://www.independent.co.uk/", true));

        // With path

        assertEquals("firefox",
                ActivityStream.extractLabel("https://addons.mozilla.org/en-US/firefox/", true));

        assertEquals("activity-stream",
                ActivityStream.extractLabel("https://trello.com/b/KX3hV8XS/activity-stream", true));

        assertEquals("activity-stream",
                ActivityStream.extractLabel("https://github.com/mozilla/activity-stream", true));

        assertEquals("sidekiq",
                ActivityStream.extractLabel("https://dispatch-news.herokuapp.com/sidekiq", true));

        assertEquals("nchapman",
                ActivityStream.extractLabel("https://github.com/nchapman/", true));

        // Unusable paths

        assertEquals("phonebook.mozilla", // instead of "login"
                ActivityStream.extractLabel("https://phonebook.mozilla.org/mellon/login?ReturnTo=https%3A%2F%2Fphonebook.mozilla.org%2F&IdP=http%3A%2F%2Fwww.okta.com", true));

        assertEquals("ipay.adp", // instead of "index.jsf"
                ActivityStream.extractLabel("https://ipay.adp.com/iPay/index.jsf", true));

        assertEquals("calendar.google", // instead of "render"
                ActivityStream.extractLabel("https://calendar.google.com/calendar/render?pli=1#main_7", true));

        assertEquals("myworkday", // instead of "home.htmld"
                ActivityStream.extractLabel("https://www.myworkday.com/vhr_mozilla/d/home.htmld", true));

        assertEquals("mail.google", // instead of "1"
                ActivityStream.extractLabel("https://mail.google.com/mail/u/1/#inbox", true));

        assertEquals("docs.google", // instead of "edit"
                ActivityStream.extractLabel("https://docs.google.com/presentation/d/11cyrcwhKTmBdEBIZ3szLO0-_Imrx2CGV2B9_LZHDrds/edit#slide=id.g15d41bb0f3_0_82", true));

        // Special cases

        assertEquals("irccloud.mozilla",
                ActivityStream.extractLabel("https://irccloud.mozilla.com/#!/ircs://irc1.dmz.scl3.mozilla.com:6697/%23universal-search", true));
    }

    @Test
    public void testExtractLabelWithoutPath() {
        assertEquals("addons.mozilla",
                ActivityStream.extractLabel("https://addons.mozilla.org/en-US/firefox/", false));

        assertEquals("trello",
                ActivityStream.extractLabel("https://trello.com/b/KX3hV8XS/activity-stream", false));

        assertEquals("github",
                ActivityStream.extractLabel("https://github.com/mozilla/activity-stream", false));

        assertEquals("dispatch-news",
                ActivityStream.extractLabel("https://dispatch-news.herokuapp.com/sidekiq", false));

        assertEquals("github",
                ActivityStream.extractLabel("https://github.com/nchapman/", false));
    }
}
