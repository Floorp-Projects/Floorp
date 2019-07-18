/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions

import android.text.SpannableString
import android.text.method.LinkMovementMethod
import android.text.style.ClickableSpan
import android.text.style.ForegroundColorSpan
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.recyclerview.widget.LinearLayoutManager
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import kotlinx.android.synthetic.main.component_exceptions.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.mvi.UIView

class ExceptionsUIView(
    container: ViewGroup,
    actionEmitter: Observer<ExceptionsAction>,
    changesObservable: Observable<ExceptionsChange>
) :
    UIView<ExceptionsState, ExceptionsAction, ExceptionsChange>(
        container,
        actionEmitter,
        changesObservable
    ) {

    override val view: FrameLayout = LayoutInflater.from(container.context)
        .inflate(R.layout.component_exceptions, container, true)
        .findViewById(R.id.exceptions_wrapper)

    init {
        view.exceptions_list.apply {
            adapter = ExceptionsAdapter(actionEmitter)
            layoutManager = LinearLayoutManager(container.context)
        }
        val descriptionText = String
            .format(
                view.exceptions_empty_view.text.toString(),
                System.getProperty("line.separator")
            )
        val linkStartIndex = descriptionText.indexOf("\n\n") + 2
        val linkAction = object : ClickableSpan() {
            override fun onClick(widget: View?) {
                actionEmitter.onNext(ExceptionsAction.LearnMore)
            }
        }
        val textWithLink = SpannableString(descriptionText).apply {
            setSpan(linkAction, linkStartIndex, descriptionText.length, 0)
            val colorSpan = ForegroundColorSpan(view.exceptions_empty_view.currentTextColor)
            setSpan(colorSpan, linkStartIndex, descriptionText.length, 0)
        }

        view.exceptions_empty_view.movementMethod = LinkMovementMethod.getInstance()
        view.exceptions_empty_view.text = textWithLink
    }

    override fun updateView() = Consumer<ExceptionsState> {
        view.exceptions_empty_view.visibility = if (it.items.isEmpty()) View.VISIBLE else View.GONE
        view.exceptions_list.visibility = if (it.items.isEmpty()) View.GONE else View.VISIBLE
        (view.exceptions_list.adapter as ExceptionsAdapter).updateData(it.items)
    }
}
