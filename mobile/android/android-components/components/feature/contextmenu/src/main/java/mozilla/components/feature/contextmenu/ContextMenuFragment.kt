/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.contextmenu

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.DialogInterface
import android.os.Build
import android.os.Bundle
import android.text.Html
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.widget.AppCompatTextView
import androidx.core.text.HtmlCompat
import androidx.fragment.app.DialogFragment
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.textview.MaterialTextView
import mozilla.components.browser.state.state.SessionState

private const val EXPANDED_TITLE_MAX_LINES = 15
private const val KEY_TITLE = "title"
private const val KEY_SESSION_ID = "session_id"
private const val KEY_IDS = "ids"
private const val KEY_LABELS = "labels"
private const val KEY_ADDITIONAL_NOTE = "additional_note"

/**
 * [DialogFragment] implementation to display the actual context menu dialog.
 */
class ContextMenuFragment : DialogFragment() {
    internal var feature: ContextMenuFeature? = null

    @VisibleForTesting internal val itemIds: List<String> by lazy {
        requireArguments().getStringArrayList(KEY_IDS)!!
    }

    @VisibleForTesting internal val itemLabels: List<String> by lazy {
        requireArguments().getStringArrayList(KEY_LABELS)!!
    }

    @VisibleForTesting internal val sessionId: String by lazy {
        requireArguments().getString(KEY_SESSION_ID)!!
    }

    @VisibleForTesting internal val title: String by lazy {
        requireArguments().getString(KEY_TITLE)!!
    }

    @VisibleForTesting internal val additionalNote: String? by lazy {
        requireArguments().getString(KEY_ADDITIONAL_NOTE)
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        @SuppressLint("UseGetLayoutInflater")
        val inflater = LayoutInflater.from(requireContext())

        val builder = AlertDialog.Builder(requireContext())
            .setCustomTitle(createDialogTitleView(inflater))
            .setView(createDialogContentView(inflater))

        return builder.create()
    }

    @SuppressLint("InflateParams")
    internal fun createDialogTitleView(inflater: LayoutInflater): View {
        return inflater.inflate(
            R.layout.mozac_feature_contextmenu_title,
            null,
        ).findViewById<AppCompatTextView>(
            R.id.titleView,
        ).apply {
            text = title

            setOnClickListener {
                maxLines = EXPANDED_TITLE_MAX_LINES
            }
        }
    }

    @SuppressLint("InflateParams")
    internal fun createDialogContentView(inflater: LayoutInflater): View {
        val view = inflater.inflate(R.layout.mozac_feature_contextmenu_dialog, null)

        view.findViewById<RecyclerView>(R.id.recyclerView).apply {
            layoutManager = LinearLayoutManager(context, RecyclerView.VERTICAL, false)
            adapter = ContextMenuAdapter(this@ContextMenuFragment, inflater)
        }

        additionalNote?.let { value ->
            val additionalNoteView = view.findViewById<MaterialTextView>(R.id.additional_note)
            additionalNoteView.visibility = View.VISIBLE
            additionalNoteView.text = getSpannedValueOfString(value)
        }

        return view
    }

    private fun getSpannedValueOfString(value: String) = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
        Html.fromHtml(value, HtmlCompat.FROM_HTML_MODE_LEGACY)
    } else {
        @Suppress("Deprecation")
        Html.fromHtml(value)
    }

    internal fun onItemSelected(position: Int) {
        feature?.onMenuItemSelected(sessionId, itemIds[position])

        dismiss()
    }

    override fun onCancel(dialog: DialogInterface) {
        feature?.onMenuCancelled(sessionId)
    }

    companion object {
        /**
         * Create a new [ContextMenuFragment].
         */
        fun create(
            tab: SessionState,
            title: String,
            ids: List<String>,
            labels: List<String>,
            additionalNote: String?,
        ): ContextMenuFragment {
            val arguments = Bundle()
            arguments.putString(KEY_TITLE, title)
            arguments.putStringArrayList(KEY_IDS, ArrayList(ids))
            arguments.putStringArrayList(KEY_LABELS, ArrayList(labels))
            arguments.putString(KEY_SESSION_ID, tab.id)
            arguments.putString(KEY_ADDITIONAL_NOTE, additionalNote)

            val fragment = ContextMenuFragment()
            fragment.arguments = arguments
            return fragment
        }
    }
}

/**
 * RecyclerView adapter for displaying the context menu.
 */
internal class ContextMenuAdapter(
    private val fragment: ContextMenuFragment,
    private val inflater: LayoutInflater,
) : RecyclerView.Adapter<ContextMenuViewHolder>() {
    override fun onCreateViewHolder(parent: ViewGroup, position: Int) = ContextMenuViewHolder(
        inflater.inflate(R.layout.mozac_feature_contextmenu_item, parent, false),
    )

    override fun getItemCount(): Int = fragment.itemIds.size

    override fun onBindViewHolder(holder: ContextMenuViewHolder, position: Int) {
        val label = fragment.itemLabels[position]
        holder.labelView.text = label

        holder.itemView.setOnClickListener { fragment.onItemSelected(position) }
    }
}

/**
 * View holder for a context menu item.
 */
internal class ContextMenuViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
    internal val labelView = itemView.findViewById<TextView>(R.id.labelView)
}
