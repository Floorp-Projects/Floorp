/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.ui

import android.text.SpannableStringBuilder
import android.text.Spanned
import android.text.format.DateUtils
import android.text.method.LinkMovementMethod
import android.text.style.ClickableSpan
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.R
import mozilla.components.lib.crash.db.CrashWithReports
import mozilla.components.lib.crash.db.ReportEntity

/**
 * RecyclerView adapter for displaying the list of crashes.
 */
internal class CrashListAdapter(
    private val crashReporter: CrashReporter,
    private val onSelection: (String) -> Unit
) : RecyclerView.Adapter<CrashViewHolder>() {
    private var crashes: List<CrashWithReports> = emptyList()

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): CrashViewHolder {
        val view = LayoutInflater.from(
            parent.context
        ).inflate(
            R.layout.mozac_lib_crash_item_crash,
            parent,
            false
        )

        return CrashViewHolder(view)
    }

    override fun getItemCount(): Int {
        return crashes.size
    }

    override fun onBindViewHolder(holder: CrashViewHolder, position: Int) {
        val crashWithReports = crashes[position]

        holder.idView.text = crashWithReports.crash.uuid

        holder.titleView.text = crashWithReports.crash.stacktrace.lines().first()

        val time = DateUtils.getRelativeDateTimeString(
            holder.footerView.context,
            crashWithReports.crash.createdAt,
            DateUtils.MINUTE_IN_MILLIS,
            DateUtils.WEEK_IN_MILLIS,
            0
        )

        holder.footerView.text = SpannableStringBuilder(time).apply {
            if (crashWithReports.reports.isNotEmpty()) {
                append(" - ")
                append(crashReporter, crashWithReports.reports, onSelection)
            }
        }
    }

    fun updateList(list: List<CrashWithReports>) {
        crashes = list
        notifyDataSetChanged()
    }
}

internal class CrashViewHolder(
    view: View
) : RecyclerView.ViewHolder(
    view
) {
    val titleView = view.findViewById<TextView>(R.id.mozac_lib_crash_title)
    val idView = view.findViewById<TextView>(R.id.mozac_lib_crash_id)
    val footerView = view.findViewById<TextView>(R.id.mozac_lib_crash_footer).apply {
        movementMethod = LinkMovementMethod.getInstance()
    }
}

private fun SpannableStringBuilder.append(
    crashReporter: CrashReporter,
    services: List<ReportEntity>,
    onSelection: (String) -> Unit
): SpannableStringBuilder {
    services.forEachIndexed { index, entity ->
        val service = crashReporter.getCrashReporterServiceById(entity.serviceId)
        val name = service?.name ?: entity.serviceId
        val url = service?.createCrashReportUrl(entity.reportId)

        if (url != null) {
            append(name, object : ClickableSpan() {
                override fun onClick(widget: View) {
                    onSelection(url)
                }
            }, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
        } else {
            append(name)
        }

        if (index < services.lastIndex) {
            append(" ")
        }
    }
    return this
}
