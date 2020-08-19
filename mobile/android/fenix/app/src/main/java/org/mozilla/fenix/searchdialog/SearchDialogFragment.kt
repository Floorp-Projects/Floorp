/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.searchdialog

import android.app.Dialog
import android.content.DialogInterface
import android.graphics.Typeface
import android.os.Bundle
import android.text.style.StyleSpan
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.constraintlayout.widget.ConstraintProperties.BOTTOM
import androidx.constraintlayout.widget.ConstraintProperties.PARENT_ID
import androidx.constraintlayout.widget.ConstraintProperties.TOP
import androidx.constraintlayout.widget.ConstraintSet
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import kotlinx.android.synthetic.main.fragment_search.*
import kotlinx.android.synthetic.main.fragment_search_dialog.*
import kotlinx.android.synthetic.main.fragment_search_dialog.pill_wrapper
import kotlinx.android.synthetic.main.fragment_search_dialog.search_scan_button
import kotlinx.android.synthetic.main.fragment_search_dialog.toolbar
import kotlinx.android.synthetic.main.fragment_search_dialog.view.*
import kotlinx.android.synthetic.main.fragment_search_dialog.view.search_engines_shortcut_button
import kotlinx.android.synthetic.main.fragment_search_dialog.view.search_scan_button
import kotlinx.android.synthetic.main.fragment_search_dialog.view.toolbar
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.feature.qr.QrFeature
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.content.hasCamera
import mozilla.components.support.ktx.android.content.res.getSpanned
import mozilla.components.support.ktx.android.view.hideKeyboard
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.search.SearchFragmentStore
import org.mozilla.fenix.search.SearchInteractor
import org.mozilla.fenix.search.awesomebar.AwesomeBarView
import org.mozilla.fenix.search.createInitialSearchFragmentState
import org.mozilla.fenix.search.toolbar.ToolbarView

typealias SearchDialogFragmentStore = SearchFragmentStore
typealias SearchDialogInteractor = SearchInteractor

class SearchDialogFragment : AppCompatDialogFragment(), UserInteractionHandler {

    private lateinit var interactor: SearchDialogInteractor
    private lateinit var store: SearchDialogFragmentStore
    private lateinit var toolbarView: ToolbarView
    private lateinit var awesomeBarView: AwesomeBarView
    private var firstUpdate = true

    private val qrFeature = ViewBoundFeatureWrapper<QrFeature>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, R.style.SearchDialogStyle)
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return object : Dialog(requireContext(), this.theme) {
            override fun onBackPressed() {
                this@SearchDialogFragment.onBackPressed()
            }
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val args by navArgs<SearchDialogFragmentArgs>()
        val view = inflater.inflate(R.layout.fragment_search_dialog, container, false)

        store = SearchDialogFragmentStore(
            createInitialSearchFragmentState(
                activity as HomeActivity,
                requireComponents,
                tabId = args.sessionId,
                pastedText = args.pastedText,
                searchAccessPoint = args.searchAccessPoint
            )
        )

        interactor = SearchDialogInteractor(
            SearchDialogController(
                activity = requireActivity() as HomeActivity,
                sessionManager = requireComponents.core.sessionManager,
                store = store,
                navController = findNavController(),
                settings = requireContext().settings(),
                metrics = requireComponents.analytics.metrics,
                clearToolbarFocus = {
                    toolbarView.view.hideKeyboard()
                    toolbarView.view.clearFocus()
                }
            )
        )

        toolbarView = ToolbarView(
            requireContext(),
            interactor,
            null,
            false,
            view.toolbar,
            requireComponents.core.engine
        )

        awesomeBarView = AwesomeBarView(
            requireContext(),
            interactor,
            view.awesome_bar
        )

        return view
    }

    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        setupConstraints(view)

        search_wrapper.setOnClickListener {
            it.hideKeyboard()
            dismissAllowingStateLoss()
        }

        view.search_engines_shortcut_button.setOnClickListener {
            interactor.onSearchShortcutsButtonClicked()
        }

        search_scan_button.visibility = if (context?.hasCamera() == true) View.VISIBLE else View.GONE

        search_scan_button.setOnClickListener {
            if (!requireContext().hasCamera()) { return@setOnClickListener }

            toolbarView.view.clearFocus()
            requireComponents.analytics.metrics.track(Event.QRScannerOpened)
            qrFeature.get()?.scan(R.id.search_wrapper)
        }

        qrFeature.set(
            QrFeature(
                requireContext(),
                fragmentManager = childFragmentManager,
                onNeedToRequestPermissions = { permissions ->
                    requestPermissions(permissions, REQUEST_CODE_CAMERA_PERMISSIONS)
                },
                onScanResult = { result ->
                    search_scan_button.isChecked = false
                    activity?.let {
                        AlertDialog.Builder(it).apply {
                            val spannable = resources.getSpanned(
                                R.string.qr_scanner_confirmation_dialog_message,
                                getString(R.string.app_name) to StyleSpan(Typeface.BOLD),
                                result to StyleSpan(Typeface.ITALIC)
                            )
                            setMessage(spannable)
                            setNegativeButton(R.string.qr_scanner_dialog_negative) { dialog: DialogInterface, _ ->
                                requireComponents.analytics.metrics.track(Event.QRScannerNavigationDenied)
                                dialog.cancel()
                            }
                            setPositiveButton(R.string.qr_scanner_dialog_positive) { dialog: DialogInterface, _ ->
                                requireComponents.analytics.metrics.track(Event.QRScannerNavigationAllowed)
                                (activity as HomeActivity)
                                    .openToBrowserAndLoad(
                                        searchTermOrURL = result,
                                        newTab = store.state.tabId == null,
                                        from = BrowserDirection.FromSearch
                                    )
                                dialog.dismiss()
                            }
                            create()
                        }.show()
                        requireComponents.analytics.metrics.track(Event.QRScannerPromptDisplayed)
                    }
                }),
            owner = this,
            view = view
        )

        consumeFrom(store) {
            val shouldShowAwesomebar =
                !firstUpdate &&
                it.query.isNotBlank() ||
                it.showSearchShortcuts

            awesome_bar?.visibility = if (shouldShowAwesomebar) View.VISIBLE else View.INVISIBLE
            toolbarView.update(it)
            awesomeBarView.update(it)
            firstUpdate = false
        }
    }

    override fun onBackPressed(): Boolean {
        return when {
            qrFeature.onBackPressed() -> {
                toolbarView.view.edit.focus()
                view?.search_scan_button?.isChecked = false
                toolbarView.view.requestFocus()
                true
            }
            else -> {
                view?.hideKeyboard()
                dismissAllowingStateLoss()
                true
            }
        }
    }

    private fun setupConstraints(view: View) {
        if (view.context.settings().toolbarPosition == ToolbarPosition.BOTTOM) {
            ConstraintSet().apply {
                clone(search_wrapper)

                clear(toolbar.id, TOP)
                connect(toolbar.id, BOTTOM, PARENT_ID, BOTTOM)

                clear(awesome_bar.id, TOP)
                clear(pill_wrapper.id, BOTTOM)
                connect(awesome_bar.id, TOP, PARENT_ID, TOP)
                connect(pill_wrapper.id, BOTTOM, toolbar.id, TOP)

                applyTo(search_wrapper)
            }
        }
    }

    companion object {
        private const val REQUEST_CODE_CAMERA_PERMISSIONS = 1
    }
}
