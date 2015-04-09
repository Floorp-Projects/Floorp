package com.adjust.sdk;

import org.json.JSONObject;

/**
 * Created by pfms on 15/12/14.
 */
public interface IAttributionHandler {
    public void init(IActivityHandler activityHandler,
                     ActivityPackage attributionPackage,
                     boolean startPaused);

    public void getAttribution();

    public void checkAttribution(JSONObject jsonResponse);

    public void pauseSending();

    public void resumeSending();
}
