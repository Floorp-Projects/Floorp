/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions

import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.core.view.isVisible
import androidx.recyclerview.widget.LinearLayoutManager
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.component_exceptions.*
import org.mozilla.fenix.R

/**
 * View that contains and configures the Exceptions List
 */
abstract class ExceptionsView<T : Any>(
    container: ViewGroup,
    protected val interactor: ExceptionsInteractor<T>
) : LayoutContainer {

    override val containerView: FrameLayout = LayoutInflater.from(container.context)
        .inflate(R.layout.component_exceptions, container, true)
        .findViewById(R.id.exceptions_wrapper)

    protected abstract val exceptionsAdapter: ExceptionsAdapter<T>

    init {
        exceptions_list.apply {
            layoutManager = LinearLayoutManager(containerView.context)
        }
    }

    fun update(items: List<T>) {
        exceptions_empty_view.isVisible = items.isEmpty()
        exceptions_list.isVisible = items.isNotEmpty()
        exceptionsAdapter.updateData(items)
    }
}
