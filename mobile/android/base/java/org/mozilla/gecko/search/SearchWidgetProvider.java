package org.mozilla.gecko.search;

/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.widget.RemoteViews;

import org.mozilla.gecko.LauncherActivity;
import org.mozilla.gecko.R;
import org.mozilla.gecko.mma.MmaDelegate;

import static org.mozilla.gecko.db.BrowserContract.SKIP_TAB_QUEUE_FLAG;

public final class SearchWidgetProvider extends AppWidgetProvider {
    private static final int VOICE_INTENT_RC = 1;
    private static final int TEXT_INTENT_RC = 0;
    public static String INPUT_TYPE_KEY = "input";

    public enum InputType {
        VOICE,
        TEXT
    }

    @Override
    public void onUpdate(Context context, AppWidgetManager appWidgetManager, int[] appWidgetIds) {
        final ComponentName searchWidgetName = new ComponentName(context, SearchWidgetProvider.class);
        int[] allWidgetIds = appWidgetManager.getAppWidgetIds(searchWidgetName);

        for (int widgetId : allWidgetIds) {
            final RemoteViews remoteViews = new RemoteViews(context.getPackageName(), R.layout.widget_search_default_col_layout);

            setVoicePendingIntent(context, remoteViews);
            setTextPendingIntent(context, remoteViews);

            appWidgetManager.updateAppWidget(widgetId, remoteViews);
        }
    }

    @Override
    public void onEnabled(Context context) {
        MmaDelegate.track(MmaDelegate.ADDED_SEARCH_WIDGET);
    }

    @Override
    public void onAppWidgetOptionsChanged(Context context, AppWidgetManager appWidgetManager, int appWidgetId, Bundle newOptions) {
        final Bundle options = appWidgetManager.getAppWidgetOptions(appWidgetId);
        final int minWidth = options.getInt(AppWidgetManager.OPTION_APPWIDGET_MIN_WIDTH);

        // Obtain appropriate widget and update it.
        appWidgetManager.updateAppWidget(appWidgetId, getRemoteViews(context, minWidth));
        super.onAppWidgetOptionsChanged(context, appWidgetManager, appWidgetId, newOptions);
    }

    private RemoteViews getRemoteViews(Context context, int minWidth) {
        final int columns = getCellsForSize(minWidth);
        RemoteViews remoteViews;
        switch (columns) {
            case 1:
                remoteViews = new RemoteViews(context.getPackageName(), R.layout.widget_search_1_col_layout);
                break;
            case 2:
                remoteViews = new RemoteViews(context.getPackageName(), R.layout.widget_search_2_col_layout);
                break;
            case 3:
                remoteViews = new RemoteViews(context.getPackageName(), R.layout.widget_search_3_col_layout);
                break;
            default:
                remoteViews = new RemoteViews(context.getPackageName(), R.layout.widget_search_default_col_layout);
                break;
        }

        setVoicePendingIntent(context, remoteViews);
        setTextPendingIntent(context, remoteViews);

        return remoteViews;
    }

    /**
     * Returns number of cells needed for given size of the widget.
     *
     * @param size Widget size in dp.
     * @return Size in number of cells.
     */
    private static int getCellsForSize(int size) {
        // Android offers us confidence in selecting the apropriate layout depending on the widget
        // size by guaranteeing that the minimum size of a widget is `70 * numCells - 30`
        // https://developer.android.com/guide/practices/ui_guidelines/widget_design#anatomy_determining_size
        if (size < 110) {
            return 1;
        } else if (size < 180) {
            return 2;
        } else if (size < 250) {
            return 3;
        } else {
            return 5;
        }
    }

    private static Intent makeSearchIntent(Context context, InputType inputType) {
        final Intent intent = new Intent(context, LauncherActivity.class);

        intent.putExtra(INPUT_TYPE_KEY, inputType);
        intent.putExtra(SKIP_TAB_QUEUE_FLAG, true);
        intent.setAction(Intent.ACTION_VIEW);
        intent.setData(Uri.parse("about:home"));

        return intent;
    }

    private static void setVoicePendingIntent(Context context, RemoteViews remoteViews) {
        final PendingIntent pendingIntent = PendingIntent.getActivity(context, VOICE_INTENT_RC,
                makeSearchIntent(context, InputType.VOICE), 0);
        remoteViews.setOnClickPendingIntent(R.id.iv_search_voice, pendingIntent);
    }

    private static void setTextPendingIntent(Context context, RemoteViews remoteViews) {
        final PendingIntent pendingIntent = PendingIntent.getActivity(context, TEXT_INTENT_RC,
                makeSearchIntent(context, InputType.TEXT), 0);
        remoteViews.setOnClickPendingIntent(R.id.tv_search_text, pendingIntent);
        remoteViews.setOnClickPendingIntent(R.id.iv_logo, pendingIntent);
    }
}