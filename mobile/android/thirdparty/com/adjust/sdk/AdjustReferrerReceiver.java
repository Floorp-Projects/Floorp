package com.adjust.sdk;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;

import static com.adjust.sdk.Constants.ENCODING;
import static com.adjust.sdk.Constants.MALFORMED;
import static com.adjust.sdk.Constants.REFERRER;

// support multiple BroadcastReceivers for the INSTALL_REFERRER:
// http://blog.appington.com/2012/08/01/giving-credit-for-android-app-installs

public class AdjustReferrerReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        String rawReferrer = intent.getStringExtra(REFERRER);
        if (null == rawReferrer) {
            return;
        }

        String referrer;
        try {
            referrer = URLDecoder.decode(rawReferrer, ENCODING);
        } catch (UnsupportedEncodingException e) {
            referrer = MALFORMED;
        }

        AdjustInstance adjust = Adjust.getDefaultInstance();
        adjust.sendReferrer(referrer);
    }
}
