/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.os.Bundle
import android.os.Environment
import android.view.View
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.findNavController
import com.google.android.material.snackbar.Snackbar
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.window.WindowRequest
import mozilla.components.concept.fetch.Response
import mozilla.components.feature.downloads.DownloadsFeature
import mozilla.components.feature.prompts.PromptFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.browser.DownloadDialogUtils
import org.mozilla.fenix.components.DownloadStyling
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.databinding.DownloadDialogLayoutBinding
import org.mozilla.fenix.downloads.DynamicDownloadDialog
import org.mozilla.fenix.ext.requireComponents

/**
 * Provides shared functionality to our fragments for add-on settings and
 * browser/page action popups.
 */
abstract class AddonPopupBaseFragment : Fragment(), EngineSession.Observer, UserInteractionHandler {
    private val promptsFeature = ViewBoundFeatureWrapper<PromptFeature>()
    private val downloadsFeature = ViewBoundFeatureWrapper<DownloadsFeature>()

    internal var session: SessionState? = null
    protected var engineSession: EngineSession? = null
    private var canGoBack: Boolean = false

    @Suppress("DEPRECATION")
    // https://github.com/mozilla-mobile/fenix/issues/19920
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        val store = requireComponents.core.store
        val safeContext = requireContext()
        session?.let {
            promptsFeature.set(
                feature = PromptFeature(
                    fragment = this,
                    store = store,
                    customTabId = it.id,
                    fragmentManager = parentFragmentManager,
                    fileUploadsDirCleaner = requireComponents.core.fileUploadsDirCleaner,
                    onNeedToRequestPermissions = { permissions ->
                        requestPermissions(permissions, REQUEST_CODE_PROMPT_PERMISSIONS)
                    },
                    tabsUseCases = requireComponents.useCases.tabsUseCases,
                ),
                owner = this,
                view = view,
            )

            downloadsFeature.set(
                DownloadsFeature(
                    safeContext,
                    store = store,
                    useCases = requireComponents.useCases.downloadUseCases,
                    fragmentManager = parentFragmentManager,
                    tabId = it.id,
                    downloadManager = requireComponents.downloadManager,
                    promptsStyling = DownloadStyling.createPrompt(safeContext),
                    onNeedToRequestPermissions = { permissions ->
                        requestPermissions(permissions, REQUEST_CODE_DOWNLOAD_PERMISSIONS)
                    },
                ),
                owner = this,
                view = view,
            )

            downloadsFeature.get()?.onDownloadStopped = { downloadState, _, downloadJobStatus ->
                val onCannotOpenFile: (DownloadState) -> Unit = {
                    FenixSnackbar.make(
                        view = getSnackBarContainer(),
                        duration = Snackbar.LENGTH_SHORT,
                        isDisplayedWithBrowserToolbar = true,
                    ).setText(DynamicDownloadDialog.getCannotOpenFileErrorMessage(requireContext(), it))
                        .show()
                }
                DownloadDialogUtils.handleOnDownloadFinished(
                    context = requireContext(),
                    downloadState = downloadState,
                    downloadJobStatus = downloadJobStatus,
                    currentTab = it,
                    onCannotOpenFile = onCannotOpenFile,
                    onFinishedDialogShown = {
                        DynamicDownloadDialog(
                            context = requireContext(),
                            downloadState = downloadState,
                            didFail = downloadJobStatus == DownloadState.Status.FAILED,
                            tryAgain = downloadsFeature.get()!!::tryAgain,
                            onCannotOpenFile = onCannotOpenFile,
                            binding = getDownloadDialogLayoutBinding(),
                            toolbarHeight = 0,
                        ) {}.show()
                    },
                )
            }
        }
    }

    override fun onDestroyView() {
        engineSession?.close()
        session?.let {
            requireComponents.core.store.dispatch(CustomTabListAction.RemoveCustomTabAction(it.id))
        }
        super.onDestroyView()
    }

    override fun onStart() {
        super.onStart()
        engineSession?.register(this)
    }

    override fun onStop() {
        super.onStop()
        engineSession?.unregister(this)
    }

    override fun onExternalResource(
        url: String,
        fileName: String?,
        contentLength: Long?,
        contentType: String?,
        cookie: String?,
        userAgent: String?,
        isPrivate: Boolean,
        skipConfirmation: Boolean,
        openInApp: Boolean,
        response: Response?,
    ) {
        session?.let { session ->
            val fileSize = if (contentLength != null && contentLength < 0) null else contentLength
            val download = DownloadState(
                url,
                fileName,
                contentType,
                fileSize,
                0,
                DownloadState.Status.INITIATED,
                userAgent,
                Environment.DIRECTORY_DOWNLOADS,
                private = isPrivate,
                skipConfirmation = skipConfirmation,
                openInApp = openInApp,
                response = response,
            )

            provideBrowserStore().dispatch(
                ContentAction.UpdateDownloadAction(
                    session.id,
                    download,
                ),
            )
        }
    }

    override fun onPromptRequest(promptRequest: PromptRequest) {
        session?.let { session ->
            requireComponents.core.store.dispatch(
                ContentAction.UpdatePromptRequestAction(
                    session.id,
                    promptRequest,
                ),
            )
        }
    }

    override fun onWindowRequest(windowRequest: WindowRequest) {
        if (windowRequest.type == WindowRequest.Type.CLOSE) {
            findNavController().popBackStack()
        } else {
            engineSession?.loadUrl(windowRequest.url)
        }
    }

    override fun onNavigationStateChange(canGoBack: Boolean?, canGoForward: Boolean?) {
        canGoBack?.let { this.canGoBack = canGoBack }
    }

    override fun onBackPressed(): Boolean {
        return if (this.canGoBack) {
            engineSession?.goBack()
            true
        } else {
            false
        }
    }

    protected fun initializeSession(fromEngineSession: EngineSession? = null) {
        engineSession = fromEngineSession ?: requireComponents.core.engine.createSession()
        session = createCustomTab(
            url = "",
            source = SessionState.Source.Internal.CustomTab,
        ).copy(engineState = EngineState(engineSession))
        requireComponents.core.store.dispatch(CustomTabListAction.AddCustomTabAction(session as CustomTabSessionState))
    }

    @VisibleForTesting
    internal fun provideBrowserStore() = requireComponents.core.store

    final override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray,
    ) {
        val feature = when (requestCode) {
            REQUEST_CODE_PROMPT_PERMISSIONS -> promptsFeature.get()
            REQUEST_CODE_DOWNLOAD_PERMISSIONS -> downloadsFeature.get()
            else -> null
        }
        feature?.onPermissionsResult(permissions, grantResults)
    }

    /**
     * Returns a [ViewGroup] where a SnackBar message should be anchored.
     */
    abstract fun getSnackBarContainer(): ViewGroup

    /**
     * Returns a [DownloadDialogLayoutBinding] to access the download dialog items.
     */
    abstract fun getDownloadDialogLayoutBinding(): DownloadDialogLayoutBinding

    companion object {
        private const val REQUEST_CODE_PROMPT_PERMISSIONS = 1
        private const val REQUEST_CODE_DOWNLOAD_PERMISSIONS = 2
    }
}
