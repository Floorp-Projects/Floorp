package org.mozilla.geckoview_example;

import android.support.annotation.NonNull;

import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoSessionSettings;

public class TabSession extends GeckoSession {
    private String mTitle;
    private String mUri;

    public TabSession(GeckoSessionSettings settings) {
        super(settings);
    }

    public String getTitle() {
        return mTitle == null || mTitle.length() == 0 ? "about:blank" : mTitle;
    }

    public void setTitle(String title) {
        this.mTitle = title;
    }

    public String getUri() {
        return mUri;
    }

    @Override
    public void loadUri(@NonNull String uri) {
        super.loadUri(uri);
        this.mUri = uri;
    }
}
