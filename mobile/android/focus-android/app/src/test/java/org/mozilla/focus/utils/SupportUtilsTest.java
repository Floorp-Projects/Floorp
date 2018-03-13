package org.mozilla.focus.utils;

import android.content.Context;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.Locale;

import static org.junit.Assert.assertEquals;

@RunWith(RobolectricTestRunner.class)
public class SupportUtilsTest {

    @Test
    public void cleanup() {
        // Other tests might get confused by our locale fiddling, so lets go back to the default:
        Locale.setDefault(Locale.ENGLISH);
    }

    /*
     * Super simple sumo URL test - it exists primarily to verify that we're setting the language
     * and page tags correctly. appVersion is null in tests, so we just test that there's a null there,
     * which doesn't seem too useful...
     */
    @Test
    public void getSumoURLForTopic() throws Exception {
        final Context context = RuntimeEnvironment.application;
        final String versionName = context.getPackageManager().getPackageInfo(context.getPackageName(), 0).versionName;

        final SupportUtils.SumoTopic testTopic = SupportUtils.SumoTopic.TRACKERS;
        final String testTopicStr = testTopic.topicStr;

        Locale.setDefault(Locale.GERMANY);
        assertEquals("https://support.mozilla.org/1/mobile/" + versionName + "/Android/de-DE/" + testTopicStr,
                SupportUtils.getSumoURLForTopic(RuntimeEnvironment.application, testTopic));

        Locale.setDefault(Locale.CANADA_FRENCH);
        assertEquals("https://support.mozilla.org/1/mobile/" + versionName + "/Android/fr-CA/" + testTopicStr,
                SupportUtils.getSumoURLForTopic(RuntimeEnvironment.application, testTopic));
    }

    /**
     * This is a pretty boring tests - it exists primarily to verify that we're actually setting
     * a langtag in the manfiesto URL.
     */
    @Test
    public void getManifestoURL() throws Exception {
        Locale.setDefault(Locale.UK);
        assertEquals("https://www.mozilla.org/en-GB/about/manifesto/",
                SupportUtils.getManifestoURL());

        Locale.setDefault(Locale.KOREA);
        assertEquals("https://www.mozilla.org/ko-KR/about/manifesto/",
                SupportUtils.getManifestoURL());
    }

}