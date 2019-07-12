/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Copyright (C) 2018 Netflix, Inc.
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
 *
 * Created by Juliano Moraes, Rohan Dhruva, Emmanuel Boudrant.
 */

package org.mozilla.fenix.mvi

import androidx.annotation.VisibleForTesting
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.OnLifecycleEvent
import com.uber.autodispose.AutoDispose.autoDisposable
import com.uber.autodispose.ObservableSubscribeProxy
import com.uber.autodispose.android.lifecycle.AndroidLifecycleScopeProvider
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.rxkotlin.merge
import io.reactivex.subjects.PublishSubject
import io.reactivex.subjects.Subject

/**
 * It implements a Factory pattern generating Rx Subjects based on Event Types.
 * It maintain a map of Rx Subjects, one per type per instance of ActionBusFactory.
 *
 * @param owner is a LifecycleOwner used to auto dispose based on destroy observable
 */
class ActionBusFactory private constructor(val owner: LifecycleOwner) {

    companion object {

        private val buses = mutableMapOf<LifecycleOwner, ActionBusFactory>()

        /**
         * Return the [ActionBusFactory] associated to the [LifecycleOwner]. It there is no bus it will create one.
         * If the [LifecycleOwner] used is a fragment it use [Fragment#getViewLifecycleOwner()]
         */
        @JvmStatic
        fun get(lifecycleOwner: LifecycleOwner): ActionBusFactory {
            return with(lifecycleOwner) {
                var bus = buses[lifecycleOwner]
                if (bus == null) {
                    bus = ActionBusFactory(lifecycleOwner)
                    buses[lifecycleOwner] = bus
                    // LifecycleOwner
                    lifecycleOwner.lifecycle.addObserver(bus.observer)
                }
                bus
            }
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    val map = HashMap<Class<*>, Subject<*>>()

    internal val observer = object : LifecycleObserver {

        @OnLifecycleEvent(Lifecycle.Event.ON_DESTROY)
        fun onDestroy() {
            map.forEach { entry -> entry.value.onComplete() }
            buses.remove(owner)
        }
    }

    private fun <T> create(clazz: Class<T>): Subject<T> {
        val subject = PublishSubject.create<T>().toSerialized()
        map[clazz] = subject
        return subject
    }

    /**
     * emit will create (if needed) or use the existing Rx Subject to send events.
     *
     * @param clazz is the Event Class
     * @param event is the instance of the Event to be sent
     */
    @Suppress("UNCHECKED_CAST")
    fun <T : Action> emit(clazz: Class<T>, event: T) {
        val subject = if (map[clazz] != null) map[clazz] else create(clazz)
        (subject as Subject<T>).onNext(event)
    }

    /**x
     * getSafeManagedObservable returns an Rx Observable which is
     *  *Safe* against reentrant events as it is serialized and
     *  *Managed* since it disposes itself based on the lifecycle
     *
     *  @param clazz is the class of the event type used by this observable
     */
    @Suppress("UNCHECKED_CAST")
    fun <T : Action> getSafeManagedObservable(clazz: Class<T>): Observable<T> {
        return if (map[clazz] != null) map[clazz] as Observable<T> else create(clazz)
    }

    fun <T : Action> getAutoDisposeObservable(clazz: Class<T>): ObservableSubscribeProxy<T> {
        return getSafeManagedObservable(clazz)
            .`as`(autoDisposable(AndroidLifecycleScopeProvider.from(owner)))
    }

    @Suppress("UNCHECKED_CAST")
    fun <T : Action> getManagedEmitter(clazz: Class<T>): Observer<T> {
        return if (map[clazz] != null) map[clazz] as Observer<T> else create(clazz)
    }

    fun logMergedObservables() {
        // TODO make this observe new items in the map and combine them
        map.values.merge().compose(logState()).subscribe()
    }

    /**
     * getDestroyObservable observes to Lifecycle owner and fires during ON_DESTROY
     */
    fun getDestroyObservable(): Observable<Unit> {
        return owner.createDestroyObservable()
    }
}

/**
 * Extension on [LifecycleOwner] used to emit an event.
 */
inline fun <reified T : Action> LifecycleOwner.emit(event: T) =
    with(ActionBusFactory.get(this)) {
        getSafeManagedObservable(T::class.java)
        emit(T::class.java, event)
    }

/**
 * Extension on [LifecycleOwner] used used to get the state observable.
 */
inline fun <reified T : Action> LifecycleOwner.getSafeManagedObservable(): Observable<T> =
    ActionBusFactory.get(this).getSafeManagedObservable(T::class.java)

inline fun <reified T : Action> LifecycleOwner.getAutoDisposeObservable(): ObservableSubscribeProxy<T> =
    ActionBusFactory.get(this).getAutoDisposeObservable(T::class.java)

inline fun <reified T : Action> LifecycleOwner.getManagedEmitter(): Observer<T> =
    ActionBusFactory.get(this).getManagedEmitter(T::class.java)

/**
 * This method returns a destroy observable that can be passed to [org.mozilla.fenix.mvi.UIView]s as needed.
 */
fun LifecycleOwner?.createDestroyObservable(): Observable<Unit> {
    return Observable.create { emitter ->
        if (this == null || this.lifecycle.currentState == Lifecycle.State.DESTROYED) {
            emitter.onNext(Unit)
            emitter.onComplete()
            return@create
        }
        this.lifecycle.addObserver(object : LifecycleObserver {
            @OnLifecycleEvent(Lifecycle.Event.ON_DESTROY)
            fun emitDestroy() {
                if (emitter.isDisposed) {
                    emitter.onNext(Unit)
                    emitter.onComplete()
                }
            }
        })
    }
}
