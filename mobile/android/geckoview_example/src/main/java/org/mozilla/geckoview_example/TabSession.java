package org.mozilla.geckoview_example;

import android.os.Parcel;
import android.support.annotation.AnyThread;
import android.support.annotation.NonNull;
import android.support.annotation.UiThread;

import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoSessionSettings;

public class TabSession extends GeckoSession {
    private String mTitle;
    private String mUri;

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


    @Override // Parcelable
    @UiThread
    public void writeToParcel(final Parcel out, final int flags) {
        super.writeToParcel(out, flags);
        out.writeString(mTitle);
        out.writeString(mUri);
    }

    // AIDL code may call readFromParcel even though it's not part of Parcelable.
    @UiThread
    public void readFromParcel(final @NonNull Parcel source) {
        super.readFromParcel(source);
        mTitle = source.readString();
        mUri = source.readString();
    }

    public static final Creator<GeckoSession> CREATOR = new Creator<GeckoSession>() {
        @Override
        @UiThread
        public TabSession createFromParcel(final Parcel in) {
            final TabSession session = new TabSession();
            session.readFromParcel(in);
            return session;
        }

        @Override
        @UiThread
        public TabSession[] newArray(final int size) {
            return new TabSession[size];
        }
    };
}
