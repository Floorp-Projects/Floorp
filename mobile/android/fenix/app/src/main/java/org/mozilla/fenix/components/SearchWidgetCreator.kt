/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.annotation.TargetApi
import android.appwidget.AppWidgetManager
import android.content.ComponentName
import android.content.Context
import android.os.Build
import org.mozilla.gecko.search.SearchWidgetProvider

/**
 * Handles the creation of search widget.
 */
object SearchWidgetCreator {

    /**
     * Attempts to display a prompt requesting the user pin the search widget
     * Returns true if the prompt is displayed successfully, and false otherwise.
     */
    @TargetApi(Build.VERSION_CODES.O)
    fun createSearchWidget(context: Context): Boolean {
        val appWidgetManager: AppWidgetManager = context.getSystemService(AppWidgetManager::class.java)
        if (!appWidgetManager.isRequestPinAppWidgetSupported) { return false }

        val myProvider = ComponentName(context, SearchWidgetProvider::class.java)
        appWidgetManager.requestPinAppWidget(myProvider, null, null)

        return true
    }
}
