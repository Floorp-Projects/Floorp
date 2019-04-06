package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.graphics.Color
import android.graphics.LightingColorFilter
import android.view.View
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.session_item.*
import org.mozilla.fenix.R
import org.mozilla.fenix.home.sessioncontrol.ArchivedSession
import org.mozilla.fenix.home.sessioncontrol.ArchivedSessionAction
import org.mozilla.fenix.home.sessioncontrol.SessionControlAction
import org.mozilla.fenix.home.sessioncontrol.onNext
import java.net.URL
import java.text.SimpleDateFormat
import java.util.Locale
import java.util.Calendar
import java.util.Date

private const val NUMBER_OF_URLS_TO_DISPLAY = 5
private const val LONGEST_HOST_ON_INTERNET_LENGTH = 64

private val timeFormatter = SimpleDateFormat("h:mm a", Locale.US)
private val monthFormatter = SimpleDateFormat("M", Locale.US)
private val dayFormatter = SimpleDateFormat("d", Locale.US)
private val dayOfWeekFormatter = SimpleDateFormat("EEEE", Locale.US)

val ArchivedSession.formattedSavedAt: String
    get() = {
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
    }()

val ArchivedSession.titles: String
    get() = {
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

        urls
            .take(NUMBER_OF_URLS_TO_DISPLAY)
            .joinToString(", ", transform = urlFormatter)
    }()

val ArchivedSession.extrasLabel: Int
    get() = maxOf(urls.size - NUMBER_OF_URLS_TO_DISPLAY, 0)

class SessionViewHolder(
    view: View,
    private val actionEmitter: Observer<SessionControlAction>,
    override val containerView: View? = view
) : RecyclerView.ViewHolder(view), LayoutContainer {
    private var session: ArchivedSession? = null

    init {
        session_item.setOnClickListener {
            session?.apply { actionEmitter.onNext(ArchivedSessionAction.Select(this)) }
        }

        session_card_overflow_button.setOnClickListener {
            session?.apply { actionEmitter.onNext(ArchivedSessionAction.MenuTapped(this)) }
        }

        session_card_share_button.setOnClickListener {
            session?.apply { actionEmitter.onNext(ArchivedSessionAction.ShareTapped(this)) }
        }
    }

    fun bind(session: ArchivedSession) {
        this.session = session
        val color = availableColors[(session.id % availableColors.size).toInt()]
        session_card_thumbnail.colorFilter =
            LightingColorFilter(ContextCompat.getColor(itemView.context, color), Color.BLACK)
        session_card_timestamp.text = session.formattedSavedAt
        session_card_titles.text = session.titles
        session_card_extras.text = if (session.extrasLabel > 0) {
            "+${session.extrasLabel} sites..."
        } else { "" }
    }

    companion object {
        private val availableColors =
            listOf(
                R.color.session_placeholder_blue,
                R.color.session_placeholder_green,
                R.color.session_placeholder_orange,
                R.color.session_placeholder_purple,
                R.color.session_placeholder_pink
            )

        const val LAYOUT_ID = R.layout.session_item
    }
}
