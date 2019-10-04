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
import org.mozilla.gecko.util.OnboardingResources;

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
    private static final String FIRSTRUN_CUSTOMIZE_PANEL_GROUP_NAME = "FirstRun Customize Panel";
    private static final String FIRSTRUN_SYNC_PANEL_GROUP_NAME = "FirstRun Sync Panel";
    private static final String FIRSTRUN_SEND_TAB_PANEL_GROUP_NAME = "FirstRun Send Tab Panel";

    @Variable(group = FIRSTRUN_WELCOME_PANEL_GROUP_NAME) public static String welcomePanelTitle;
    @Variable(group = FIRSTRUN_WELCOME_PANEL_GROUP_NAME) public static String welcomePanelMessage;
    @Variable(group = FIRSTRUN_WELCOME_PANEL_GROUP_NAME) public static String welcomePanelSubtext;
    @DrawableRes private static int welcomeDrawableId;

    @Variable(group = FIRSTRUN_CUSTOMIZE_PANEL_GROUP_NAME) public static String customizePanelTitle;
    @Variable(group = FIRSTRUN_CUSTOMIZE_PANEL_GROUP_NAME) public static String customizePanelMessage;
    @Variable(group = FIRSTRUN_CUSTOMIZE_PANEL_GROUP_NAME) public static String customizePanelSubtext;
    @DrawableRes private static int customizingDrawableId;

    @Variable(group = FIRSTRUN_SYNC_PANEL_GROUP_NAME) public static String syncPanelTitle;
    @Variable(group = FIRSTRUN_SYNC_PANEL_GROUP_NAME) public static String syncPanelMessage;
    @Variable(group = FIRSTRUN_SYNC_PANEL_GROUP_NAME) public static String syncPanelSubtext;
    @DrawableRes private static int syncDrawableId;

    @Variable(group = FIRSTRUN_SEND_TAB_PANEL_GROUP_NAME) public static String sendTabPanelTitle;
    @Variable(group = FIRSTRUN_SEND_TAB_PANEL_GROUP_NAME) public static String sendTabPanelMessage;
    @Variable(group = FIRSTRUN_SEND_TAB_PANEL_GROUP_NAME) public static String sendTabPanelSubtext;
    @DrawableRes private static int sendTabDrawableId;

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
        customizePanelTitle = appResources.getString(R.string.firstrun_panel_title_customize);

        // The new Onboarding UX uses different messages and images. Only if they are localized.
        OnboardingResources onboardingUtil = OnboardingResources.getInstance(context);
        syncPanelTitle = onboardingUtil.getSyncTitle();

        sendTabPanelTitle = appResources.getString(R.string.firstrun_sendtab_title);

        if (onboardingUtil.useNewOnboarding()) {
            welcomePanelMessage = onboardingUtil.getWelcomeMessage();
            welcomePanelSubtext = onboardingUtil.getWelcomeSubtext();
            welcomeDrawableId = R.drawable.firstrun_welcome2;

            syncPanelMessage = FirstrunPanel.NO_MESSAGE;
            syncPanelSubtext = onboardingUtil.getSyncSubtext();
            syncDrawableId = onboardingUtil.getSyncImageResId();

            sendTabPanelMessage = FirstrunPanel.NO_MESSAGE;
            sendTabPanelSubtext = appResources.getString(R.string.firstrun_sendtab_message);
            sendTabDrawableId = R.drawable.firstrun_sendtab;

        } else {
            welcomePanelMessage = appResources.getString(R.string.firstrun_urlbar_message);
            welcomePanelSubtext = appResources.getString(R.string.firstrun_urlbar_subtext);
            welcomeDrawableId = R.drawable.firstrun_welcome;

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

    public static int getCustomizingImage() {
        return customizingDrawableId;
    }

    public static int getSyncImage() {
        return syncDrawableId;
    }

    public static int getSendTabImage() {
        return sendTabDrawableId;
    }

}
