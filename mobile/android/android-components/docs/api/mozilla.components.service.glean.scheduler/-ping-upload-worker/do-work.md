[android-components](../../index.md) / [mozilla.components.service.glean.scheduler](../index.md) / [PingUploadWorker](index.md) / [doWork](./do-work.md)

# doWork

`fun doWork(): Result` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/scheduler/PingUploadWorker.kt#L86)

This method is called on a background thread - you are required to **synchronously** do your
work and return the [androidx.work.ListenableWorker.Result](#) from this method.  Once you
return from this method, the Worker is considered to have finished what its doing and will be
destroyed.

A Worker is given a maximum of ten minutes to finish its execution and return a
[androidx.work.ListenableWorker.Result](#).  After this time has expired, the Worker will
be signalled to stop.

**Return**
The [androidx.work.ListenableWorker.Result](#) of the computation

