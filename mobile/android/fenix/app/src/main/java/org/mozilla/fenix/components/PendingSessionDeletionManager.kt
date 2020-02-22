/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.app.Activity
import android.app.Application
import android.content.Context
import android.os.Bundle
import org.mozilla.fenix.ext.settings

class PendingSessionDeletionManager(application: Application) :
    Application.ActivityLifecycleCallbacks {

    private val sessionIdsPendingDeletion = mutableSetOf<String>()

    init {
        application.registerActivityLifecycleCallbacks(this)
    }

    fun addSession(sessionId: String) {
        sessionIdsPendingDeletion.add(sessionId)
    }

    fun removeSession(sessionId: String) {
        sessionIdsPendingDeletion.remove(sessionId)
    }

    fun getSessionsToDelete(context: Context): Set<String> {
        return context.settings().preferences.getStringSet(
            PREF_KEY,
            setOf()
        ) ?: setOf()
    }

    override fun onActivityPaused(activity: Activity?) {
        activity?.settings()?.preferences?.edit()?.putStringSet(
            PREF_KEY,
            sessionIdsPendingDeletion
        )?.apply()
    }

    override fun onActivityResumed(p0: Activity?) {
        /* no-op */
    }

    override fun onActivityStarted(p0: Activity?) {
        /* no-op */
    }

    override fun onActivityDestroyed(p0: Activity?) {
        /* no-op */
    }

    override fun onActivitySaveInstanceState(p0: Activity?, p1: Bundle?) {
        /* no-op */
    }

    override fun onActivityStopped(p0: Activity?) {
        /* no-op */
    }

    override fun onActivityCreated(p0: Activity?, p1: Bundle?) {
        /* no-op */
    }

    companion object {
        private const val PREF_KEY = "pref_key_session_id_set_to_delete"
    }
}
