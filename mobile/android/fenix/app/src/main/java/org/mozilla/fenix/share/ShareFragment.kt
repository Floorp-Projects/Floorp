/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.fragment.app.clearFragmentResult
import androidx.fragment.app.setFragmentResult
import androidx.fragment.app.viewModels
import androidx.lifecycle.ViewModelProvider.AndroidViewModelFactory
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.feature.accounts.push.SendTabUseCases
import mozilla.components.feature.share.RecentAppsStorage
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.databinding.FragmentShareBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

class ShareFragment : AppCompatDialogFragment() {

    private val args by navArgs<ShareFragmentArgs>()
    private val viewModel: ShareViewModel by viewModels {
        AndroidViewModelFactory(requireActivity().application)
    }
    private lateinit var shareInteractor: ShareInteractor
    private lateinit var shareCloseView: ShareCloseView
    private lateinit var shareToAccountDevicesView: ShareToAccountDevicesView
    private lateinit var shareToAppsView: ShareToAppsView

    override fun onAttach(context: Context) {
        super.onAttach(context)
        viewModel.loadDevicesAndApps(requireContext())
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        context?.components?.analytics?.crashReporter?.recordCrashBreadcrumb(
            Breadcrumb("ShareFragment onCreate"),
        )
        setStyle(STYLE_NO_TITLE, R.style.ShareDialogStyle)
    }

    override fun onPause() {
        super.onPause()
        context?.components?.analytics?.crashReporter?.recordCrashBreadcrumb(
            Breadcrumb("ShareFragment dismiss"),
        )
        consumePrompt { onDismiss() }
        dismiss()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        val binding = FragmentShareBinding.inflate(
            inflater,
            container,
            false,
        )
        val shareData = args.data.toList()

        val accountManager = requireComponents.backgroundServices.accountManager

        shareInteractor = ShareInteractor(
            DefaultShareController(
                context = requireContext(),
                shareSubject = args.shareSubject,
                shareData = shareData,
                snackbar = FenixSnackbar.make(
                    view = requireActivity().getRootView()!!,
                    isDisplayedWithBrowserToolbar = true,
                ),
                navController = findNavController(),
                sendTabUseCases = SendTabUseCases(accountManager),
                saveToPdfUseCase = requireComponents.useCases.sessionUseCases.saveToPdf,
                printUseCase = requireComponents.useCases.sessionUseCases.printContent,
                recentAppsStorage = RecentAppsStorage(requireContext()),
                viewLifecycleScope = viewLifecycleOwner.lifecycleScope,
            ) { result ->
                consumePrompt {
                    when (result) {
                        ShareController.Result.DISMISSED -> onDismiss()
                        ShareController.Result.SHARE_ERROR -> onFailure()
                        ShareController.Result.SUCCESS -> onSuccess()
                    }
                }
                super.dismiss()
            },
        )

        binding.shareWrapper.setOnClickListener { shareInteractor.onShareClosed() }
        shareToAccountDevicesView =
            ShareToAccountDevicesView(binding.devicesShareLayout, shareInteractor)

        if (args.showPage) {
            // Show the previous fragment underneath the share background scrim
            // by making it translucent.
            binding.closeSharingScrim.alpha = SHOW_PAGE_ALPHA
            binding.shareWrapper.setOnClickListener { shareInteractor.onShareClosed() }
        } else {
            // Otherwise, show a list of tabs to share.
            binding.closeSharingScrim.alpha = 1.0f
            shareCloseView = ShareCloseView(binding.closeSharingContent, shareInteractor)
            shareCloseView.setTabs(shareData)
        }
        shareToAppsView = ShareToAppsView(binding.appsShareLayout, shareInteractor)

        binding.savePdf.setContent {
            FirefoxTheme(theme = Theme.getTheme(allowPrivateTheme = false)) {
                SaveToPDFItem {
                    shareInteractor.onSaveToPDF(tabId = args.sessionId)
                }
            }
        }

        if (FeatureFlags.print) {
            binding.print.setContent {
                FirefoxTheme(theme = Theme.getTheme(allowPrivateTheme = false)) {
                    PrintItem {
                        shareInteractor.onPrint(tabId = args.sessionId)
                    }
                }
            }
        }
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        viewModel.devicesList.observe(viewLifecycleOwner) { devicesShareOptions ->
            shareToAccountDevicesView.setShareTargets(devicesShareOptions)
        }
        viewModel.appsList.observe(viewLifecycleOwner) { appsToShareTo ->
            shareToAppsView.setShareTargets(appsToShareTo)
        }
        viewModel.recentAppsList.observe(viewLifecycleOwner) { appsToShareTo ->
            shareToAppsView.setRecentShareTargets(appsToShareTo)
        }
    }

    override fun onDestroy() {
        context?.components?.analytics?.crashReporter?.recordCrashBreadcrumb(
            Breadcrumb("ShareFragment onDestroy"),
        )
        setFragmentResult(RESULT_KEY, Bundle())
        // Clear the stored result in case there is no listener with the same key set.
        clearFragmentResult(RESULT_KEY)

        super.onDestroy()
    }

    /**
     * If [ShareFragmentArgs.sessionId] is set and the session has a pending Web Share
     * prompt request, call [consume] then clean up the prompt.
     */
    private fun consumePrompt(
        consume: PromptRequest.Share.() -> Unit,
    ) {
        val browserStore = requireComponents.core.store
        args.sessionId
            ?.let { sessionId -> browserStore.state.findTabOrCustomTab(sessionId) }
            ?.let { tab ->
                val promptRequest = tab.content.promptRequests.lastOrNull { it is PromptRequest.Share }
                if (promptRequest is PromptRequest.Share) {
                    consume(promptRequest)
                    browserStore.dispatch(ContentAction.ConsumePromptRequestAction(tab.id, promptRequest))
                }
            }
    }

    companion object {
        const val SHOW_PAGE_ALPHA = 0.6f
        const val RESULT_KEY = "shareFragmentResultKey"
    }
}
