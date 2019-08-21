/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

/*
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import android.content.Context
import android.content.res.TypedArray
import android.os.Parcel
import android.os.Parcelable
import android.util.AttributeSet
import android.util.Log
import android.util.TypedValue
import android.view.KeyEvent
import android.view.View
import android.widget.SeekBar
import android.widget.SeekBar.OnSeekBarChangeListener
import android.widget.TextView
import androidx.preference.Preference
import androidx.preference.PreferenceViewHolder
import org.mozilla.fenix.R

import java.text.NumberFormat

/**
 * Preference based on android.preference.SeekBarPreference but uses support preference as a base.
 * It contains a title and a [SeekBar] and a SeekBar value [TextView] and an Example [TextView].
 * The actual preference layout is customizable by setting `android:layout` on the
 * preference widget layout or `seekBarPreferenceStyle` attribute.
 *
 * The [SeekBar] within the preference can be defined adjustable or not by setting `adjustable` attribute.
 * If adjustable, the preference will be responsive to DPAD left/right keys.
 * Otherwise, it skips those keys.
 *
 * The [SeekBar] value view can be shown or disabled by setting `showSeekBarValue`
 * attribute to true or false, respectively.
 *
 * Other [SeekBar] specific attributes (e.g. `title, summary, defaultValue, min, max`)
 * can be set directly on the preference widget layout.
 */
class TextPercentageSeekBarPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = R.attr.seekBarPreferenceStyle,
    defStyleRes: Int = 0
) : Preference(context, attrs, defStyleAttr, defStyleRes) {
    /* synthetic access */
    internal var mSeekBarValue: Int = 0
    /* synthetic access */
    internal var mMin: Int = 0
    private var mMax: Int = 0
    private var mSeekBarIncrement: Int = 0
    /* synthetic access */
    internal var mTrackingTouch: Boolean = false
    /* synthetic access */
    internal var mSeekBar: SeekBar? = null
    private var mSeekBarValueTextView: TextView? = null
    private var mExampleTextTextView: TextView? = null
    /**
     * Whether the SeekBar should respond to the left/right keys
     */
    /* synthetic access */
    var isAdjustable: Boolean = false
    /**
     * Whether to show the SeekBar value TextView next to the bar
     */
    private var mShowSeekBarValue: Boolean = false
    /**
     * Whether the SeekBarPreference should continuously save the Seekbar value while it is being dragged.
     */
    /* synthetic access */
    var updatesContinuously: Boolean = false
    /**
     * Listener reacting to the [SeekBar] changing value by the user
     */
    private val mSeekBarChangeListener = object : OnSeekBarChangeListener {
        override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
            if (fromUser && (updatesContinuously || !mTrackingTouch)) {
                syncValueInternal(seekBar)
            } else {
                // We always want to update the text while the seekbar is being dragged
                updateLabelValue(progress + mMin)
                updateExampleTextValue(progress + mMin)
            }
        }

        override fun onStartTrackingTouch(seekBar: SeekBar) {
            mTrackingTouch = true
        }

        override fun onStopTrackingTouch(seekBar: SeekBar) {
            mTrackingTouch = false
            if (seekBar.progress + mMin != mSeekBarValue) {
                syncValueInternal(seekBar)
            }
        }
    }

    /**
     * Listener reacting to the user pressing DPAD left/right keys if `adjustable` attribute is
     * set to true; it transfers the key presses to the [SeekBar]
     * to be handled accordingly.
     */
    private val mSeekBarKeyListener = View.OnKeyListener { _, keyCode, event ->
        return@OnKeyListener if (event.action != KeyEvent.ACTION_DOWN) {
            false
        } else if (!isAdjustable && (keyCode == KeyEvent.KEYCODE_DPAD_LEFT || keyCode == KeyEvent.KEYCODE_DPAD_RIGHT)) {
            // Right or left keys are pressed when in non-adjustable mode; Skip the keys.
            false
        } else if (keyCode == KeyEvent.KEYCODE_DPAD_CENTER || keyCode == KeyEvent.KEYCODE_ENTER) {
            // We don't want to propagate the click keys down to the SeekBar view since it will
            // create the ripple effect for the thumb.
            false
        } else if (mSeekBar == null) {
            Log.e(TAG, "SeekBar view is null and hence cannot be adjusted.")
            false
        } else {
            mSeekBar!!.onKeyDown(keyCode, event)
        }
    }

    /**
     * Gets the lower bound set on the [SeekBar].
     * @return The lower bound set
     * Sets the lower bound on the [SeekBar].
     * @param min The lower bound to set
     */
    var min: Int
        get() = mMin
        set(min) {
            var minimum = min
            if (minimum > mMax) {
                minimum = mMax
            }
            if (minimum != mMin) {
                mMin = minimum
                notifyChanged()
            }
        }

    /**
     * Returns the amount of increment change via each arrow key click. This value is derived from
     * user's specified increment value if it's not zero. Otherwise, the default value is picked
     * from the default mKeyProgressIncrement value in [android.widget.AbsSeekBar].
     * @return The amount of increment on the [SeekBar] performed after each user's arrow
     * key press
     */
    var seekBarIncrement: Int
        get() = mSeekBarIncrement
        /**
         * Sets the increment amount on the [SeekBar] for each arrow key press.
         * @param seekBarIncrement The amount to increment or decrement when the user presses an
         * arrow key.
         */
        set(seekBarIncrement) {
            if (seekBarIncrement != mSeekBarIncrement) {
                mSeekBarIncrement = Math.min(mMax - mMin, Math.abs(seekBarIncrement))
                notifyChanged()
            }
        }

    /**
     * Gets/Sets the upper bound set on the [SeekBar].
     */
    var max: Int
        get() = mMax
        set(max) {
            var maximum = max
            if (maximum < mMin) {
                maximum = mMin
            }
            if (maximum != mMax) {
                mMax = maximum
                notifyChanged()
            }
        }

    /**
     * Gets whether the current [SeekBar] value is displayed to the user.
     * @return Whether the current [SeekBar] value is displayed to the user
     * @see .setShowSeekBarValue
     */
    var showSeekBarValue: Boolean
        get() = mShowSeekBarValue
        /**
         * Sets whether the current [SeekBar] value is displayed to the user.
         * @param showSeekBarValue Whether the current [SeekBar] value is displayed to the user
         * @see .getShowSeekBarValue
         */
        set(showSeekBarValue) {
            mShowSeekBarValue = showSeekBarValue
            notifyChanged()
        }

    /**
     * Gets/Sets the current progress of the [SeekBar].
     */
    var value: Int
        get() = mSeekBarValue
        set(seekBarValue) = setValueInternal(seekBarValue, true)

    init {
        val a = context.obtainStyledAttributes(
            attrs, R.styleable.SeekBarPreference, defStyleAttr, defStyleRes
        )

        // The ordering of these two statements are important. If we want to set max first, we need
        // to perform the same steps by changing min/max to max/min as following:
        // mMax = a.getInt(...) and setMin(...).
        mMin = a.getInt(R.styleable.SeekBarPreference_min, 0)
        max = a.getInt(R.styleable.SeekBarPreference_android_max, SEEK_BAR_MAX)
        seekBarIncrement = a.getInt(R.styleable.SeekBarPreference_seekBarIncrement, 0)
        isAdjustable = a.getBoolean(R.styleable.SeekBarPreference_adjustable, true)
        mShowSeekBarValue = a.getBoolean(R.styleable.SeekBarPreference_showSeekBarValue, false)
        updatesContinuously = a.getBoolean(
            R.styleable.SeekBarPreference_updatesContinuously,
            false
        )
        a.recycle()
    }

    override fun onBindViewHolder(view: PreferenceViewHolder) {
        super.onBindViewHolder(view)
        view.itemView.setOnKeyListener(mSeekBarKeyListener)
        mSeekBar = view.findViewById(R.id.seekbar) as SeekBar
        mExampleTextTextView = view.findViewById(R.id.sampleText) as TextView
        mSeekBarValueTextView = view.findViewById(R.id.seekbar_value) as TextView
        if (mShowSeekBarValue) {
            mSeekBarValueTextView?.visibility = View.VISIBLE
        } else {
            mSeekBarValueTextView?.visibility = View.GONE
            mSeekBarValueTextView = null
        }

        if (mSeekBar == null) {
            Log.e(TAG, "SeekBar view is null in onBindViewHolder.")
            return
        }
        mSeekBar?.setOnSeekBarChangeListener(mSeekBarChangeListener)
        mSeekBar?.max = mMax - mMin
        // If the increment is not zero, use that. Otherwise, use the default mKeyProgressIncrement
        // in AbsSeekBar when it's zero. This default increment value is set by AbsSeekBar
        // after calling setMax. That's why it's important to call setKeyProgressIncrement after
        // calling setMax() since setMax() can change the increment value.
        if (mSeekBarIncrement != 0) {
            mSeekBar?.keyProgressIncrement = mSeekBarIncrement
        } else {
            mSeekBarIncrement = mSeekBar!!.keyProgressIncrement
        }

        mSeekBar?.progress = mSeekBarValue - mMin
        updateExampleTextValue(mSeekBarValue)
        updateLabelValue(mSeekBarValue)
        mSeekBar?.isEnabled = isEnabled
    }

    override fun onSetInitialValue(initialValue: Any?) {
        var defaultValue = initialValue
        if (defaultValue == null) {
            defaultValue = 0
        }
        value = getPersistedInt((defaultValue as Int?)!!)
    }

    override fun onGetDefaultValue(a: TypedArray?, index: Int): Any {
        return a!!.getInt(index, 0)
    }

    private fun setValueInternal(value: Int, notifyChanged: Boolean) {
        var seekBarValue = value
        if (seekBarValue < mMin) {
            seekBarValue = mMin
        }
        if (seekBarValue > mMax) {
            seekBarValue = mMax
        }

        if (seekBarValue != mSeekBarValue) {
            mSeekBarValue = seekBarValue
            updateLabelValue(mSeekBarValue)
            updateExampleTextValue(mSeekBarValue)
            persistInt(seekBarValue)
            if (notifyChanged) {
                notifyChanged()
            }
        }
    }

    /**
     * Persist the [SeekBar]'s SeekBar value if callChangeListener returns true, otherwise
     * set the [SeekBar]'s value to the stored value.
     */
    /* synthetic access */
    internal fun syncValueInternal(seekBar: SeekBar) {
        val seekBarValue = mMin + seekBar.progress
        if (seekBarValue != mSeekBarValue) {
            if (callChangeListener(seekBarValue)) {
                setValueInternal(seekBarValue, false)
            } else {
                seekBar.progress = mSeekBarValue - mMin
                updateLabelValue(mSeekBarValue)
                updateExampleTextValue(mSeekBarValue)
            }
        }
    }

    /**
     * Attempts to update the TextView label that displays the current value.
     *
     * @param labelValue the value to display next to the [SeekBar]
     */
    /* synthetic access */
    internal fun updateLabelValue(labelValue: Int) {
        var value = labelValue
        if (mSeekBarValueTextView != null) {
            value = value * STEP_SIZE + MIN_VALUE
            val decimalValue = (value / DECIMAL_CONVERSION).toDouble()
            val percentage = NumberFormat.getPercentInstance().format(decimalValue)
            mSeekBarValueTextView?.text = percentage
        }
    }

    /**
     * Attempts to update the example TextView text with text scale size.
     *
     * @param textValue the value of text size
     */
    internal fun updateExampleTextValue(textValue: Int) {
        var value = textValue
        if (mExampleTextTextView != null) {
            value = value * STEP_SIZE + MIN_VALUE
            val decimal = value / DECIMAL_CONVERSION
            val textSize = TEXT_SIZE * decimal
            mExampleTextTextView?.setTextSize(TypedValue.COMPLEX_UNIT_SP, textSize)
        }
    }

    override fun onSaveInstanceState(): Parcelable {
        val superState = super.onSaveInstanceState()
        if (isPersistent) {
            // No need to save instance state since it's persistent
            return superState
        }

        // Save the instance state
        val myState = SavedState(superState)
        myState.mSeekBarValue = mSeekBarValue
        myState.mMin = mMin
        myState.mMax = mMax
        return myState
    }

    override fun onRestoreInstanceState(state: Parcelable?) {
        if (state?.javaClass != SavedState::class.java) {
            // Didn't save state for us in onSaveInstanceState
            super.onRestoreInstanceState(state)
            return
        }

        // Restore the instance state
        val myState = state as SavedState?
        super.onRestoreInstanceState(myState!!.superState)
        mSeekBarValue = myState.mSeekBarValue
        mMin = myState.mMin
        mMax = myState.mMax
        notifyChanged()
    }

    /**
     * SavedState, a subclass of [BaseSavedState], will store the state of this preference.
     *
     *
     * It is important to always call through to super methods.
     */
    private class SavedState : BaseSavedState {

        internal var mSeekBarValue: Int = 0
        internal var mMin: Int = 0
        internal var mMax: Int = 0

        internal constructor(source: Parcel) : super(source) {

            // Restore the click counter
            mSeekBarValue = source.readInt()
            mMin = source.readInt()
            mMax = source.readInt()
        }

        internal constructor(superState: Parcelable) : super(superState)

        override fun writeToParcel(dest: Parcel, flags: Int) {
            super.writeToParcel(dest, flags)

            // Save the click counter
            dest.writeInt(mSeekBarValue)
            dest.writeInt(mMin)
            dest.writeInt(mMax)
        }

        companion object {
            @JvmField
            val CREATOR: Parcelable.Creator<SavedState> = object : Parcelable.Creator<SavedState> {
                override fun createFromParcel(`in`: Parcel): SavedState {
                    return SavedState(`in`)
                }

                override fun newArray(size: Int): Array<SavedState?> {
                    return arrayOfNulls(size)
                }
            }
        }
    }

    companion object {
        private const val TAG = "SeekBarPreference"
        private const val STEP_SIZE = 5
        private const val MIN_VALUE = 50
        private const val DECIMAL_CONVERSION = 100f
        private const val TEXT_SIZE = 16f
        private const val SEEK_BAR_MAX = 100
    }
}
