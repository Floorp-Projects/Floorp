[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [AutoPushFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`AutoPushFeature(context: <ERROR CLASS>, service: `[`PushService`](../../mozilla.components.concept.push/-push-service/index.md)`, config: `[`PushConfig`](../-push-config/index.md)`, coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Executors.newSingleThreadExecutor().asCoroutineDispatcher(), connection: `[`PushConnection`](../-push-connection/index.md)` = RustPushConnection(
        senderId = config.senderId,
        serverHost = config.serverHost,
        socketProtocol = config.protocol,
        serviceType = config.serviceType,
        databasePath = File(context.filesDir, DB_NAME).canonicalPath
    ), crashReporter: `[`CrashReporting`](../../mozilla.components.support.base.crash/-crash-reporting/index.md)`? = null)`

A implementation of a [PushProcessor](../../mozilla.components.concept.push/-push-processor/index.md) that should live as a singleton by being installed
in the Application's onCreate. It receives messages from a service and forwards them
to be decrypted and routed.

Listen for subscription information changes for each registered scope:

Listen also for push messages:

