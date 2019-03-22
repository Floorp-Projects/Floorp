/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessions

import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.session.bundling.SessionBundle
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.ViewState
import java.net.URL
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Date
import java.util.Locale

data class ArchivedSession(
    val id: Long,
    val bundle: SessionBundle,
    private val savedAt: Long,
    private val _urls: List<String>
) {
    val formattedSavedAt by lazy {
        val isSameDay: (Calendar, Calendar) -> Boolean = { a, b ->
            a.get(Calendar.ERA) == b.get(Calendar.ERA) &&
                    a.get(Calendar.YEAR) == b.get(Calendar.YEAR) &&
                    a.get(Calendar.DAY_OF_YEAR) == b.get(Calendar.DAY_OF_YEAR)
        }

        val parse: (Date) -> String = { date ->
            val dateCal = Calendar.getInstance().apply { time = date }
            val today = Calendar.getInstance()
            val yesterday = Calendar.getInstance().apply { add(Calendar.DAY_OF_YEAR, -1) }

            val time = timeFormatter.format(date)
            val month = monthFormatter.format(date)
            val day = dayFormatter.format(date)
            val dayOfWeek = dayOfWeekFormatter.format(date)

            when {
                isSameDay(dateCal, today) -> "Today @ $time"
                isSameDay(dateCal, yesterday) -> "Yesterday @ $time"
                else -> "$dayOfWeek $month/$day @ $time"
            }
        }

        parse(Date(savedAt))
    }

    val titles by lazy {
        // Until we resolve (https://github.com/mozilla-mobile/fenix/issues/532) we
        // just want to grab the host from the URL
        @SuppressWarnings("TooGenericExceptionCaught")
        val urlFormatter: (String) -> String = { url ->
            var formattedURL = try {
                URL(url).host
            } catch (e: Exception) {
                url
            }
            if (formattedURL.length > LONGEST_HOST_ON_INTERNET_LENGTH) {
                formattedURL = formattedURL.take(LONGEST_HOST_ON_INTERNET_LENGTH).plus("...")
            }
            formattedURL
        }

        _urls
            .take(NUMBER_OF_URLS_TO_DISPLAY)
            .joinToString(", ", transform = urlFormatter)
    }

    val extrasLabel = maxOf(_urls.size - NUMBER_OF_URLS_TO_DISPLAY, 0)

    private companion object {
        private const val NUMBER_OF_URLS_TO_DISPLAY = 5
        private const val LONGEST_HOST_ON_INTERNET_LENGTH = 64

        private val timeFormatter = SimpleDateFormat("h:mm a", Locale.US)
        private val monthFormatter = SimpleDateFormat("M", Locale.US)
        private val dayFormatter = SimpleDateFormat("d", Locale.US)
        private val dayOfWeekFormatter = SimpleDateFormat("EEEE", Locale.US)
    }
}

class SessionsComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    override var initialState: SessionsState = SessionsState(emptyList())
) :
    UIComponent<SessionsState, SessionsAction, SessionsChange>(
        bus.getManagedEmitter(SessionsAction::class.java),
        bus.getSafeManagedObservable(SessionsChange::class.java)
    ) {

    override val reducer: (SessionsState, SessionsChange) -> SessionsState = { state, change ->
        when (change) {
            is SessionsChange.Changed -> state.copy(archivedSessions = change.archivedSessions)
        }
    }

    override fun initView() = SessionsUIView(container, actionEmitter, changesObservable)
    val view: RecyclerView
        get() = uiView.view as RecyclerView

    init {
        render(reducer)
    }
}

data class SessionsState(val archivedSessions: List<ArchivedSession>) : ViewState

sealed class SessionsAction : Action {
    data class Select(val archivedSession: ArchivedSession) : SessionsAction()
    data class Delete(val archivedSession: ArchivedSession) : SessionsAction()
    data class MenuTapped(val archivedSession: ArchivedSession) : SessionsAction()
    data class ShareTapped(val archivedSession: ArchivedSession) : SessionsAction()
}

sealed class SessionsChange : Change {
    data class Changed(val archivedSessions: List<ArchivedSession>) : SessionsChange()
}
