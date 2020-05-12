package org.mozilla.fenix.ext

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.extensions.LayoutContainer

/**
 * A base class for all recycler view holders supporting Android Extensions-style view access.
 * This allows views to be used without an `itemView.<id>` prefix, and additionally caches them.
 */
abstract class ViewHolder(
    override val containerView: View
) : RecyclerView.ViewHolder(containerView), LayoutContainer
