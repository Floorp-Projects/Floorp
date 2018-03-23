package org.mozilla.geckoview.test;

import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.mozglue.SafeIntent;
import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoSessionSettings;
import org.mozilla.geckoview.GeckoView;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

public class TestRunnerActivity extends Activity {
    private static final String LOGTAG = "TestRunnerActivity";

    GeckoSession mSession;
    GeckoView mView;

    private GeckoSession.NavigationDelegate mNavigationDelegate = new GeckoSession.NavigationDelegate() {
        @Override
        public void onLocationChange(GeckoSession session, String url) {
            getActionBar().setSubtitle(url);
        }

        @Override
        public void onCanGoBack(GeckoSession session, boolean canGoBack) {

        }

        @Override
        public void onCanGoForward(GeckoSession session, boolean canGoForward) {

        }

        @Override
        public void onLoadRequest(GeckoSession session, String uri,
                                  int target, GeckoSession.Response<Boolean> response) {
            // Allow Gecko to load all URIs
            response.respond(false);
        }

        @Override
        public void onNewSession(GeckoSession session, String uri, GeckoSession.Response<GeckoSession> response) {
            response.respond(createSession(session.getSettings()));
        }
    };

    private GeckoSession.ContentDelegate mContentDelegate = new GeckoSession.ContentDelegate() {
        @Override
        public void onTitleChange(GeckoSession session, String title) {

        }

        @Override
        public void onFocusRequest(GeckoSession session) {

        }

        @Override
        public void onCloseRequest(GeckoSession session) {
            session.close();
        }

        @Override
        public void onFullScreen(GeckoSession session, boolean fullScreen) {

        }

        @Override
        public void onContextMenu(GeckoSession session, int screenX, int screenY, String uri, int elementType, String elementSrc) {

        }
    };

    private GeckoSession createSession() {
        return createSession(null);
    }

    private GeckoSession createSession(GeckoSessionSettings settings) {
        if (settings == null) {
            settings = new GeckoSessionSettings();

            // We can't use e10s because we get deadlocked when quickly creating and
            // destroying sessions. Bug 1348361.
            settings.setBoolean(GeckoSessionSettings.USE_MULTIPROCESS, false);
        }

        final GeckoSession session = new GeckoSession(settings);
        session.setNavigationDelegate(mNavigationDelegate);
        return session;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final Intent intent = getIntent();
        GeckoSession.preload(this, new String[] { "-purgecaches" },
                             intent.getExtras(), false /* no multiprocess, see below */);

        // We can't use e10s because we get deadlocked when quickly creating and
        // destroying sessions. Bug 1348361.
        mSession = createSession();
        mSession.open(this);

        // If we were passed a URI in the Intent, open it
        final Uri uri = intent.getData();
        if (uri != null) {
            mSession.loadUri(uri);
        }

        mView = new GeckoView(this);
        mView.setSession(mSession);
        setContentView(mView);
    }

    @Override
    protected void onDestroy() {
        mSession.close();
        super.onDestroy();
    }

    public GeckoView getGeckoView() {
        return mView;
    }

    public GeckoSession getGeckoSession() {
        return mSession;
    }
}
