/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session;

import android.os.Bundle;
import android.support.annotation.NonNull;
import android.text.TextUtils;

import org.mozilla.focus.architecture.NonNullLiveData;
import org.mozilla.focus.architecture.NonNullMutableLiveData;
import org.mozilla.focus.customtabs.CustomTabConfig;

import java.util.UUID;

/**
 * Keeping track of state / data of a single browsing session (tab).
 */
public class Session {
    private final Source source;
    private final String uuid;
    private final NonNullMutableLiveData<String> url;
    private final NonNullMutableLiveData<Integer> progress;
    private final NonNullMutableLiveData<Boolean> secure;
    private final NonNullMutableLiveData<Boolean> loading;
    private final NonNullMutableLiveData<Integer> trackersBlocked;
    private CustomTabConfig customTabConfig;
    private Bundle webviewState;
    private String searchTerms;
    private String searchUrl;
    private boolean isRecorded;
    private boolean isBlockingEnabled;

    /* package */ Session(Source source, String url) {
        this.uuid = UUID.randomUUID().toString();
        this.source = source;

        this.url = new NonNullMutableLiveData<>(url);
        this.progress = new NonNullMutableLiveData<>(0);
        this.secure = new NonNullMutableLiveData<>(false);
        this.loading = new NonNullMutableLiveData<>(false);
        this.trackersBlocked = new NonNullMutableLiveData<>(0);

        this.isBlockingEnabled = true;
        this.isRecorded = false;
    }

    /* package */ Session(String url, @NonNull CustomTabConfig customTabConfig) {
        this(Source.CUSTOM_TAB, url);

        this.customTabConfig = customTabConfig;
    }

    public Source getSource() {
        return source;
    }

    public String getUUID() {
        return uuid;
    }

    /* package */ void setUrl(String url) {
        this.url.setValue(url);
    }

    public NonNullLiveData<String> getUrl() {
        return url;
    }

    /* package */ void setProgress(int progress) {
        this.progress.setValue(progress);
    }

    public NonNullLiveData<Integer> getProgress() {
        return progress;
    }

    /* package */ void setSecure(boolean secure) {
        this.secure.setValue(secure);
    }

    public NonNullLiveData<Boolean> getSecure() {
        return secure;
    }

    /* package */ void setLoading(boolean loading) {
        this.loading.setValue(loading);
    }

    public NonNullLiveData<Boolean> getLoading() {
        return loading;
    }

    /* package */ void setTrackersBlocked(int trackersBlocked) {
        this.trackersBlocked.postValue(trackersBlocked);
    }

    /* package */ void clearSearchTerms() {
        searchTerms = null;
    }

    public NonNullLiveData<Integer> getBlockedTrackers() {
        return trackersBlocked;
    }

    public void saveWebViewState(Bundle bundle) {
        this.webviewState = bundle;
    }

    public Bundle getWebViewState() {
        return webviewState;
    }

    public boolean hasWebViewState() {
        return webviewState != null;
    }

    public boolean isCustomTab() {
        return customTabConfig != null;
    }

    public CustomTabConfig getCustomTabConfig() {
        return customTabConfig;
    }

    public boolean isRecorded() {
        return isRecorded;
    }

    public void markAsRecorded() {
        isRecorded = true;
    }

    public boolean isSearch() {
        return !TextUtils.isEmpty(searchTerms);
    }

    public void setSearchTerms(String searchTerms) {
        this.searchTerms = searchTerms;
    }

    public String getSearchTerms() {
        return searchTerms;
    }

    public String getSearchUrl() {
        return searchUrl;
    }

    public void setSearchUrl(String searchUrl) {
        this.searchUrl = searchUrl;
    }

    public boolean isSameAs(@NonNull Session session) {
        return uuid.equals(session.getUUID());
    }

    public boolean isBlockingEnabled() {
        return isBlockingEnabled;
    }

    public void setBlockingEnabled(boolean blockingEnabled) {
        this.isBlockingEnabled = blockingEnabled;
    }
}
