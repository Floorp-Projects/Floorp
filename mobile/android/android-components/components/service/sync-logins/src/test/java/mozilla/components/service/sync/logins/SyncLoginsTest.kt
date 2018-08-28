package mozilla.components.service.sync.logins

import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

import org.mozilla.sync15.logins.SyncResult

@RunWith(RobolectricTestRunner::class)
class SyncLoginsTest {

    @Test
    fun syncLogins() {
        SyncResult.fromValue(42)
    }
}
