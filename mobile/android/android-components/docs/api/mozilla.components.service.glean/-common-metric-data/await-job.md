[android-components](../../index.md) / [mozilla.components.service.glean](../index.md) / [CommonMetricData](index.md) / [awaitJob](./await-job.md)

# awaitJob

`open fun awaitJob(job: Job, timeout: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = JOB_TIMEOUT_MS): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/CommonMetricData.kt#L100)

Helper function to help await Jobs returned from coroutine launch executions used by metrics
when recording data.  This is to help ensure that data is updated before test functions
check or access them.

### Parameters

`job` - Job that is to be awaited

`timeout` - Length of time in milliseconds that .join will be awaited. Defaults to
    [JOB_TIMEOUT_MS](#).

### Exceptions

`TimeoutCancellationException` - if the function times out waiting for the join()