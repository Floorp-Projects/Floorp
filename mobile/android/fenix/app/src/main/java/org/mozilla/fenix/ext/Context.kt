package org.mozilla.fenix.ext

import android.content.Context
import org.mozilla.fenix.FenixApplication
import org.mozilla.fenix.components.Components

val Context.application: FenixApplication
    get() = applicationContext as FenixApplication

val Context.components: Components
    get() = application.components