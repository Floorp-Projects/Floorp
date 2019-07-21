/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.GeckoSessionSettings
import org.mozilla.geckoview.StorageController

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.json.JSONObject
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
@MediumTest
class StorageControllerTest : BaseSessionTest() {

    @Test fun clearData() {
        sessionRule.session.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        sessionRule.session.evaluateJS("""
            localStorage.setItem('ctx', 'test');
            document.cookie = 'ctx=test';
        """)

        var localStorage = sessionRule.session.evaluateJS("""
            localStorage.getItem('ctx') || 'null'
        """) as String

        var cookie = sessionRule.session.evaluateJS("""
            document.cookie || 'null'
        """) as String

        assertThat("Local storage value should match",
                   localStorage,
                   equalTo("test"))
        assertThat("Cookie value should match",
                   cookie,
                   equalTo("ctx=test"))

        sessionRule.waitForResult(
            sessionRule.runtime.storageController.clearData(
                StorageController.ClearFlags.ALL))

        localStorage = sessionRule.session.evaluateJS("""
            localStorage.getItem('ctx') || 'null'
        """) as String

        cookie = sessionRule.session.evaluateJS("""
            document.cookie || 'null'
        """) as String

        assertThat("Local storage value should match",
                   localStorage,
                   equalTo("null"))
        assertThat("Cookie value should match",
                   cookie,
                   equalTo("null"))
    }

    @Test fun clearDataFlags() {
        sessionRule.session.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        sessionRule.session.evaluateJS("""
            localStorage.setItem('ctx', 'test');
            document.cookie = 'ctx=test';
        """)

        var localStorage = sessionRule.session.evaluateJS("""
            localStorage.getItem('ctx') || 'null'
        """) as String

        var cookie = sessionRule.session.evaluateJS("""
            document.cookie || 'null'
        """) as String

        assertThat("Local storage value should match",
                   localStorage,
                   equalTo("test"))
        assertThat("Cookie value should match",
                   cookie,
                   equalTo("ctx=test"))

        sessionRule.waitForResult(
            sessionRule.runtime.storageController.clearData(
                StorageController.ClearFlags.COOKIES))

        localStorage = sessionRule.session.evaluateJS("""
            localStorage.getItem('ctx') || 'null'
        """) as String

        cookie = sessionRule.session.evaluateJS("""
            document.cookie || 'null'
        """) as String

        assertThat("Local storage value should match",
                   localStorage,
                   equalTo("test"))
        assertThat("Cookie value should match",
                   cookie,
                   equalTo("null"))

        sessionRule.session.evaluateJS("""
            document.cookie = 'ctx=test';
        """)

        sessionRule.waitForResult(
            sessionRule.runtime.storageController.clearData(
                StorageController.ClearFlags.DOM_STORAGES))

        localStorage = sessionRule.session.evaluateJS("""
            localStorage.getItem('ctx') || 'null'
        """) as String

        cookie = sessionRule.session.evaluateJS("""
            document.cookie || 'null'
        """) as String

        assertThat("Local storage value should match",
                   localStorage,
                   equalTo("null"))
        assertThat("Cookie value should match",
                   cookie,
                   equalTo("ctx=test"))

        sessionRule.session.evaluateJS("""
            localStorage.setItem('ctx', 'test');
        """)

        sessionRule.waitForResult(
            sessionRule.runtime.storageController.clearData(
                StorageController.ClearFlags.SITE_DATA))

        localStorage = sessionRule.session.evaluateJS("""
            localStorage.getItem('ctx') || 'null'
        """) as String

        cookie = sessionRule.session.evaluateJS("""
            document.cookie || 'null'
        """) as String

        assertThat("Local storage value should match",
                   localStorage,
                   equalTo("null"))
        assertThat("Cookie value should match",
                   cookie,
                   equalTo("null"))
    }

    @Test fun clearDataFromHost() {
        sessionRule.session.loadUri("https://example.com")
        sessionRule.waitForPageStop()

        sessionRule.session.evaluateJS("""
            localStorage.setItem('ctx', 'test');
            document.cookie = 'ctx=test';
        """)

        var localStorage = sessionRule.session.evaluateJS("""
            localStorage.getItem('ctx') || 'null'
        """) as String

        var cookie = sessionRule.session.evaluateJS("""
            document.cookie || 'null'
        """) as String

        assertThat("Local storage value should match",
                   localStorage,
                   equalTo("test"))
        assertThat("Cookie value should match",
                   cookie,
                   equalTo("ctx=test"))

        sessionRule.waitForResult(
            sessionRule.runtime.storageController.clearDataFromHost(
                "test.com",
                StorageController.ClearFlags.ALL))

        localStorage = sessionRule.session.evaluateJS("""
            localStorage.getItem('ctx') || 'null'
        """) as String

        cookie = sessionRule.session.evaluateJS("""
            document.cookie || 'null'
        """) as String

        assertThat("Local storage value should match",
                   localStorage,
                   equalTo("test"))
        assertThat("Cookie value should match",
                   cookie,
                   equalTo("ctx=test"))

        sessionRule.waitForResult(
            sessionRule.runtime.storageController.clearDataFromHost(
                "example.com",
                StorageController.ClearFlags.ALL))

        localStorage = sessionRule.session.evaluateJS("""
            localStorage.getItem('ctx') || 'null'
        """) as String

        cookie = sessionRule.session.evaluateJS("""
            document.cookie || 'null'
        """) as String

        assertThat("Local storage value should match",
                   localStorage,
                   equalTo("null"))
        assertThat("Cookie value should match",
                   cookie,
                   equalTo("null"))
    }

    @Test fun sessionContext() {
        val session1 = sessionRule.createOpenSession(
                GeckoSessionSettings.Builder(mainSession.settings)
                .contextId("1")
                .build())
        session1.loadUri("https://example.com")
        session1.waitForPageStop()

        session1.evaluateJS("""
            localStorage.setItem('ctx', '1');
        """)

        var localStorage = session1.evaluateJS("""
            localStorage.getItem('ctx') || 'null'
        """) as String

        assertThat("Local storage value should match",
                   localStorage,
                   equalTo("1"))

        session1.reload()
        session1.waitForPageStop()

        localStorage = session1.evaluateJS("""
            localStorage.getItem('ctx') || 'null'
        """) as String

        assertThat("Local storage value should match",
                   localStorage,
                   equalTo("1"))

        val session2 = sessionRule.createOpenSession(
                GeckoSessionSettings.Builder(mainSession.settings)
                .contextId("2")
                .build())

        session2.loadUri("https://example.com")
        session2.waitForPageStop()

        localStorage = session2.evaluateJS("""
            localStorage.getItem('ctx') || 'null'
        """) as String

        assertThat("Local storage value should match",
                   localStorage,
                   equalTo("null"))

        session2.evaluateJS("""
            localStorage.setItem('ctx', '2');
        """)

        localStorage = session2.evaluateJS("""
            localStorage.getItem('ctx') || 'null'
        """) as String

        assertThat("Local storage value should match",
                   localStorage,
                   equalTo("2"))

        session1.loadUri("https://example.com")
        session1.waitForPageStop()

        localStorage = session1.evaluateJS("""
            localStorage.getItem('ctx') || 'null'
        """) as String

        assertThat("Local storage value should match",
                   localStorage,
                   equalTo("1"))

        val session3 = sessionRule.createOpenSession(
                GeckoSessionSettings.Builder(mainSession.settings)
                .contextId("2")
                .build())

        session3.loadUri("https://example.com")
        session3.waitForPageStop()

        localStorage = session3.evaluateJS("""
            localStorage.getItem('ctx') || 'null'
        """) as String

        assertThat("Local storage value should match",
                   localStorage,
                   equalTo("2"))
    }

    @Test fun clearDataForSessionContext() {
        val session1 = sessionRule.createOpenSession(
                GeckoSessionSettings.Builder(mainSession.settings)
                .contextId("1")
                .build())
        session1.loadUri("https://example.com")
        session1.waitForPageStop()

        session1.evaluateJS("""
            localStorage.setItem('ctx', '1');
        """)

        var localStorage = session1.evaluateJS("""
            localStorage.getItem('ctx') || 'null'
        """) as String

        assertThat("Local storage value should match",
                   localStorage,
                   equalTo("1"))

        session1.close()

        val session2 = sessionRule.createOpenSession(
                GeckoSessionSettings.Builder(mainSession.settings)
                .contextId("2")
                .build())

        session2.loadUri("https://example.com")
        session2.waitForPageStop()

        session2.evaluateJS("""
            localStorage.setItem('ctx', '2');
        """)

        localStorage = session2.evaluateJS("""
            localStorage.getItem('ctx') || 'null'
        """) as String

        assertThat("Local storage value should match",
                   localStorage,
                   equalTo("2"))

        session2.close()

        sessionRule.runtime.storageController.clearDataForSessionContext("1")

        val session3 = sessionRule.createOpenSession(
                GeckoSessionSettings.Builder(mainSession.settings)
                .contextId("1")
                .build())

        session3.loadUri("https://example.com")
        session3.waitForPageStop()

        localStorage = session3.evaluateJS("""
            localStorage.getItem('ctx') || 'null'
        """) as String

        assertThat("Local storage value should match",
                   localStorage,
                   equalTo("null"))

        val session4 = sessionRule.createOpenSession(
                GeckoSessionSettings.Builder(mainSession.settings)
                .contextId("2")
                .build())

        session4.loadUri("https://example.com")
        session4.waitForPageStop()

        localStorage = session4.evaluateJS("""
            localStorage.getItem('ctx') || 'null'
        """) as String

        assertThat("Local storage value should match",
                   localStorage,
                   equalTo("2"))
    }
}
