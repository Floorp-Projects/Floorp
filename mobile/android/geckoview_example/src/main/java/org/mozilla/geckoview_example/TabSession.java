package org.mozilla.geckoview_example;

import android.os.Parcel;
import android.support.annotation.AnyThread;
import android.support.annotation.NonNull;
import android.support.annotation.UiThread;

import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoSessionSettings;
import org.mozilla.geckoview.WebExtension;

public class TabSession extends GeckoSession {
    private String mTitle;
    private String mUri;
    public WebExtension.Action action;

    public TabSession() { super(); }

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
        mUri = uri;
    }

    public void onLocationChange(@NonNull String uri) {
        mUri = uri;
    }
}
