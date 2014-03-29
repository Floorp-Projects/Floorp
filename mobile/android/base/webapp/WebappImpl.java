/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.webapp;

import java.io.File;
import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.webapp.ApkResources;
import org.mozilla.gecko.webapp.InstallHelper;
import org.mozilla.gecko.webapp.InstallHelper.InstallCallback;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager.NameNotFoundException;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.Display;
import android.view.View;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.widget.ImageView;
import android.widget.TextView;

public class WebappImpl extends GeckoApp implements InstallCallback {
    private static final String LOGTAG = "GeckoWebappImpl";

    private URL mOrigin;
    private TextView mTitlebarText = null;
    private View mTitlebar = null;

    private View mSplashscreen;

    private boolean mIsApk = true;
    private ApkResources mApkResources;
    private String mManifestUrl;
    private String mAppName;

    protected int getIndex() { return 0; }

    @Override
    public int getLayout() { return R.layout.web_app; }

    @Override
    public boolean hasTabsSideBar() { return false; }

    @Override
    public void onCreate(Bundle savedInstance)
    {

        String action = getIntent().getAction();
        Bundle extras = getIntent().getExtras();
        if (extras == null) {
            extras = savedInstance;
        }

        if (extras == null) {
            extras = new Bundle();
        }

        boolean isInstalled = extras.getBoolean("isInstalled", false);
        String packageName = extras.getString("packageName");

        if (packageName == null) {
            Log.w(LOGTAG, "no package name; treating as legacy shortcut");

            mIsApk = false;

            // Shortcut apps are already installed.
            isInstalled = true;

            Uri data = getIntent().getData();
            if (data == null) {
                Log.wtf(LOGTAG, "can't get manifest URL from shortcut data");
                setResult(RESULT_CANCELED);
                finish();
                return;
            }
            mManifestUrl = data.toString();

            String shortcutName = extras.getString(Intent.EXTRA_SHORTCUT_NAME);
            mAppName = shortcutName != null ? shortcutName : "Web App";
        } else {
            try {
                mApkResources = new ApkResources(this, packageName);
            } catch (NameNotFoundException e) {
                Log.e(LOGTAG, "Can't find package for webapp " + packageName, e);
                setResult(RESULT_CANCELED);
                finish();
                return;
            }

            mManifestUrl = mApkResources.getManifestUrl();
            mAppName = mApkResources.getAppName();
        }

        // start Gecko.
        super.onCreate(savedInstance);

        mTitlebarText = (TextView)findViewById(R.id.webapp_title);
        mTitlebar = findViewById(R.id.webapp_titlebar);
        mSplashscreen = findViewById(R.id.splashscreen);

        Allocator allocator = Allocator.getInstance(this);
        int index = getIndex();

        // We have to migrate old prefs before getting the origin because origin
        // is one of the prefs we might migrate.
        allocator.maybeMigrateOldPrefs(index);

        String origin = allocator.getOrigin(index);
        boolean isInstallCompleting = (origin == null);

        if (!GeckoThread.checkLaunchState(GeckoThread.LaunchState.GeckoRunning) || !isInstalled || isInstallCompleting) {
            // Show the splash screen if we need to start Gecko, or we need to install this.
            overridePendingTransition(R.anim.grow_fade_in_center, android.R.anim.fade_out);
            showSplash();
        } else {
            mSplashscreen.setVisibility(View.GONE);
        }

        if (!isInstalled || isInstallCompleting) {
            InstallHelper installHelper = new InstallHelper(getApplicationContext(), mApkResources, this);
            if (!isInstalled) {
                // start the vanilla install.
                try {
                    installHelper.startInstall(getDefaultProfileName());
                } catch (IOException e) {
                    Log.e(LOGTAG, "Couldn't install packaged app", e);
                }
            } else {
                // an install is already happening, so we should let it complete.
                Log.i(LOGTAG, "Waiting for existing install to complete");
                installHelper.registerGeckoListener();
            }
            return;
        } else {
            launchWebapp(origin);
        }

        setTitle(mAppName);
    }

    @Override
    protected String getURIFromIntent(Intent intent) {
        String uri = super.getURIFromIntent(intent);
        if (uri != null) {
            return uri;
        }
        // This is where we construct the URL from the Intent from the
        // the synthesized APK.

        // TODO Translate AndroidIntents into WebActivities here.
        if (mIsApk) {
            return mApkResources.getManifestUrl();
        }

        // If this is a legacy shortcut, then we should have been able to get
        // the URI from the intent data.  Otherwise, we should have been able
        // to get it from the APK resources.  So we should never get here.
        Log.wtf(LOGTAG, "Couldn't get URI from intent nor APK resources");
        return null;
    }

    @Override
    protected void loadStartupTab(String uri) {
        // NOP
    }

    private void showSplash() {

        // get the favicon dominant color, stored when the app was installed
        int dominantColor = Allocator.getInstance().getColor(getIndex());

        setBackgroundGradient(dominantColor);

        ImageView image = (ImageView)findViewById(R.id.splashscreen_icon);
        Drawable d = null;

        if (mIsApk) {
            Uri uri = mApkResources.getAppIconUri();
            image.setImageURI(uri);
            d = image.getDrawable();
        } else {
            // look for a logo.png in the profile dir and show it. If we can't find a logo show nothing
            File profile = getProfile().getDir();
            File logoFile = new File(profile, "logo.png");
            if (logoFile.exists()) {
                d = Drawable.createFromPath(logoFile.getPath());
                image.setImageDrawable(d);
            }
        }

        if (d != null) {
            Animation fadein = AnimationUtils.loadAnimation(this, R.anim.grow_fade_in_center);
            fadein.setStartOffset(500);
            fadein.setDuration(1000);
            image.startAnimation(fadein);
        }
    }

    public void setBackgroundGradient(int dominantColor) {
        int[] colors = new int[2];
        // now lighten it, to ensure that the icon stands out in the center
        float[] f = new float[3];
        Color.colorToHSV(dominantColor, f);
        f[2] = Math.min(f[2]*2, 1.0f);
        colors[0] = Color.HSVToColor(255, f);

        // now generate a second, slightly darker version of the same color
        f[2] *= 0.75;
        colors[1] = Color.HSVToColor(255, f);

        // Draw the background gradient
        GradientDrawable gd = new GradientDrawable(GradientDrawable.Orientation.TL_BR, colors);
        gd.setGradientType(GradientDrawable.RADIAL_GRADIENT);
        Display display = getWindowManager().getDefaultDisplay();
        gd.setGradientCenter(0.5f, 0.5f);
        gd.setGradientRadius(Math.max(display.getWidth()/2, display.getHeight()/2));
        mSplashscreen.setBackgroundDrawable(gd);
    }

    /* (non-Javadoc)
     * @see org.mozilla.gecko.GeckoApp#getDefaultProfileName()
     */
    @Override
    protected String getDefaultProfileName() {
        return "webapp" + getIndex();
    }

    @Override
    protected boolean getSessionRestoreState(Bundle savedInstanceState) {
        // for now webapps never restore your session
        return false;
    }

    @Override
    public void onTabChanged(Tab tab, Tabs.TabEvents msg, Object data) {
        switch(msg) {
            case SELECTED:
            case LOCATION_CHANGE:
                if (Tabs.getInstance().isSelectedTab(tab)) {
                    final String urlString = tab.getURL();
                    final URL url;

                    try {
                        url = new URL(urlString);
                    } catch (java.net.MalformedURLException ex) {
                        mTitlebarText.setText(urlString);

                        // If we can't parse the url, and its an app protocol hide
                        // the titlebar and return, otherwise show the titlebar
                        // and the full url
                        if (urlString != null && !urlString.startsWith("app://")) {
                            mTitlebar.setVisibility(View.VISIBLE);
                        } else {
                            mTitlebar.setVisibility(View.GONE);
                        }
                        return;
                    }

                    if (mOrigin != null && mOrigin.getHost().equals(url.getHost())) {
                        mTitlebar.setVisibility(View.GONE);
                    } else {
                        mTitlebarText.setText(url.getProtocol() + "://" + url.getHost());
                        mTitlebar.setVisibility(View.VISIBLE);
                    }
                }
                break;
            case LOADED:
                hideSplash();
                break;
            case START:
                if (mSplashscreen != null && mSplashscreen.getVisibility() == View.VISIBLE) {
                    View area = findViewById(R.id.splashscreen_progress);
                    area.setVisibility(View.VISIBLE);
                    Animation fadein = AnimationUtils.loadAnimation(this, android.R.anim.fade_in);
                    fadein.setDuration(1000);
                    area.startAnimation(fadein);
                }
                break;
        }
        super.onTabChanged(tab, msg, data);
    }

    protected void hideSplash() {
        if (mSplashscreen != null && mSplashscreen.getVisibility() == View.VISIBLE) {
            Animation fadeout = AnimationUtils.loadAnimation(this, android.R.anim.fade_out);
            fadeout.setAnimationListener(new Animation.AnimationListener() {
                @Override
                public void onAnimationEnd(Animation animation) {
                  mSplashscreen.setVisibility(View.GONE);
                }
                @Override
                public void onAnimationRepeat(Animation animation) { }
                @Override
                public void onAnimationStart(Animation animation) { }
            });
            mSplashscreen.startAnimation(fadeout);
        }
    }

    @Override
    public void installCompleted(InstallHelper installHelper, String event, JSONObject message) {
        if (event == null) {
            return;
        }

        if (event.equals("Webapps:Postinstall")) {
            String origin = message.optString("origin");
            launchWebapp(origin);
        }
    }

    @Override
    public void installErrored(InstallHelper installHelper, Exception exception) {
        Log.e(LOGTAG, "Install errored", exception);
    }

    private void setOrigin(String origin) {
        try {
            mOrigin = new URL(origin);
        } catch (java.net.MalformedURLException ex) {
            // If this isn't an app: URL, just settle for not having an origin.
            if (!origin.startsWith("app://")) {
                return;
            }

            // If that failed fall back to the origin stored in the shortcut.
            if (!mIsApk) {
                Log.i(LOGTAG, "Origin is app: URL; falling back to intent URL");
                Uri data = getIntent().getData();
                if (data != null) {
                    try {
                        mOrigin = new URL(data.toString());
                    } catch (java.net.MalformedURLException ex2) {
                        Log.e(LOGTAG, "Unable to parse intent URL: ", ex);
                    }
                }
            }
        }
    }

    public void launchWebapp(String origin) {
        setOrigin(origin);

        try {
            JSONObject launchObject = new JSONObject();
            launchObject.putOpt("url", mManifestUrl);
            launchObject.putOpt("name", mAppName);
            Log.i(LOGTAG, "Trying to launch: " + launchObject);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Webapps:Load", launchObject.toString()));
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error populating launch message", e);
        }
    }

    @Override
    protected boolean getIsDebuggable() {
        if (mIsApk) {
            return mApkResources.isDebuggable();
        }

        // This is a legacy shortcut, which didn't provide a way to determine
        // that the app is debuggable, so we say the app is not debuggable.
        return false;
    }
}
