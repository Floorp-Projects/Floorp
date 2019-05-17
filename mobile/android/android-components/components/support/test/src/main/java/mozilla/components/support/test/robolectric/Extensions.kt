package mozilla.components.support.test.robolectric

import android.content.Context
import androidx.test.core.app.ApplicationProvider

/**
 * Provides application context for test purposes
 */
inline val testContext: Context
    get() = ApplicationProvider.getApplicationContext()
