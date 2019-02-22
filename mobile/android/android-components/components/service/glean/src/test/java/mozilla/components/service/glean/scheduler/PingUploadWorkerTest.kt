package mozilla.components.service.glean.scheduler

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.work.BackoffPolicy
import androidx.work.Constraints
import androidx.work.NetworkType
import androidx.work.OneTimeWorkRequestBuilder
import androidx.work.WorkerParameters
import mozilla.components.service.glean.config.Configuration
import mozilla.components.service.glean.resetGlean
import org.junit.Assert
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.MockitoAnnotations
import org.robolectric.RobolectricTestRunner
import java.util.concurrent.TimeUnit

@RunWith(RobolectricTestRunner::class)
class PingUploadWorkerTest {

    @Mock
    var workerParams: WorkerParameters? = null

    var context: Context = ApplicationProvider.getApplicationContext()
    private var pingUploadWorker: PingUploadWorker? = null

    @Before
    @Throws(Exception::class)
    fun setUp() {
        MockitoAnnotations.initMocks(this)

        resetGlean(context, config = Configuration().copy(
            logPings = true
        ))

        pingUploadWorker = PingUploadWorker(context, workerParams!!)
    }

    @Test
    fun testBaselinePingConfiguration() {
        // Set the constraints around which the worker can be run, in this case it
        // only requires that any network connection be available.
        val workConstraints = Constraints.Builder()
            .setRequiredNetworkType(NetworkType.CONNECTED)
            .build()
        val workRequest = OneTimeWorkRequestBuilder<PingUploadWorker>()
            .addTag(PingUploadWorker.PING_WORKER_TAG)
            .setConstraints(workConstraints)
            .setBackoffCriteria(
                BackoffPolicy.EXPONENTIAL,
                PingUploadWorker.DEFAULT_BACKOFF_FOR_UPLOAD,
                TimeUnit.SECONDS)
            .build()
        val workSpec = workRequest.workSpec

        // verify constraints
        Assert.assertEquals(NetworkType.CONNECTED, workSpec.constraints.requiredNetworkType)
        Assert.assertEquals(BackoffPolicy.EXPONENTIAL, workSpec.backoffPolicy)
        Assert.assertTrue(workRequest.tags.contains(PingUploadWorker.PING_WORKER_TAG))
    }

    @Test
    fun testDoWorkSuccess() {
        val result = pingUploadWorker!!.doWork()
        Assert.assertTrue(result.toString().contains("Success"))
    }
}
