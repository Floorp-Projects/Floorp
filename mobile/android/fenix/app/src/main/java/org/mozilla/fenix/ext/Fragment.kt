package org.mozilla.fenix.ext

import androidx.fragment.app.Fragment
import org.mozilla.fenix.components.Components

val Fragment.requireComponents: Components
    get() = requireContext().components