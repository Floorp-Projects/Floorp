/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.activitystream;

import android.os.SystemClock;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.shadows.ShadowLooper;

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
        assertLabelEquals("", "", true);
        assertLabelEquals("", null, true);

        // Without path
        assertLabelEquals("news.ycombinator", "https://news.ycombinator.com/", true);
        assertLabelEquals("sql.telemetry.mozilla", "https://sql.telemetry.mozilla.org/", true);
        assertLabelEquals("sso.mozilla", "http://sso.mozilla.com/", true);
        assertLabelEquals("youtube", "http://youtube.com/", true);
        assertLabelEquals("images.google", "http://images.google.com/", true);
        assertLabelEquals("smile.amazon", "http://smile.amazon.com/", true);
        assertLabelEquals("localhost", "http://localhost:5000/", true);
        assertLabelEquals("independent", "http://www.independent.co.uk/", true);

        // With path
        assertLabelEquals("firefox", "https://addons.mozilla.org/en-US/firefox/", true);
        assertLabelEquals("activity-stream", "https://trello.com/b/KX3hV8XS/activity-stream", true);
        assertLabelEquals("activity-stream", "https://github.com/mozilla/activity-stream", true);
        assertLabelEquals("sidekiq", "https://dispatch-news.herokuapp.com/sidekiq", true);
        assertLabelEquals("nchapman", "https://github.com/nchapman/", true);

        // Unusable paths
        assertLabelEquals("phonebook.mozilla","https://phonebook.mozilla.org/mellon/login?ReturnTo=https%3A%2F%2Fphonebook.mozilla.org%2F&IdP=http%3A%2F%2Fwww.okta.com", true);
        assertLabelEquals("ipay.adp", "https://ipay.adp.com/iPay/index.jsf", true);
        assertLabelEquals("calendar.google", "https://calendar.google.com/calendar/render?pli=1#main_7", true);
        assertLabelEquals("myworkday", "https://www.myworkday.com/vhr_mozilla/d/home.htmld", true);
        assertLabelEquals("mail.google", "https://mail.google.com/mail/u/1/#inbox", true);
        assertLabelEquals("docs.google", "https://docs.google.com/presentation/d/11cyrcwhKTmBdEBIZ3szLO0-_Imrx2CGV2B9_LZHDrds/edit#slide=id.g15d41bb0f3_0_82", true);

        // Special cases
        assertLabelEquals("irccloud.mozilla", "https://irccloud.mozilla.com/#!/ircs://irc1.dmz.scl3.mozilla.com:6697/%23universal-search", true);
    }

    @Test
    public void testExtractLabelWithoutPath() {
        assertLabelEquals("addons.mozilla", "https://addons.mozilla.org/en-US/firefox/", false);
        assertLabelEquals("trello", "https://trello.com/b/KX3hV8XS/activity-stream", false);
        assertLabelEquals("github", "https://github.com/mozilla/activity-stream", false);
        assertLabelEquals("dispatch-news", "https://dispatch-news.herokuapp.com/sidekiq", false);
        assertLabelEquals("github", "https://github.com/nchapman/", false);
    }

    private void assertLabelEquals(String expectedLabel, String url, boolean usePath) {
        final String[] actualLabel = new String[1];

        ActivityStream.LabelCallback callback = new ActivityStream.LabelCallback() {
            @Override
            public void onLabelExtracted(String label) {
                actualLabel[0] = label;
            }
        };

        ActivityStream.extractLabel(RuntimeEnvironment.application, url, usePath, callback);

        ShadowLooper.runUiThreadTasks();

        assertEquals(expectedLabel, actualLabel[0]);
    }
}
