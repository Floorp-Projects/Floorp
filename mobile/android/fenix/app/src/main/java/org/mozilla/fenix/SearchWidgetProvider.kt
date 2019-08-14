/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.app.PendingIntent
import android.appwidget.AppWidgetManager
import android.appwidget.AppWidgetManager.OPTION_APPWIDGET_MIN_WIDTH
import android.appwidget.AppWidgetProvider
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.widget.RemoteViews
import org.mozilla.fenix.utils.Settings

class SearchWidgetProvider : AppWidgetProvider() {

    override fun onEnabled(context: Context) {
        Settings.instance?.addSearchWidgetInstalled(1)
    }

    override fun onDeleted(context: Context, appWidgetIds: IntArray) {
        Settings.instance?.addSearchWidgetInstalled(-appWidgetIds.size)
    }

    override fun onUpdate(context: Context, appWidgetManager: AppWidgetManager, appWidgetIds: IntArray) {
        val textSearchIntent = createTextSearchIntent(context)
        val voiceSearchIntent = createVoiceSearchIntent(context)

        appWidgetIds.forEach { appWidgetId ->
            val currentWidth = appWidgetManager.getAppWidgetOptions(appWidgetId).getInt(OPTION_APPWIDGET_MIN_WIDTH)
            val layoutSize = getLayoutSize(currentWidth)
            val layout = getLayout(layoutSize)
            val text = getText(layoutSize, context)

            val views = createRemoteViews(context, layout, textSearchIntent, voiceSearchIntent, text)
            appWidgetManager.updateAppWidget(appWidgetId, views)
        }
    }

    override fun onAppWidgetOptionsChanged(
        context: Context,
        appWidgetManager: AppWidgetManager,
        appWidgetId: Int,
        newOptions: Bundle?
    ) {
        val textSearchIntent = createTextSearchIntent(context)
        val voiceSearchIntent = createVoiceSearchIntent(context)

        val currentWidth = appWidgetManager.getAppWidgetOptions(appWidgetId).getInt(OPTION_APPWIDGET_MIN_WIDTH)
        val layoutSize = getLayoutSize(currentWidth)
        val layout = getLayout(layoutSize)
        val text = getText(layoutSize, context)

        val views = createRemoteViews(context, layout, textSearchIntent, voiceSearchIntent, text)
        appWidgetManager.updateAppWidget(appWidgetId, views)
    }

    private fun getLayoutSize(dp: Int) = when {
        dp >= DP_LARGE -> SearchWidgetProviderSize.LARGE
        dp >= DP_MEDIUM -> SearchWidgetProviderSize.MEDIUM
        dp >= DP_SMALL -> SearchWidgetProviderSize.SMALL
        dp >= DP_EXTRA_SMALL -> SearchWidgetProviderSize.EXTRA_SMALL_V2
        else -> SearchWidgetProviderSize.EXTRA_SMALL_V1
    }

    private fun getLayout(size: SearchWidgetProviderSize) = when (size) {
        SearchWidgetProviderSize.LARGE -> R.layout.search_widget_large
        SearchWidgetProviderSize.MEDIUM -> R.layout.search_widget_medium
        SearchWidgetProviderSize.SMALL -> R.layout.search_widget_small
        SearchWidgetProviderSize.EXTRA_SMALL_V2 -> R.layout.search_widget_extra_small_v2
        SearchWidgetProviderSize.EXTRA_SMALL_V1 -> R.layout.search_widget_extra_small_v1
    }

    private fun getText(layout: SearchWidgetProviderSize, context: Context) = when (layout) {
        SearchWidgetProviderSize.MEDIUM -> context.getString(R.string.search_widget_text_short)
        SearchWidgetProviderSize.LARGE -> context.getString(R.string.search_widget_text_long)
        else -> null
    }

    private fun createTextSearchIntent(context: Context): PendingIntent {
        return Intent(context, HomeActivity::class.java)
            .let { intent ->
                intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
                intent.putExtra(HomeActivity.OPEN_TO_SEARCH, true)
                PendingIntent.getActivity(context, REQUEST_CODE_NEW_TAB, intent, PendingIntent.FLAG_UPDATE_CURRENT)
            }
    }

    private fun createVoiceSearchIntent(context: Context): PendingIntent {
        return Intent(context, IntentReceiverActivity::class.java)
            .let { intent ->
                intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
                intent.putExtra(IntentReceiverActivity.SPEECH_PROCESSING, true)
                PendingIntent.getActivity(context, REQUEST_CODE_VOICE, intent, 0)
            }
    }

    private fun createRemoteViews(
        context: Context,
        layout: Int,
        textSearchIntent: PendingIntent,
        voiceSearchIntent: PendingIntent,
        text: String?
    ): RemoteViews {
        return RemoteViews(context.packageName, layout).apply {
            when (layout) {
                R.layout.search_widget_extra_small_v1,
                R.layout.search_widget_extra_small_v2 -> {
                    setOnClickPendingIntent(R.id.button_search_widget_new_tab, textSearchIntent)
                }
                R.layout.search_widget_small -> {
                    setOnClickPendingIntent(R.id.button_search_widget_new_tab, textSearchIntent)
                    setOnClickPendingIntent(R.id.button_search_widget_voice, voiceSearchIntent)
                }
                R.layout.search_widget_medium,
                R.layout.search_widget_large -> {
                    setOnClickPendingIntent(R.id.button_search_widget_new_tab, textSearchIntent)
                    setOnClickPendingIntent(R.id.button_search_widget_voice, voiceSearchIntent)
                    setTextViewText(R.id.button_search_widget_new_tab, text)
                }
            }
        }
    }

    // Cell sizes obtained from the actual dimensions listed in search widget specs
    companion object {
        private const val DP_EXTRA_SMALL = 64
        private const val DP_SMALL = 100
        private const val DP_MEDIUM = 192
        private const val DP_LARGE = 256
        private const val REQUEST_CODE_NEW_TAB = 0
        private const val REQUEST_CODE_VOICE = 1
    }
}

enum class SearchWidgetProviderSize {
    EXTRA_SMALL_V1,
    EXTRA_SMALL_V2,
    SMALL,
    MEDIUM,
    LARGE,
}
