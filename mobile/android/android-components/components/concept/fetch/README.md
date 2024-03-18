# [Android Components](../../../README.md) > Concept > Fetch

The `concept-fetch` component contains interfaces for defining an abstract HTTP client for fetching resources.

The primary use of this component is to hide the actual implementation of the HTTP client from components required to make HTTP requests. This allows apps to configure a single app-wide used client without the components enforcing a particular dependency.

The API and name of the component is inspired by the [Web Fetch API](https://developer.mozilla.org/en-US/docs/Web/API/Fetch_API).

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:concept-fetch:{latest-version}"
```

### Performing requests

#### Get a URL

```Kotlin
val request = Request(url)
val response = client.fetch(request)
val body = response.string()
```

A `Response` may hold references to other resources (e.g. streams). Therefore it's important to always close the `Response` object or its `Body`. This can be done by either consuming the content of the `Body` with one of the available methods or by using Kotlin's extension methods for using `Closeable` implementations (e.g. `use()`):

```Kotlin
client.fetch(Request(url)).use { response ->
    val body = response.body.string()
}
```

#### Post to a URL

```Kotlin
val request = Request(
   url = "...",
   method = Request.Method.POST,
   body = Request.Body.fromStream(stream))

client.fetch(request).use { response ->
   if (response.success) {
       // ...
   }
}
```

#### Github API example

```Kotlin
val request = Request(
   url = "https://api.github.com/repos/mozilla-mobile/android-components/issues",
   headers = MutableHeaders(
       "User-Agent" to "AwesomeBrowser/1.0",
       "Accept" to "application/json; q=0.5",
       "Accept" to "application/vnd.github.v3+json"))

client.fetch(request).use { response ->
    val server = response.headers.get('Server')
    val result = response.body.string()
}
```

#### Posting a file

```Kotlin
val file = File("README.md")

val request = Request(
   url = "https://api.github.com/markdown/raw",
   headers = MutableHeaders(
       "Content-Type", "text/x-markdown; charset=utf-8"
   ),
   body = Request.Body.fromFile(file))

client.fetch(request).use { response ->
   if (request.success) {
      // Upload was successful!
   }
}

```

#### Asynchronous requests

Client implementations are synchronous. For asynchronous requests it's recommended to wrap a client in a Coroutine with a scope the calling code is in control of:

```Kotlin
val deferredResponse = async { client.fetch(request) }
val body = deferredResponse.await().body.string()
```

### Interceptors

Interceptors are a powerful mechanism to monitor, modify, retry, redirect or record requests as well as responses going through a `Client`. Interceptors can be used with any `concept-fetch` implementation.

The `withInterceptors()` extension method can be used to create a wrapped `Client` that will use the provided interceptors for requests.

```kotlin
val response = HttpURLConnectionClient()
  .withInterceptors(LoggingInterceptor(), RetryInterceptor())
  .fetch(request)
```

The following example implements a simple `Interceptor` that logs requests and how long they took:

```kotlin
class LoggingInterceptor(
    private val logger: Logger = Logger("Client")
): Interceptor {
    override fun intercept(chain: Interceptor.Chain): Response {
        logger.info("Request to ${chain.request.url}")

        val startTime = System.currentTimeMillis()

        val response = chain.proceed(chain.request)

        val took = System.currentTimeMillis() - startTime
        logger.info("[${response.status}] took $took ms")

        return response
    }
}
```

And the following example is a naive implementation of an interceptor that retries requests:

```kotlin
class NaiveRetryInterceptor(
    private val maxRetries: Int = 3
) : Interceptor {
    override fun intercept(chain: Interceptor.Chain): Response {
        val response = chain.proceed(chain.request)
        if (response.isSuccess) {
            return response
        }

        return retry(chain) ?: response
    }

    fun retry(chain: Interceptor.Chain): Response? {
        var lastResponse: Response? = null
        var retries = 0

        while (retries < maxRetries) {
            lastResponse = chain.proceed(chain.request)
            if (lastResponse.isSuccess) {
                return lastResponse
            }
            retries++
        }

        return lastResponse
    }
}
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
