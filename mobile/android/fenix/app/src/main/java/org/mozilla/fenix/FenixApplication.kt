package org.mozilla.fenix

import android.app.Application
import org.mozilla.fenix.components.Components

class FenixApplication : Application() {
    val components by lazy { Components(this) }
}
