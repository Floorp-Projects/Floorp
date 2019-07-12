/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import androidx.lifecycle.LifecycleOwner
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.reactivex.android.plugins.RxAndroidPlugins
import io.reactivex.plugins.RxJavaPlugins
import io.reactivex.schedulers.Schedulers
import org.mozilla.fenix.mvi.ActionBusFactory

object TestUtils {
    fun setRxSchedulers() {
        RxJavaPlugins.setIoSchedulerHandler { Schedulers.trampoline() }
        RxAndroidPlugins.setInitMainThreadSchedulerHandler { Schedulers.trampoline() }
    }

    val owner = mockk<LifecycleOwner> {
        every { lifecycle } returns mockk()
        every { lifecycle.addObserver(any()) } just Runs
    }
    val bus: ActionBusFactory = ActionBusFactory.get(owner)
}
