/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.open

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.Context
import android.content.Intent
import android.content.pm.ActivityInfo
import android.net.Uri
import android.os.Bundle
import android.view.ContextThemeWrapper
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.LayoutRes
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.fragment.app.commit
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialog
import mozilla.components.support.utils.ext.getParcelableArrayCompat
import mozilla.components.support.utils.ext.getParcelableCompat
import org.mozilla.focus.GleanMetrics.OpenWith.ListItemTappedExtra
import org.mozilla.focus.GleanMetrics.OpenWith.listItemTapped
import org.mozilla.focus.R
import org.mozilla.focus.ext.isTablet
import org.mozilla.focus.open.AppAdapter.OnAppSelectedListener

/**
 * [AppCompatDialogFragment] used to display open in app options.
 */
class OpenWithFragment : AppCompatDialogFragment(), OnAppSelectedListener {
    override fun onPause() {
        requireActivity().supportFragmentManager.commit(allowStateLoss = true) {
            remove(this@OpenWithFragment)
        }
        super.onPause()
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val wrapper = ContextThemeWrapper(context, android.R.style.Theme_Material_Light)

        @SuppressLint("InflateParams") // This View will have it's params ignored anyway:
        val view = LayoutInflater.from(wrapper).inflate(R.layout.fragment_open_with, null)
        val dialog: Dialog = CustomWidthBottomSheetDialog(wrapper)
        dialog.setContentView(view)
        val appList = view.findViewById<RecyclerView>(R.id.apps)
        appList.layoutManager = LinearLayoutManager(
            wrapper,
            RecyclerView.VERTICAL,
            false,
        )
        val adapter = requireArguments().getParcelableArrayCompat(ARGUMENT_KEY_APPS, ActivityInfo::class.java)
            ?.let { infoArray ->
                AppAdapter(
                    wrapper,
                    infoArray,
                    requireArguments().getParcelableCompat(ARGUMENT_STORE, ActivityInfo::class.java),
                )
            }

        adapter?.setOnAppSelectedListener(this)
        appList.adapter = adapter
        return dialog
    }

    internal class CustomWidthBottomSheetDialog(context: Context) : BottomSheetDialog(context) {
        private var contentView: View? = null
        override fun onCreate(savedInstanceState: Bundle?) {
            super.onCreate(savedInstanceState)

            // The support library makes the bottomsheet full width on all devices (and then uses a 16:9
            // keyline). On tablets, the system bottom sheets use a narrower width - lets do that too:
            if (context.isTablet()) {
                val width =
                    context.resources.getDimensionPixelSize(R.dimen.tablet_bottom_sheet_width)
                val window = window
                window?.setLayout(width, ViewGroup.LayoutParams.MATCH_PARENT)
            }
        }

        override fun setContentView(view: View) {
            super.setContentView(view)
            contentView = view
        }

        override fun setContentView(@LayoutRes layoutResID: Int) {
            throw IllegalStateException("CustomWidthBottomSheetDialog only supports setContentView(View)")
        }

        override fun setContentView(view: View, params: ViewGroup.LayoutParams?) {
            throw IllegalStateException("CustomWidthBottomSheetDialog only supports setContentView(View)")
        }

        override fun show() {
            if (context.isTablet()) {
                val peekHeight =
                    context.resources.getDimensionPixelSize(R.dimen.tablet_bottom_sheet_peekheight)
                val bsBehaviour = BottomSheetBehavior.from(
                    contentView!!.parent as View,
                )
                bsBehaviour.peekHeight = peekHeight
            }
            super.show()
        }
    }

    override fun onAppSelected(app: AppAdapter.App) {
        val intent = Intent(Intent.ACTION_VIEW, Uri.parse(requireArguments().getString(ARGUMENT_URL)))
        intent.setClassName(app.packageName, app.activityName)
        startActivity(intent)

        listItemTapped.record(ListItemTappedExtra(app.packageName.contains("mozilla")))

        dismiss()
    }

    companion object {
        const val FRAGMENT_TAG = "open_with"
        private const val ARGUMENT_KEY_APPS = "apps"
        private const val ARGUMENT_URL = "url"
        private const val ARGUMENT_STORE = "store"

        /**
         * Creates a new instance of [AppCompatDialogFragment].
         */
        fun newInstance(
            apps: Array<ActivityInfo?>?,
            url: String?,
            store: ActivityInfo?,
        ): OpenWithFragment {
            val arguments = Bundle()
            arguments.putParcelableArray(ARGUMENT_KEY_APPS, apps)
            arguments.putString(ARGUMENT_URL, url)
            arguments.putParcelable(ARGUMENT_STORE, store)
            val fragment = OpenWithFragment()
            fragment.arguments = arguments
            return fragment
        }
    }
}
