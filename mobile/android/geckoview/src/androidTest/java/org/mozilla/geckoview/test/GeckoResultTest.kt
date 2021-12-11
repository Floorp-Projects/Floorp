/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
package org.mozilla.geckoview.test

import org.junit.Test
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.test.util.Environment

import org.hamcrest.Matchers.*
import org.junit.Assert.assertThat

val env = Environment()

fun <T> GeckoResult<T>.pollDefault(): T? =
        this.poll(env.defaultTimeoutMillis)

class GeckoResultTestKotlin {
    class MockException : RuntimeException()

    @Test fun pollIncompleteWithValue() {
        val result = GeckoResult<Int>()
        val thread = Thread { result.complete(42) }

        thread.start()
        assertThat("Value should match", result.pollDefault(), equalTo(42))
    }

    @Test(expected = MockException::class) fun pollIncompleteWithError() {
        val result = GeckoResult<Void>()

        val thread = Thread { result.completeExceptionally(MockException()) }
        thread.start()

        result.pollDefault()
    }
}