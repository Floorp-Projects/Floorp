package org.mozilla.samples.toolbar

import android.app.Activity
import android.content.Intent
import android.support.v7.widget.RecyclerView
import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.TextView

// Code needed for assembling the sample application - but not needed to actually explain the toolbar

enum class ToolbarConfiguration(val label: String) {
    DEFAULT("Default"),
    FOCUS_TABLET("Firefox Focus (Tablet)"),
    FOCUS_PHONE("Firefox Focus (Phone)")
}

class ConfigurationAdapter(val configuration: ToolbarConfiguration) : RecyclerView.Adapter<ConfigurationViewHolder>() {
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
            holder.labelView.setBackgroundColor(0xFF222222.toInt())
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