/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.content.Context
import android.view.ViewGroup
import androidx.core.net.toUri
import mozilla.components.feature.sitepermissions.SitePermissions
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.mvi.ViewState
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.UIComponentViewModelBase
import org.mozilla.fenix.mvi.UIComponentViewModelProvider
import org.mozilla.fenix.mvi.UIView
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.toggle
import org.mozilla.fenix.utils.Settings

class QuickSettingsComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    viewModelProvider: UIComponentViewModelProvider<QuickSettingsState, QuickSettingsChange>
) : UIComponent<QuickSettingsState, QuickSettingsAction, QuickSettingsChange>(
    bus.getManagedEmitter(QuickSettingsAction::class.java),
    bus.getSafeManagedObservable(QuickSettingsChange::class.java),
    viewModelProvider
) {
    override fun initView(): UIView<QuickSettingsState, QuickSettingsAction, QuickSettingsChange> {
        return QuickSettingsUIView(container, actionEmitter, changesObservable, container)
    }

    init {
        bind()
    }

    fun toggleSitePermission(
        context: Context,
        featurePhone: PhoneFeature,
        url: String,
        sitePermissions: SitePermissions?
    ): SitePermissions {

        return if (sitePermissions == null) {
            val settings = Settings.getInstance(context)
            val origin = requireNotNull(url.toUri().host)
            var location = settings.getSitePermissionsPhoneFeatureLocation().toStatus()
            var camera = settings.getSitePermissionsPhoneFeatureCameraAction().toStatus()
            var microphone = settings.getSitePermissionsPhoneFeatureMicrophoneAction().toStatus()
            var notification = settings.getSitePermissionsPhoneFeatureNotificationAction().toStatus()

            when (featurePhone) {
                PhoneFeature.CAMERA -> camera = camera.toggle()
                PhoneFeature.LOCATION -> location = location.toggle()
                PhoneFeature.MICROPHONE -> microphone = microphone.toggle()
                PhoneFeature.NOTIFICATION -> notification = notification.toggle()
            }
            context.components.core.permissionStorage
                .addSitePermissionException(origin, location, notification, microphone, camera)
        } else {
            val updatedSitePermissions = sitePermissions.toggle(featurePhone)
            context.components.core.permissionStorage.updateSitePermissions(updatedSitePermissions)
            updatedSitePermissions
        }
    }
}

data class QuickSettingsState(val mode: Mode) : ViewState {
    sealed class Mode {
        data class Normal(
            val url: String,
            val isSecured: Boolean,
            val isTrackingProtectionOn: Boolean,
            val sitePermissions: SitePermissions?
        ) : Mode()

        data class ActionLabelUpdated(
            val phoneFeature: PhoneFeature,
            val sitePermissions: SitePermissions?
        ) :
            Mode()

        data class CheckPendingFeatureBlockedByAndroid(val sitePermissions: SitePermissions?) :
            Mode()
    }
}

sealed class QuickSettingsAction : Action {
    data class SelectReportProblem(val url: String) : QuickSettingsAction()
    object SelectTrackingProtectionSettings : QuickSettingsAction()
    data class ToggleTrackingProtection(val trackingProtection: Boolean) : QuickSettingsAction()
    data class SelectBlockedByAndroid(val permissions: Array<String>) : QuickSettingsAction()
    data class TogglePermission(val featurePhone: PhoneFeature) : QuickSettingsAction()
}

sealed class QuickSettingsChange : Change {
    data class Change(
        val url: String,
        val isSecured: Boolean,
        val isTrackingProtectionOn: Boolean,
        val sitePermissions: SitePermissions?
    ) : QuickSettingsChange()

    data class PermissionGranted(val phoneFeature: PhoneFeature, val sitePermissions: SitePermissions?) :
        QuickSettingsChange()

    data class PromptRestarted(val sitePermissions: SitePermissions?) : QuickSettingsChange()
    data class Stored(val phoneFeature: PhoneFeature, val sitePermissions: SitePermissions?) : QuickSettingsChange()
}

class QuickSettingsViewModel(
    initialState: QuickSettingsState
) : UIComponentViewModelBase<QuickSettingsState, QuickSettingsChange>(initialState, reducer) {
    companion object {
        val reducer: (QuickSettingsState, QuickSettingsChange) -> QuickSettingsState = { state, change ->
            when (change) {
                is QuickSettingsChange.Change -> {
                    state.copy(
                        mode = QuickSettingsState.Mode.Normal(
                            change.url,
                            change.isSecured,
                            change.isTrackingProtectionOn,
                            change.sitePermissions
                        )
                    )
                }
                is QuickSettingsChange.PermissionGranted -> {
                    state.copy(
                        mode = QuickSettingsState.Mode.ActionLabelUpdated(change.phoneFeature, change.sitePermissions)
                    )
                }
                is QuickSettingsChange.PromptRestarted -> {
                    state.copy(
                        mode = QuickSettingsState.Mode.CheckPendingFeatureBlockedByAndroid(change.sitePermissions)
                    )
                }
                is QuickSettingsChange.Stored -> {
                    state.copy(
                        mode = QuickSettingsState.Mode.ActionLabelUpdated(change.phoneFeature, change.sitePermissions)
                    )
                }
            }
        }
    }
}
