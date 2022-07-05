/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment

import android.content.res.Resources
import android.view.animation.Animation
import android.view.animation.AnimationSet
import android.view.animation.AnimationUtils
import androidx.activity.result.contract.ActivityResultContracts
import androidx.fragment.app.Fragment
import org.mozilla.focus.ext.hideToolbar

abstract class BaseFragment : Fragment() {
    private var animationSet: AnimationSet? = null

    override fun onResume() {
        super.onResume()
        hideToolbar()
    }

    fun cancelAnimation() {
        if (animationSet != null) {
            animationSet!!.duration = 0
            animationSet!!.cancel()
        }
    }

    @Suppress("SwallowedException")
    override fun onCreateAnimation(transit: Int, enter: Boolean, nextAnim: Int): Animation? {
        var animation = super.onCreateAnimation(transit, enter, nextAnim)
        if (animation == null && nextAnim != 0) {
            animation = try {
                AnimationUtils.loadAnimation(activity, nextAnim)
            } catch (e: Resources.NotFoundException) {
                return null
            }
        }
        return if (animation != null) {
            val animSet = AnimationSet(true)
            animSet.addAnimation(animation)
            animationSet = animSet
            animSet
        } else {
            null
        }
    }
}

fun Fragment.requestInPlacePermissions(
    permissionsToRequest: Array<String>,
    onResult: (Map<String, Boolean>) -> Unit
) {

    requireActivity().activityResultRegistry.register(
        "permissionsRequest",
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        onResult(permissions)
    }.also {
        it.launch(permissionsToRequest)
    }
}
