/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.toolbar

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.graphics.Canvas
import android.graphics.drawable.ClipDrawable
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.core.content.res.ResourcesCompat
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

// Code needed for assembling the sample application - but not needed to actually explain the toolbar

enum class ToolbarConfiguration(val label: String) {
    DEFAULT("Default"),
    FOCUS_TABLET("Firefox Focus (Tablet)"),
    FOCUS_PHONE("Firefox Focus (Phone)"),
    CUSTOM_MENU("Custom Menu"),
    PRIVATE_MODE("Private Mode"),
    FENIX("Fenix"),
    FENIX_CUSTOMTAB("Fenix (Custom Tab)"),
}

class ConfigurationAdapter(
    private val configuration: ToolbarConfiguration,
) : RecyclerView.Adapter<ConfigurationViewHolder>() {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ConfigurationViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(R.layout.item_toolbar_configuration, parent, false)
        return ConfigurationViewHolder(view as TextView)
    }

    override fun getItemCount() = ToolbarConfiguration.values().size

    override fun onBindViewHolder(holder: ConfigurationViewHolder, position: Int) {
        val item = ToolbarConfiguration.values()[position]
        holder.labelView.text = item.label

        holder.labelView.setOnClickListener {
            (it.context as Activity).finish()

            val intent = Intent(it.context, ToolbarActivity::class.java)
            intent.putExtra(Extra.TOOLBAR_LABEL, item.label)
            it.context.startActivity(intent)
        }

        if (item == configuration) {
            holder.labelView.setBackgroundResource(R.color.selected_configuration)
        }
    }
}

class ConfigurationViewHolder(val labelView: TextView) : RecyclerView.ViewHolder(labelView)

fun getToolbarConfiguration(intent: Intent): ToolbarConfiguration {
    val label = intent.extras?.getString(Extra.TOOLBAR_LABEL) ?: ToolbarConfiguration.DEFAULT.label

    ToolbarConfiguration.values().forEach {
        if (label == it.label) {
            return it
        }
    }

    return ToolbarConfiguration.DEFAULT
}

object Extra {
    internal const val TOOLBAR_LABEL = "toolbar_label"
}

/**
 * A custom view to be drawn behind the URL and page actions. Acts as a custom progress view.
 */
class UrlBoxProgressView(
    context: Context,
) : View(context) {
    var progress: Int = 0
        set(value) {
            // We clip the background and progress drawable based on the new progress:
            //
            //                      progress
            //                         v
            //   +---------------------+-------------------+
            //   | background drawable | progress drawable |
            //   +---------------------+-------------------+
            //
            // The drawable is clipped completely and not visible when the level is 0 and fully
            // revealed when the level is 10,000.
            backgroundDrawable.level = LEVEL_STEP_SIZE * (MAX_PROGRESS - value)
            progressDrawable.level = MAX_LEVEL - backgroundDrawable.level
            field = value
            invalidate() // Force redraw

            // If the progress is 100% then we want to go back to 0 to hide the progress drawable
            // again. However we want to show the full progress bar briefly so we wait 250ms before
            // going back to 0.
            if (value == MAX_PROGRESS) {
                CoroutineScope(Dispatchers.Main).launch {
                    delay(PROGRESS_VISIBLE_DELAY_MS)
                    progress = 0
                }
            }
        }

    private var backgroundDrawable = ClipDrawable(
        ResourcesCompat.getDrawable(resources, R.drawable.sample_url_background, context.theme),
        Gravity.END,
        ClipDrawable.HORIZONTAL,
    ).apply {
        level = MAX_LEVEL
    }

    private var progressDrawable = ClipDrawable(
        ResourcesCompat.getDrawable(resources, R.drawable.sample_url_progress, context.theme),
        Gravity.START,
        ClipDrawable.HORIZONTAL,
    ).apply {
        level = 0
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        backgroundDrawable.setBounds(0, 0, w, h)
        progressDrawable.setBounds(0, 0, w, h)
    }

    override fun onDraw(canvas: Canvas) {
        backgroundDrawable.draw(canvas)
        progressDrawable.draw(canvas)
    }

    companion object {
        private const val MAX_PROGRESS = 100
        private const val PROGRESS_VISIBLE_DELAY_MS = 250L
        private const val LEVEL_STEP_SIZE = 100
        private const val MAX_LEVEL = 10000
    }
}
