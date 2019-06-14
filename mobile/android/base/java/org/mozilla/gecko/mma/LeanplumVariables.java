/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mma;

import android.content.Context;
import android.content.res.Resources;
import android.support.annotation.DrawableRes;
import android.support.annotation.NonNull;

import com.leanplum.annotations.Variable;

import org.mozilla.gecko.R;
import org.mozilla.gecko.firstrun.FirstrunPanel;
import org.mozilla.gecko.util.OnboardingStringUtil;

import java.lang.reflect.Field;

/**
 * Unified repo for all LeanPlum variables.<br>
 * <ul>To make them appear in the LP dashboard and get new values from the server
 *      <li>they must be annotated with {@link com.leanplum.annotations.Variable}.</li>
 *      <li>they need to be parsed with {@link com.leanplum.annotations.Parser} after {@link com.leanplum.Leanplum#setApplicationContext(Context)}</li>
 * </ul>
 * Although some fields are public (LP SDK limitation) they are not to be written into.
 *
 * @see <a href="https://docs.leanplum.com/reference#defining-variables">Official LP variables documentation</a>
 */
public class LeanplumVariables {
    private static LeanplumVariables INSTANCE;
    private static Resources appResources;

    private static final String FIRSTRUN_WELCOME_PANEL_GROUP_NAME = "FirstRun Welcome Panel";
    private static final String FIRSTRUN_PRIVACY_PANEL_GROUP_NAME = "FirstRun Privacy Panel";
    private static final String FIRSTRUN_CUSTOMIZE_PANEL_GROUP_NAME = "FirstRun Customize Panel";
    private static final String FIRSTRUN_SYNC_PANEL_GROUP_NAME = "FirstRun Sync Panel";

    @Variable(group = FIRSTRUN_WELCOME_PANEL_GROUP_NAME) public static String welcomePanelTitle;
    @Variable(group = FIRSTRUN_WELCOME_PANEL_GROUP_NAME) public static String welcomePanelMessage;
    @Variable(group = FIRSTRUN_WELCOME_PANEL_GROUP_NAME) public static String welcomePanelSubtext;
    @DrawableRes private static int welcomeDrawableId;

    @Variable(group = FIRSTRUN_PRIVACY_PANEL_GROUP_NAME) public static String privacyPanelTitle;
    @Variable(group = FIRSTRUN_PRIVACY_PANEL_GROUP_NAME) public static String privacyPanelMessage;
    @Variable(group = FIRSTRUN_PRIVACY_PANEL_GROUP_NAME) public static String privacyPanelSubtext;
    @DrawableRes private static int privacyDrawableId;

    @Variable(group = FIRSTRUN_CUSTOMIZE_PANEL_GROUP_NAME) public static String customizePanelTitle;
    @Variable(group = FIRSTRUN_CUSTOMIZE_PANEL_GROUP_NAME) public static String customizePanelMessage;
    @Variable(group = FIRSTRUN_CUSTOMIZE_PANEL_GROUP_NAME) public static String customizePanelSubtext;
    @DrawableRes private static int customizingDrawableId;

    @Variable(group = FIRSTRUN_SYNC_PANEL_GROUP_NAME) public static String syncPanelTitle;
    @Variable(group = FIRSTRUN_SYNC_PANEL_GROUP_NAME) public static String syncPanelMessage;
    @Variable(group = FIRSTRUN_SYNC_PANEL_GROUP_NAME) public static String syncPanelSubtext;
    @DrawableRes private static int syncDrawableId;

    /**
     * Allows constructing and/or returning an already constructed instance of this class
     * which has all it's fields populated with values from Resources.<br><br>
     *
     * An instance of this class needs exist to allow overwriting it's fields with downloaded values from LeanPlum
     * @see com.leanplum.annotations.Parser#defineFileVariable(Object, String, String, Field)
     */
    public static LeanplumVariables getInstance(Context appContext) {
        // Simple Singleton as we don't expect concurrency problems.
        if (INSTANCE == null) {
            INSTANCE = new LeanplumVariables(appContext);
        }

        return INSTANCE;
    }

    /**
     * Allows setting default values for instance variables.
     * @param context used to access application resources
     */
    private LeanplumVariables(@NonNull Context context) {
        appResources = context.getResources();

        // Same titles for the screens of the old / new onboarding UX.
        welcomePanelTitle = appResources.getString(R.string.firstrun_panel_title_welcome);
        privacyPanelTitle = appResources.getString(R.string.firstrun_panel_title_privacy);
        customizePanelTitle = appResources.getString(R.string.firstrun_panel_title_customize);
        syncPanelTitle = appResources.getString(R.string.firstrun_sync_title);

        // The new Onboarding UX uses different messages and images. Only if they are localized.
        if (OnboardingStringUtil.getInstance(context).areStringsLocalized()) {
            welcomePanelMessage = appResources.getString(R.string.newfirstrun_urlbar_message);
            welcomePanelSubtext = appResources.getString(R.string.newfirstrun_privacy_subtext);
            welcomeDrawableId = R.drawable.firstrun_welcome2;

            privacyPanelMessage = FirstrunPanel.NO_MESSAGE;
            privacyPanelSubtext = appResources.getString(R.string.newfirstrun_privacy_subtext);
            privacyDrawableId = R.drawable.firstrun_private2;

            syncPanelMessage = FirstrunPanel.NO_MESSAGE;
            syncPanelSubtext = appResources.getString(R.string.newfirstrun_sync_subtext);
            syncDrawableId = R.drawable.firstrun_sync2;
        } else {
            welcomePanelMessage = appResources.getString(R.string.firstrun_urlbar_message);
            welcomePanelSubtext = appResources.getString(R.string.firstrun_urlbar_subtext);
            welcomeDrawableId = R.drawable.firstrun_welcome;

            privacyPanelMessage = appResources.getString(R.string.firstrun_privacy_message);
            privacyPanelSubtext = appResources.getString(R.string.firstrun_privacy_subtext);
            privacyDrawableId = R.drawable.firstrun_private;

            customizePanelMessage = appResources.getString(R.string.firstrun_customize_message);
            customizePanelSubtext = appResources.getString(R.string.firstrun_customize_subtext);
            customizingDrawableId = R.drawable.firstrun_data;

            syncPanelMessage = appResources.getString(R.string.firstrun_sync_message);
            syncPanelSubtext = appResources.getString(R.string.firstrun_sync_subtext);
            syncDrawableId = R.drawable.firstrun_sync;
        }
    }

    public static int getWelcomeImage() {
        return welcomeDrawableId;
    }

    public static int getPrivacyImage() {
        return privacyDrawableId;
    }

    public static int getCustomizingImage() {
        return customizingDrawableId;
    }

    public static int getSyncImage() {
        return syncDrawableId;
    }

}
