/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.widget

import android.app.PendingIntent
import android.appwidget.AppWidgetManager
import android.appwidget.AppWidgetManager.OPTION_APPWIDGET_MIN_WIDTH
import android.appwidget.AppWidgetProvider
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.view.View
import android.widget.RemoteViews
import androidx.annotation.Dimension
import androidx.annotation.Dimension.Companion.DP
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.graphics.drawable.toBitmap
import mozilla.components.feature.search.R
import mozilla.components.feature.search.widget.BaseVoiceSearchActivity.Companion.SPEECH_PROCESSING
import mozilla.components.support.utils.PendingIntentUtils

/**
 * An abstract [AppWidgetProvider] that implements core behaviour needed to support a Search Widget
 * on the launcher.
 */
abstract class AppSearchWidgetProvider : AppWidgetProvider() {

    override fun onUpdate(
        context: Context,
        appWidgetManager: AppWidgetManager,
        appWidgetIds: IntArray,
    ) {
        val textSearchIntent = createTextSearchIntent(context)
        val voiceSearchIntent = createVoiceSearchIntent(context)

        appWidgetIds.forEach { appWidgetId ->
            updateWidgetLayout(
                context = context,
                appWidgetId = appWidgetId,
                appWidgetManager = appWidgetManager,
                voiceSearchIntent = voiceSearchIntent,
                textSearchIntent = textSearchIntent,
            )
        }
    }

    override fun onAppWidgetOptionsChanged(
        context: Context,
        appWidgetManager: AppWidgetManager,
        appWidgetId: Int,
        newOptions: Bundle?,
    ) {
        val textSearchIntent = createTextSearchIntent(context)
        val voiceSearchIntent = createVoiceSearchIntent(context)

        updateWidgetLayout(
            context = context,
            appWidgetId = appWidgetId,
            appWidgetManager = appWidgetManager,
            voiceSearchIntent = voiceSearchIntent,
            textSearchIntent = textSearchIntent,
        )
    }

    /**
     * Builds pending intent that opens the browser and starts a new text search.
     */
    abstract fun createTextSearchIntent(context: Context): PendingIntent

    /**
     * If the microphone will appear on the Search Widget and the user can perform a voice search.
     */
    abstract fun shouldShowVoiceSearch(context: Context): Boolean

    /**
     * Activity that extends BaseVoiceSearchActivity.
     */
    abstract fun voiceSearchActivity(): Class<out BaseVoiceSearchActivity>

    /**
     * Config that sets the icons and the strings for search widget.
     */
    abstract val config: SearchWidgetConfig

    /**
     * Builds pending intent that starts a new voice search.
     */
    @VisibleForTesting
    internal fun createVoiceSearchIntent(context: Context): PendingIntent? {
        if (!shouldShowVoiceSearch(context)) {
            return null
        }

        val voiceIntent = Intent(context, voiceSearchActivity()).apply {
            flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
            putExtra(SPEECH_PROCESSING, true)
        }

        return PendingIntent.getActivity(
            context,
            REQUEST_CODE_VOICE,
            voiceIntent,
            PendingIntentUtils.defaultFlags,
        )
    }

    private fun updateWidgetLayout(
        context: Context,
        appWidgetId: Int,
        appWidgetManager: AppWidgetManager,
        voiceSearchIntent: PendingIntent?,
        textSearchIntent: PendingIntent,
    ) {
        val currentWidth =
            appWidgetManager.getAppWidgetOptions(appWidgetId).getInt(OPTION_APPWIDGET_MIN_WIDTH)
        val layoutSize = getLayoutSize(currentWidth)
        // It's not enough to just hide the microphone on the "small" sized widget due to its design.
        // The "small" widget needs a complete redesign, meaning it needs a new layout file.
        val showMic = (voiceSearchIntent != null)
        val layout = getLayout(layoutSize, showMic)
        val text = getText(layoutSize, context)

        val views =
            createRemoteViews(context, layout, textSearchIntent, voiceSearchIntent, text)
        appWidgetManager.updateAppWidget(appWidgetId, views)
    }

    private fun createRemoteViews(
        context: Context,
        layout: Int,
        textSearchIntent: PendingIntent,
        voiceSearchIntent: PendingIntent?,
        text: String?,
    ): RemoteViews {
        return RemoteViews(context.packageName, layout).apply {
            setSearchWidgetIcon(context)
            setMicrophoneIcon(context)
            when (layout) {
                R.layout.mozac_search_widget_extra_small_v1,
                R.layout.mozac_search_widget_extra_small_v2,
                R.layout.mozac_search_widget_small_no_mic,
                -> {
                    setOnClickPendingIntent(
                        R.id.mozac_button_search_widget_new_tab,
                        textSearchIntent,
                    )
                }
                R.layout.mozac_search_widget_small -> {
                    setOnClickPendingIntent(
                        R.id.mozac_button_search_widget_new_tab,
                        textSearchIntent,
                    )
                    setOnClickPendingIntent(
                        R.id.mozac_button_search_widget_voice,
                        voiceSearchIntent,
                    )
                }
                R.layout.mozac_search_widget_medium,
                R.layout.mozac_search_widget_large,
                -> {
                    setOnClickPendingIntent(
                        R.id.mozac_button_search_widget_new_tab,
                        textSearchIntent,
                    )
                    setOnClickPendingIntent(
                        R.id.mozac_button_search_widget_voice,
                        voiceSearchIntent,
                    )
                    setOnClickPendingIntent(
                        R.id.mozac_button_search_widget_new_tab_icon,
                        textSearchIntent,
                    )
                    setTextViewText(R.id.mozac_button_search_widget_new_tab, text)

                    // Unlike "small" widget, "medium" and "large" sizes do not have separate layouts
                    // that exclude the microphone icon, which is why we must hide it accordingly here.
                    if (voiceSearchIntent == null) {
                        setViewVisibility(R.id.mozac_button_search_widget_voice, View.GONE)
                    }
                }
            }
        }
    }

    private fun RemoteViews.setMicrophoneIcon(context: Context) {
        setImageView(
            context,
            R.id.mozac_button_search_widget_voice,
            config.searchWidgetMicrophoneResource,
        )
    }

    private fun RemoteViews.setSearchWidgetIcon(context: Context) {
        setImageView(
            context,
            R.id.mozac_button_search_widget_new_tab_icon,
            config.searchWidgetIconResource,
        )
        val appName = context.getString(config.appName)
        setContentDescription(
            R.id.mozac_button_search_widget_new_tab_icon,
            context.getString(R.string.search_widget_content_description, appName),
        )
    }

    private fun RemoteViews.setImageView(context: Context, viewId: Int, resourceId: Int) {
        // gradient color available for android:fillColor only on SDK 24+
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            setImageViewResource(
                viewId,
                resourceId,
            )
        } else {
            setImageViewBitmap(
                viewId,
                AppCompatResources.getDrawable(
                    context,
                    resourceId,
                )?.toBitmap(),
            )
        }
    }

    // Cell sizes obtained from the actual dimensions listed in search widget specs.
    companion object {
        private const val DP_EXTRA_SMALL = 64
        private const val DP_SMALL = 100
        private const val DP_MEDIUM = 192
        private const val DP_LARGE = 256
        private const val REQUEST_CODE_VOICE = 1

        /**
         * It updates AppSearchWidgetProvider size and microphone icon visibility.
         */
        fun updateAllWidgets(context: Context, clazz: Class<out AppSearchWidgetProvider>) {
            val widgetManager = AppWidgetManager.getInstance(context)
            val widgetIds = widgetManager.getAppWidgetIds(
                ComponentName(
                    context,
                    clazz,
                ),
            )
            if (widgetIds.isNotEmpty()) {
                context.sendBroadcast(
                    Intent(context, clazz).apply {
                        action = AppWidgetManager.ACTION_APPWIDGET_UPDATE
                        putExtra(AppWidgetManager.EXTRA_APPWIDGET_IDS, widgetIds)
                    },
                )
            }
        }

        @VisibleForTesting
        internal fun getLayoutSize(@Dimension(unit = DP) dp: Int) = when {
            dp >= DP_LARGE -> SearchWidgetProviderSize.LARGE
            dp >= DP_MEDIUM -> SearchWidgetProviderSize.MEDIUM
            dp >= DP_SMALL -> SearchWidgetProviderSize.SMALL
            dp >= DP_EXTRA_SMALL -> SearchWidgetProviderSize.EXTRA_SMALL_V2
            else -> SearchWidgetProviderSize.EXTRA_SMALL_V1
        }

        /**
         * Get the layout resource to use for the search widget.
         */
        @VisibleForTesting
        internal fun getLayout(size: SearchWidgetProviderSize, showMic: Boolean) = when (size) {
            SearchWidgetProviderSize.LARGE -> R.layout.mozac_search_widget_large
            SearchWidgetProviderSize.MEDIUM -> R.layout.mozac_search_widget_medium
            SearchWidgetProviderSize.SMALL -> {
                if (showMic) {
                    R.layout.mozac_search_widget_small
                } else {
                    R.layout.mozac_search_widget_small_no_mic
                }
            }
            SearchWidgetProviderSize.EXTRA_SMALL_V2 -> R.layout.mozac_search_widget_extra_small_v2
            SearchWidgetProviderSize.EXTRA_SMALL_V1 -> R.layout.mozac_search_widget_extra_small_v1
        }

        /**
         * Get the text to place in the search widget.
         */
        @VisibleForTesting
        internal fun getText(layout: SearchWidgetProviderSize, context: Context) = when (layout) {
            SearchWidgetProviderSize.MEDIUM -> context.getString(R.string.search_widget_text_short)
            SearchWidgetProviderSize.LARGE -> context.getString(R.string.search_widget_text_long)
            else -> null
        }
    }
}

/**
 * Client App can set from this config icons and the app name for search widget.
 */
data class SearchWidgetConfig(
    val searchWidgetIconResource: Int,
    val searchWidgetMicrophoneResource: Int,
    val appName: Int,
)

internal enum class SearchWidgetProviderSize {
    EXTRA_SMALL_V1,
    EXTRA_SMALL_V2,
    SMALL,
    MEDIUM,
    LARGE,
}
