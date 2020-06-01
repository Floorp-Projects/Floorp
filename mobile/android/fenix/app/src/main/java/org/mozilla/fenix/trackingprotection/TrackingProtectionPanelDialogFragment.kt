/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.app.Dialog
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.LinearLayout
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.appcompat.view.ContextThemeWrapper
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.whenStarted
import androidx.navigation.fragment.navArgs
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialog
import kotlinx.android.synthetic.main.fragment_tracking_protection.view.*
import kotlinx.coroutines.launch
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.lib.state.ext.observe
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents

class TrackingProtectionPanelDialogFragment : AppCompatDialogFragment(), UserInteractionHandler {

    private val args by navArgs<TrackingProtectionPanelDialogFragmentArgs>()

    private fun inflateRootView(container: ViewGroup? = null): View {
        val contextThemeWrapper = ContextThemeWrapper(
            activity,
            (activity as HomeActivity).themeManager.currentThemeResource
        )
        return LayoutInflater.from(contextThemeWrapper).inflate(
            R.layout.fragment_tracking_protection,
            container,
            false
        )
    }

    private lateinit var trackingProtectionStore: TrackingProtectionStore
    private lateinit var trackingProtectionView: TrackingProtectionPanelView
    private lateinit var trackingProtectionInteractor: TrackingProtectionPanelInteractor
    private lateinit var trackingProtectionUseCases: TrackingProtectionUseCases

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val components = requireComponents
        trackingProtectionUseCases = TrackingProtectionUseCases(
            sessionManager = components.core.sessionManager,
            engine = components.core.engine
        )
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflateRootView(container)
        val session = requireComponents.core.sessionManager.findSessionById(args.sessionId)
        session?.register(sessionObserver, view = view)
        trackingProtectionStore = StoreProvider.get(this) {
            TrackingProtectionStore(
                TrackingProtectionState(
                    session,
                    args.url,
                    args.trackingProtectionEnabled,
                    listTrackers = listOf(),
                    mode = TrackingProtectionState.Mode.Normal,
                    lastAccessedCategory = ""
                )
            )
        }
        trackingProtectionInteractor = TrackingProtectionPanelInteractor(
            trackingProtectionStore,
            ::toggleTrackingProtection,
            ::openTrackingProtectionSettings
        )
        trackingProtectionView =
            TrackingProtectionPanelView(view.fragment_tp, trackingProtectionInteractor)
        session?.let { updateTrackers(it) }
        return view
    }

    private val sessionObserver = object : Session.Observer {
        override fun onUrlChanged(session: Session, url: String) {
            trackingProtectionStore.dispatch(TrackingProtectionAction.UrlChange(url))
        }

        override fun onTrackerBlocked(session: Session, tracker: Tracker, all: List<Tracker>) {
            updateTrackers(session)
        }

        override fun onTrackerLoaded(session: Session, tracker: Tracker, all: List<Tracker>) {
            updateTrackers(session)
        }
    }

    private fun updateTrackers(session: Session) {
        trackingProtectionUseCases.fetchTrackingLogs(
            session,
            onSuccess = {
                trackingProtectionStore.dispatch(TrackingProtectionAction.TrackerLogChange(it))
            },
            onError = {
                Logger.error("TrackingProtectionUseCases - fetchTrackingLogs onError", it)
            }
        )
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        trackingProtectionStore.observe(view) {
            viewLifecycleOwner.lifecycleScope.launch {
                whenStarted {
                    trackingProtectionView.update(it)
                }
            }
        }
    }

    private fun openTrackingProtectionSettings() {
        requireContext().metrics.track(Event.TrackingProtectionSettingsPanel)
        nav(
            R.id.trackingProtectionPanelDialogFragment,
            TrackingProtectionPanelDialogFragmentDirections.actionGlobalTrackingProtectionFragment()
        )
    }

    private fun toggleTrackingProtection(isEnabled: Boolean) {
        context?.let { context ->
            val session = context.components.core.sessionManager.findSessionById(args.sessionId)
            session?.let {
                if (isEnabled) {
                    trackingProtectionUseCases.removeException(it)
                } else {
                    context.metrics.track(Event.TrackingProtectionException)
                    trackingProtectionUseCases.addException(it)
                }

                with(context.components) {
                    useCases.sessionUseCases.reload.invoke(session)
                }
            }
        }
        trackingProtectionStore.dispatch(TrackingProtectionAction.TrackerBlockingChanged(isEnabled))
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return if (args.gravity == Gravity.BOTTOM) {
            object : BottomSheetDialog(requireContext(), this.theme) {
                override fun onBackPressed() {
                    this@TrackingProtectionPanelDialogFragment.onBackPressed()
                }
            }.apply {
                setOnShowListener {
                    val bottomSheet =
                        findViewById<View>(com.google.android.material.R.id.design_bottom_sheet) as FrameLayout
                    val behavior = BottomSheetBehavior.from(bottomSheet)
                    behavior.state = BottomSheetBehavior.STATE_EXPANDED
                }
            }
        } else {
            object : Dialog(requireContext()) {
                override fun onBackPressed() {
                    this@TrackingProtectionPanelDialogFragment.onBackPressed()
                }
            }.applyCustomizationsForTopDialog(inflateRootView())
        }
    }

    private fun Dialog.applyCustomizationsForTopDialog(rootView: View): Dialog {
        addContentView(
            rootView,
            LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.MATCH_PARENT
            )
        )

        window?.apply {
            setGravity(args.gravity)
            setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
            // This must be called after addContentView, or it won't fully fill to the edge.
            setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
        }
        return this
    }

    override fun onBackPressed(): Boolean {
        if (!trackingProtectionView.onBackPressed()) {
            dismiss()
        }
        return true
    }
}
