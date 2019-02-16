[android-components](../../index.md) / [mozilla.components.tooling.fetch.tests](../index.md) / [FetchTestCases](./index.md)

# FetchTestCases

`abstract class FetchTestCases` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/tooling/fetch-tests/src/main/java/mozilla/components/tooling/fetch/tests/FetchTestCases.kt#L38)

Generic test cases for concept-fetch implementations.

We expect any implementation of concept-fetch to pass all test cases here.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FetchTestCases()`<br>Generic test cases for concept-fetch implementations. |

### Functions

| Name | Summary |
|---|---|
| [createNewClient](create-new-client.md) | `abstract fun createNewClient(): `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)<br>Creates a new [Client](../../mozilla.components.concept.fetch/-client/index.md) for running a specific test case with it. |
| [createWebServer](create-web-server.md) | `open fun createWebServer(): MockWebServer`<br>Creates a new [MockWebServer](#) to accept test requests. |
| [get200OverridingDefaultHeaders](get200-overriding-default-headers.md) | `open fun get200OverridingDefaultHeaders(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [get200WithCacheControl](get200-with-cache-control.md) | `open fun get200WithCacheControl(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [get200WithContentTypeCharset](get200-with-content-type-charset.md) | `open fun get200WithContentTypeCharset(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [get200WithCookiePolicy](get200-with-cookie-policy.md) | `open fun get200WithCookiePolicy(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [get200WithDefaultHeaders](get200-with-default-headers.md) | `open fun get200WithDefaultHeaders(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [get200WithDuplicatedCacheControlRequestHeaders](get200-with-duplicated-cache-control-request-headers.md) | `open fun get200WithDuplicatedCacheControlRequestHeaders(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [get200WithDuplicatedCacheControlResponseHeaders](get200-with-duplicated-cache-control-response-headers.md) | `open fun get200WithDuplicatedCacheControlResponseHeaders(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [get200WithGzippedBody](get200-with-gzipped-body.md) | `open fun get200WithGzippedBody(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [get200WithHeaders](get200-with-headers.md) | `open fun get200WithHeaders(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [get200WithReadTimeout](get200-with-read-timeout.md) | `open fun get200WithReadTimeout(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [get200WithStringBody](get200-with-string-body.md) | `open fun get200WithStringBody(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [get200WithUserAgent](get200-with-user-agent.md) | `open fun get200WithUserAgent(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [get302FollowRedirects](get302-follow-redirects.md) | `open fun get302FollowRedirects(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [get302FollowRedirectsDisabled](get302-follow-redirects-disabled.md) | `open fun get302FollowRedirectsDisabled(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [get404WithBody](get404-with-body.md) | `open fun get404WithBody(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [getThrowsIOExceptionWhenHostNotReachable](get-throws-i-o-exception-when-host-not-reachable.md) | `open fun getThrowsIOExceptionWhenHostNotReachable(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [post200WithBody](post200-with-body.md) | `open fun post200WithBody(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [put201FileUpload](put201-file-upload.md) | `open fun put201FileUpload(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
