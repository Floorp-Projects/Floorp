/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.content.Context
import org.mozilla.fenix.FenixApplication
import org.mozilla.fenix.components.Components

val Context.application: FenixApplication
    get() = applicationContext as FenixApplication

val Context.components: Components
    get() = application.components