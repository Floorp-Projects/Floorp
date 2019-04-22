package org.mozilla.fenix.collections

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.view.LayoutInflater
import android.view.ViewGroup
import org.mozilla.fenix.R
import io.reactivex.Observer
import io.reactivex.Observable
import io.reactivex.functions.Consumer
import org.mozilla.fenix.mvi.UIView

class CollectionCreationUIView(
    container: ViewGroup,
    actionEmitter: Observer<CollectionCreationAction>,
    changesObservable: Observable<CollectionCreationChange>
) : UIView<CollectionCreationState, CollectionCreationAction, CollectionCreationChange>(
    container,
    actionEmitter,
    changesObservable
) {
    override val view = LayoutInflater.from(container.context)
        .inflate(R.layout.component_collection_creation, container, true)

    override fun updateView() = Consumer<CollectionCreationState> {

    }
}