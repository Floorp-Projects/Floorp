/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.mvi

import io.reactivex.ObservableTransformer
import io.reactivex.subjects.Subject
import mozilla.components.support.base.log.logger.Logger

/**
 * An action is a command or intent the user performed
 */
interface Action

/**
 * A Change is a change to the view coming from the model
 * (Extending action so we can reuse the ActionBusFactory)
 */
interface Change : Action

/**
 * A ViewState is a model reflecting the current state of the view
 */
interface ViewState

/**
 * A Reducer applies changes to the ViewState
 */
typealias Reducer<S, C> = (S, C) -> S

/**
 * Simple logger for tracking ViewState changes
 */
fun <S> logState(): ObservableTransformer<S, S> = ObservableTransformer { observable ->
    observable.doOnNext {
        if (BuildConfig.DEBUG) Logger("State").debug(it.toString())
    }
}

/**
 * For capturing state to a Subject for testing
 */
fun <S> captureState(subject: Subject<S>):
        ObservableTransformer<S, S> = ObservableTransformer { observable ->
    observable.doOnNext {
        subject.onNext(it)
    }
}
