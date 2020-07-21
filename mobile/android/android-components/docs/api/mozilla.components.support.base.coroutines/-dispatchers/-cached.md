[android-components](../../index.md) / [mozilla.components.support.base.coroutines](../index.md) / [Dispatchers](index.md) / [Cached](./-cached.md)

# Cached

`val Cached: ExecutorCoroutineDispatcher` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/coroutines/Dispatchers.kt#L26)

[CoroutineDispatcher](#) for short-lived asynchronous tasks. This dispatcher is using a thread
pool that creates new threads as needed, but will reuse previously constructed threads when
they are available.

Threads that have not been used for sixty seconds are terminated and removed from the cache.

