package com.adjust.sdk;

import android.net.Uri;

import org.json.JSONObject;

/**
 * Created by pfms on 15/12/14.
 */
public interface IActivityHandler {
    public void init(AdjustConfig config);

    public void trackSubsessionStart();

    public void trackSubsessionEnd();

    public void trackEvent(AdjustEvent event);

    public void finishedTrackingActivity(JSONObject jsonResponse);

    public void setEnabled(boolean enabled);

    public boolean isEnabled();

    public void readOpenUrl(Uri url, long clickTime);

    public boolean tryUpdateAttribution(AdjustAttribution attribution);

    public void sendReferrer(String referrer, long clickTime);

    public void setOfflineMode(boolean enabled);

    public void setAskingAttribution(boolean askingAttribution);

    public ActivityPackage getAttributionPackage();
}
