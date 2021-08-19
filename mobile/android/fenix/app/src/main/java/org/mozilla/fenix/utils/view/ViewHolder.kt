/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils.view

import android.view.View
import androidx.recyclerview.widget.RecyclerView

/**
 * A base class for all recycler view holders supporting Android Extensions-style view access.
 * This allows views to be used without an `itemView.<id>` prefix, and additionally caches them.
 */
abstract class ViewHolder(
    val containerView: View
) : RecyclerView.ViewHolder(containerView)
