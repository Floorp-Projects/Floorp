/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.app.Dialog
import android.content.Context
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.DialogFragment
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.FragmentOnboardingHomeDialogBinding
import org.mozilla.fenix.databinding.OnboardingJumpBackInCfrBinding
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.recenttabs.view.RecentTabsHeaderViewHolder

/**
 * Dialog displayed once when one or multiples of these sections are shown in the home screen
 * recentTabs,recentBookmarks,historyMetadata or pocketArticles.
 */
class HomeOnboardingDialogFragment : DialogFragment() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, R.style.HomeOnboardingDialogStyle)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? = inflater.inflate(R.layout.fragment_onboarding_home_dialog, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val binding = FragmentOnboardingHomeDialogBinding.bind(view)

        binding.finishButton.setOnClickListener {
            context?.settings()?.let { settings ->
                settings.hasShownHomeOnboardingDialog = true
            }
            showJumpCFR()
            dismiss()
        }
    }

    private fun showJumpCFR() {
        val jumpBackInView = findJumpBackInView()
        jumpBackInView?.let {
            val crfDialog = createJumpCRF(anchor = jumpBackInView)
            crfDialog?.show()
        }
    }

    private fun findJumpBackInView(): View? {
        val list = activity?.findViewById<RecyclerView>(R.id.sessionControlRecyclerView)
        val count = list?.adapter?.itemCount ?: return null

        for (index in 0..count) {
            val viewHolder = list.findViewHolderForAdapterPosition(index)
            if (viewHolder is RecentTabsHeaderViewHolder) {
                return viewHolder.containerView
            }
        }
        return null
    }

    private fun createJumpCRF(anchor: View): Dialog? {
        val context: Context = requireContext()
        val anchorPosition = IntArray(2)
        val popupBinding = OnboardingJumpBackInCfrBinding.inflate(LayoutInflater.from(context))
        val popup = Dialog(context)

        popup.apply {
            setContentView(popupBinding.root)
            setCancelable(false)
            // removing title or setting it as an empty string does not prevent a11y services from assigning one
            setTitle(" ")
        }
        popupBinding.closeInfoBanner.setOnClickListener {
            popup.dismiss()
        }

        anchor.getLocationOnScreen(anchorPosition)
        val (x, y) = anchorPosition

        if (x == 0 && y == 0) {
            return null
        }

        popupBinding.root.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED)

        popup.window?.apply {
            val attr = attributes
            setGravity(Gravity.START or Gravity.TOP)
            attr.x = x
            attr.y = y - popupBinding.root.measuredHeight
            attributes = attr
            setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
        }
        return popup
    }
}
