package org.mozilla.gecko.tests;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;

/**
 * Tests session OOM restore behavior.
 *
 * Loads a session and tests that it is restored correctly.
 */
public class testSessionOOMRestore extends SessionTest {
    private Session mSession;
    private static final String PREFS_NAME = "GeckoApp";
    private static final String PREFS_ALLOW_STATE_BUNDLE = "allowStateBundle";

    @Override
    public void setActivityIntent(Intent intent) {
        PageInfo home = new PageInfo("about:home");
        PageInfo page1 = new PageInfo("page1");
        PageInfo page2 = new PageInfo("page2");
        PageInfo page3 = new PageInfo("page3");
        PageInfo page4 = new PageInfo("page4");
        PageInfo page5 = new PageInfo("page5");
        PageInfo page6 = new PageInfo("page6");

        SessionTab tab1 = new SessionTab(0, home, page1, page2);
        SessionTab tab2 = new SessionTab(1, home, page3, page4);
        SessionTab tab3 = new SessionTab(2, home, page5, page6);

        mSession = new Session(1, tab1, tab2, tab3);

        String sessionString = buildSessionJSON(mSession);
        writeProfileFile("sessionstore.js", sessionString);

        // This feature is pref-protected to prevent other apps from injecting
        // a state bundle, so enable it here.
        SharedPreferences prefs = getInstrumentation().getTargetContext()
                .getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        prefs.edit().putBoolean(PREFS_ALLOW_STATE_BUNDLE, true).commit();

        Bundle bundle = new Bundle();
        bundle.putString("privateSession", null);
        intent.putExtra("stateBundle", bundle);

        super.setActivityIntent(intent);
    }

    public void testSessionOOMRestore() throws Exception {
        blockForGeckoReady();
        verifySessionTabs(mSession);
    }
}
