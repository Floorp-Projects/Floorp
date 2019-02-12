package org.mozilla.fenix.home

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@SuppressWarnings("MagicNumber")
class KeyTrigger(
    frame: Int,
    private val onPositiveCross: () -> Unit,
    private val onNegativeCross: () -> Unit
) {
    private val fireThreshhold = (frame + 0.5F) / 100.0F
    private var negativeReset = false
    private var positiveReset = false
    private var lastFirePosition = 0f
    private val triggerSlack = 0.1f

    fun conditionallyFire(progress: Float) {
        var offset: Float
        var lastOffset: Float

        if (negativeReset) {
            offset = progress - fireThreshhold
            lastOffset = lastFirePosition - fireThreshhold
            if (offset * lastOffset < 0.0f && offset < 0.0f) {
                onNegativeCross.invoke()
                negativeReset = false
            }
        } else if (Math.abs(progress - fireThreshhold) > triggerSlack) {
            negativeReset = true
        }

        if (positiveReset) {
            offset = progress - fireThreshhold
            lastOffset = lastFirePosition - fireThreshhold
            if (offset * lastOffset < 0.0f && offset > 0.0f) {
                onPositiveCross.invoke()
                positiveReset = false
            }
        } else if (Math.abs(progress - fireThreshhold) > triggerSlack) {
            positiveReset = true
        }

        lastFirePosition = progress
    }
}
