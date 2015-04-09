package com.adjust.sdk.plugin;

import android.content.Context;

import java.util.Map;

/**
 * Created by pfms on 18/09/14.
 */
public interface Plugin {
    Map.Entry<String, String> getParameter(Context context);
}
