/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.CookiePolicy
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.Config
import org.mozilla.fenix.R
import org.mozilla.fenix.ReleaseChannel
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class TrackingProtectionPolicyFactoryTest {

    private lateinit var all: String
    private lateinit var social: String
    private lateinit var thirdParty: String
    private lateinit var unvisited: String
    private lateinit var private: String

    @Before
    fun setup() {
        mockkObject(Config)
        every { Config.channel } returns ReleaseChannel.Nightly

        all = testContext.resources.getString(R.string.all)
        social = testContext.resources.getString(R.string.social)
        thirdParty = testContext.resources.getString(R.string.third_party)
        unvisited = testContext.resources.getString(R.string.unvisited)
        private = testContext.resources.getString(R.string.private_string)
    }

    @Test
    fun `WHEN useStrictMode is true then SHOULD return strict mode`() {
        val expected = TrackingProtectionPolicy.strict()

        val factory = TrackingProtectionPolicyFactory(
            mockSettings(useStrict = true),
            testContext.resources,
        )

        val privateOnly =
            factory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)
        val normalOnly =
            factory.createTrackingProtectionPolicy(normalMode = true, privateMode = false)
        val always = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)
        val none = factory.createTrackingProtectionPolicy(normalMode = false, privateMode = false)

        expected.assertPolicyEquals(privateOnly, checkPrivacy = false)
        expected.assertPolicyEquals(normalOnly, checkPrivacy = false)
        expected.assertPolicyEquals(always, checkPrivacy = false)
        TrackingProtectionPolicy.none().assertPolicyEquals(none, checkPrivacy = false)
    }

    @Test
    fun `WHEN neither use strict nor use custom is true SHOULD return recommended mode`() {
        val expected = TrackingProtectionPolicy.recommended()

        val factory = TrackingProtectionPolicyFactory(
            mockSettings(useStrict = false, useCustom = false),
            testContext.resources,
        )

        val privateOnly =
            factory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)
        val normalOnly =
            factory.createTrackingProtectionPolicy(normalMode = true, privateMode = false)
        val always = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)
        val none = factory.createTrackingProtectionPolicy(normalMode = false, privateMode = false)

        expected.assertPolicyEquals(privateOnly, checkPrivacy = false)
        expected.assertPolicyEquals(normalOnly, checkPrivacy = false)
        expected.assertPolicyEquals(always, checkPrivacy = false)
        TrackingProtectionPolicy.none().assertPolicyEquals(none, checkPrivacy = false)
    }

    @Test
    fun `GIVEN custom policy WHEN should not block cookies THEN tracking policy should not block cookies`() {
        val expected = TrackingProtectionPolicy.select(
            cookiePolicy = CookiePolicy.ACCEPT_ALL,
            trackingCategories = allTrackingCategories,
            strictSocialTrackingProtection = true,
        )

        val factory = TrackingProtectionPolicyFactory(
            settingsForCustom(shouldBlockCookiesInCustom = false),
            testContext.resources,
        )

        val privateOnly =
            factory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)
        val normalOnly =
            factory.createTrackingProtectionPolicy(normalMode = true, privateMode = false)
        val always = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        expected.assertPolicyEquals(privateOnly, checkPrivacy = false)
        expected.assertPolicyEquals(normalOnly, checkPrivacy = false)
        expected.assertPolicyEquals(always, checkPrivacy = false)
    }

    @Test
    fun `GIVEN custom policy WHEN cookie policy block all THEN tracking policy should have cookie policy allow none`() {
        val expected = TrackingProtectionPolicy.select(
            cookiePolicy = CookiePolicy.ACCEPT_NONE,
            trackingCategories = allTrackingCategories,
            strictSocialTrackingProtection = true,
        )

        val factory = TrackingProtectionPolicyFactory(
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockCookiesSelection = all,
            ),
            testContext.resources,
        )

        val privateOnly =
            factory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)
        val normalOnly =
            factory.createTrackingProtectionPolicy(normalMode = true, privateMode = false)
        val always = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        expected.assertPolicyEquals(privateOnly, checkPrivacy = false)
        expected.assertPolicyEquals(normalOnly, checkPrivacy = false)
        expected.assertPolicyEquals(always, checkPrivacy = false)
    }

    @Test
    fun `GIVEN TCP is enabled by nimbus WHEN applyTCPIfNeeded THEN cookie policy should be TCP`() {
        val settings: Settings = mockk(relaxed = true)
        every { settings.enabledTotalCookieProtection } returns true

        val policies = arrayOf(
            TrackingProtectionPolicy.strict(),
            TrackingProtectionPolicy.recommended(),
            TrackingProtectionPolicy.select(),
        )

        for (policy in policies) {
            val adaptedPolicy = policy.applyTCPIfNeeded(settings)
            assertEquals(
                CookiePolicy.ACCEPT_FIRST_PARTY_AND_ISOLATE_OTHERS,
                adaptedPolicy.cookiePolicy,
            )
        }
    }

    fun `GIVEN TCP is NOT enabled by nimbus WHEN applyTCPIfNeeded THEN reuse cookie policy`() {
        val settings: Settings = mockk(relaxed = true)

        every { settings.enabledTotalCookieProtection } returns false

        val policies = arrayOf(
            TrackingProtectionPolicy.strict(),
            TrackingProtectionPolicy.recommended(),
            TrackingProtectionPolicy.select(),
        )

        for (policy in policies) {
            val adaptedPolicy = policy.applyTCPIfNeeded(settings)
            assertEquals(
                policy.cookiePolicy,
                adaptedPolicy.cookiePolicy,
            )
        }
    }

    @Test
    fun `GIVEN custom policy WHEN cookie policy social THEN tracking policy should have cookie policy allow non-trackers`() {
        val expected = TrackingProtectionPolicy.select(
            cookiePolicy = CookiePolicy.ACCEPT_NON_TRACKERS,
            trackingCategories = allTrackingCategories,
            strictSocialTrackingProtection = true,
        )

        val factory = TrackingProtectionPolicyFactory(
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockCookiesSelection = social,
            ),
            testContext.resources,
        )

        val privateOnly =
            factory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)
        val normalOnly =
            factory.createTrackingProtectionPolicy(normalMode = true, privateMode = false)
        val always = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        expected.assertPolicyEquals(privateOnly, checkPrivacy = false)
        expected.assertPolicyEquals(normalOnly, checkPrivacy = false)
        expected.assertPolicyEquals(always, checkPrivacy = false)
    }

    @Test
    fun `GIVEN custom policy WHEN cookie policy accept visited THEN tracking policy should have cookie policy allow visited`() {
        val expected = TrackingProtectionPolicy.select(
            cookiePolicy = CookiePolicy.ACCEPT_VISITED,
            trackingCategories = allTrackingCategories,
            strictSocialTrackingProtection = true,
        )

        val factory = TrackingProtectionPolicyFactory(
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockCookiesSelection = unvisited,
            ),
            testContext.resources,
        )

        val privateOnly =
            factory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)
        val normalOnly =
            factory.createTrackingProtectionPolicy(normalMode = true, privateMode = false)
        val always = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        expected.assertPolicyEquals(privateOnly, checkPrivacy = false)
        expected.assertPolicyEquals(normalOnly, checkPrivacy = false)
        expected.assertPolicyEquals(always, checkPrivacy = false)
    }

    @Test
    fun `GIVEN custom policy WHEN blockTrackingContentInCustom in private windows only THEN strict social tracking protection should be false`() {
        val privateFactory = TrackingProtectionPolicyFactory(
            settingsForCustom(
                shouldBlockCookiesInCustom = false,
                blockTrackingContentInCustom = private,
            ),
            testContext.resources,
        )

        val privateOnly =
            privateFactory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)

        assertFalse(privateOnly.strictSocialTrackingProtection!!)
    }

    @Test
    fun `GIVEN custom policy WHEN blockTrackingContentInCustom in all windows THEN strict social tracking protection should be true`() {
        val privateFactory = TrackingProtectionPolicyFactory(
            settingsForCustom(
                shouldBlockCookiesInCustom = false,
                blockTrackingContentInCustom = all,
            ),
            testContext.resources,
        )

        val privateOnly =
            privateFactory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)

        assertTrue(privateOnly.strictSocialTrackingProtection!!)
    }

    @Test
    fun `GIVEN custom policy WHEN cookie policy block third party THEN tracking policy should have cookie policy allow first party`() {
        val expected = TrackingProtectionPolicy.select(
            cookiePolicy = CookiePolicy.ACCEPT_ONLY_FIRST_PARTY,
            trackingCategories = allTrackingCategories,
            strictSocialTrackingProtection = true,
        )

        val factory = TrackingProtectionPolicyFactory(
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockCookiesSelection = thirdParty,
            ),
            testContext.resources,
        )

        val privateOnly =
            factory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)
        val normalOnly =
            factory.createTrackingProtectionPolicy(normalMode = true, privateMode = false)
        val always = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        expected.assertPolicyEquals(privateOnly, checkPrivacy = false)
        expected.assertPolicyEquals(normalOnly, checkPrivacy = false)
        expected.assertPolicyEquals(always, checkPrivacy = false)
    }

    @Test
    fun `GIVEN custom policy WHEN cookie policy is total protection THEN tracking policy should have cookie policy to block cross-site cookies`() {
        val expected = TrackingProtectionPolicy.select(
            cookiePolicy = CookiePolicy.ACCEPT_FIRST_PARTY_AND_ISOLATE_OTHERS,
            trackingCategories = allTrackingCategories,
            strictSocialTrackingProtection = true,
        )

        val factory = TrackingProtectionPolicyFactory(
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockCookiesSelection = "total-protection",
            ),
            testContext.resources,
        )

        val privateOnly =
            factory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)
        val normalOnly =
            factory.createTrackingProtectionPolicy(normalMode = true, privateMode = false)
        val always = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        expected.assertPolicyEquals(privateOnly, checkPrivacy = false)
        expected.assertPolicyEquals(normalOnly, checkPrivacy = false)
        expected.assertPolicyEquals(always, checkPrivacy = false)
    }

    @Test
    fun `GIVEN custom policy WHEN cookie policy unrecognized THEN tracking policy should have cookie policy block all`() {
        val expected = TrackingProtectionPolicy.select(
            cookiePolicy = CookiePolicy.ACCEPT_NONE,
            trackingCategories = allTrackingCategories,
            strictSocialTrackingProtection = true,
        )

        val factory = TrackingProtectionPolicyFactory(
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockCookiesSelection = "some text!",
            ),
            testContext.resources,
        )

        val privateOnly =
            factory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)
        val normalOnly =
            factory.createTrackingProtectionPolicy(normalMode = true, privateMode = false)
        val always = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        expected.assertPolicyEquals(privateOnly, checkPrivacy = false)
        expected.assertPolicyEquals(normalOnly, checkPrivacy = false)
        expected.assertPolicyEquals(always, checkPrivacy = false)
    }

    @Test
    fun `all cookies_options_entry_values values should create policies without crashing`() {
        testContext.resources.getStringArray(R.array.cookies_options_entry_values).forEach {
            TrackingProtectionPolicyFactory(
                settingsForCustom(
                    shouldBlockCookiesInCustom = true,
                    blockCookiesSelection = it,
                ),
                testContext.resources,
            )
                .createTrackingProtectionPolicy(normalMode = true, privateMode = true)
        }
    }

    @Test
    fun `factory should construct policies with privacy settings that match their inputs`() {
        val allFactories = listOf(
            TrackingProtectionPolicyFactory(
                mockSettings(useStrict = true),
                testContext.resources,
            ),
            TrackingProtectionPolicyFactory(
                mockSettings(useStrict = false, useCustom = false),
                testContext.resources,
            ),
        )

        allFactories.map {
            it.createTrackingProtectionPolicy(normalMode = true, privateMode = false)
        }.forEach {
            assertTrue(it.useForRegularSessions)
            assertFalse(it.useForPrivateSessions)
        }

        allFactories.map {
            it.createTrackingProtectionPolicy(normalMode = false, privateMode = true)
        }.forEach {
            assertTrue(it.useForPrivateSessions)
            assertFalse(it.useForRegularSessions)
        }

        allFactories.map {
            it.createTrackingProtectionPolicy(normalMode = true, privateMode = true)
        }.forEach {
            assertTrue(it.useForRegularSessions)
            assertTrue(it.useForPrivateSessions)
        }

        // `normalMode = true, privateMode = true` can never be shown to the user
    }

    @Test
    fun `factory should follow global ETP settings by default`() {
        var useETPFactory = TrackingProtectionPolicyFactory(
            mockSettings(useTrackingProtection = true),
            testContext.resources,
        )
        var policy = useETPFactory.createTrackingProtectionPolicy()
        assertTrue(policy.useForPrivateSessions)
        assertTrue(policy.useForRegularSessions)

        useETPFactory = TrackingProtectionPolicyFactory(
            mockSettings(useTrackingProtection = false),
            testContext.resources,
        )
        policy = useETPFactory.createTrackingProtectionPolicy()
        assertEquals(policy, TrackingProtectionPolicy.none())
    }

    @Test
    fun `custom tabs should respect their privacy rules`() {
        val allSettings = listOf(
            settingsForCustom(
                shouldBlockCookiesInCustom = false,
                blockTrackingContentInCustom = all,
            ),
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockCookiesSelection = all,
                blockTrackingContentInCustom = all,
            ),
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockCookiesSelection = all,
                blockTrackingContentInCustom = all,
            ),
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockCookiesSelection = unvisited,
                blockTrackingContentInCustom = all,
            ),
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockCookiesSelection = thirdParty,
                blockTrackingContentInCustom = all,
            ),
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockCookiesSelection = "some text!",
                blockTrackingContentInCustom = all,
            ),
        )

        val privateSettings = listOf(
            settingsForCustom(
                shouldBlockCookiesInCustom = false,
                blockTrackingContentInCustom = private,
            ),
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockCookiesSelection = all,
                blockTrackingContentInCustom = private,
            ),
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockCookiesSelection = all,
                blockTrackingContentInCustom = private,
            ),
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockCookiesSelection = unvisited,
                blockTrackingContentInCustom = private,
            ),
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockCookiesSelection = thirdParty,
                blockTrackingContentInCustom = private,
            ),
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockCookiesSelection = "some text!",
                blockTrackingContentInCustom = private,
            ),
        )

        allSettings.map {
            TrackingProtectionPolicyFactory(
                it,
                testContext.resources,
            ).createTrackingProtectionPolicy(
                normalMode = true,
                privateMode = true,
            )
        }
            .forEach {
                assertTrue(it.useForRegularSessions)
                assertTrue(it.useForPrivateSessions)
            }

        privateSettings.map {
            TrackingProtectionPolicyFactory(
                it,
                testContext.resources,
            ).createTrackingProtectionPolicy(
                normalMode = true,
                privateMode = true,
            )
        }
            .forEach {
                assertFalse(it.useForRegularSessions)
                assertTrue(it.useForPrivateSessions)
            }
    }

    @Test
    fun `GIVEN custom policy WHEN default tracking policies THEN tracking policies should match default`() {
        val defaultTrackingCategories = arrayOf(
            TrackingProtectionPolicy.TrackingCategory.AD,
            TrackingProtectionPolicy.TrackingCategory.ANALYTICS,
            TrackingProtectionPolicy.TrackingCategory.SOCIAL,
            TrackingProtectionPolicy.TrackingCategory.MOZILLA_SOCIAL,
        )

        val expected = TrackingProtectionPolicy.select(
            cookiePolicy = CookiePolicy.ACCEPT_NONE,
            trackingCategories = defaultTrackingCategories,
            strictSocialTrackingProtection = false,
        )

        val factory = TrackingProtectionPolicyFactory(
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockTrackingContent = false,
                blockFingerprinters = false,
                blockCryptominers = false,
            ),
            testContext.resources,
        )
        val actual = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        expected.assertPolicyEquals(actual, checkPrivacy = false)
    }

    @Test
    fun `GIVEN custom policy WHEN all tracking policies THEN tracking policies should match all`() {
        val expected = TrackingProtectionPolicy.select(
            cookiePolicy = CookiePolicy.ACCEPT_NONE,
            trackingCategories = allTrackingCategories,
            strictSocialTrackingProtection = true,
        )

        val factory = TrackingProtectionPolicyFactory(
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockTrackingContent = true,
                blockFingerprinters = true,
                blockCryptominers = true,
            ),
            testContext.resources,
        )
        val actual = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        expected.assertPolicyEquals(actual, checkPrivacy = false)
    }

    @Test
    fun `GIVEN custom policy WHEN some tracking policies THEN tracking policies should match passed policies`() {
        val someTrackingCategories = arrayOf(
            TrackingProtectionPolicy.TrackingCategory.AD,
            TrackingProtectionPolicy.TrackingCategory.ANALYTICS,
            TrackingProtectionPolicy.TrackingCategory.SOCIAL,
            TrackingProtectionPolicy.TrackingCategory.MOZILLA_SOCIAL,
            TrackingProtectionPolicy.TrackingCategory.FINGERPRINTING,
        )

        val expected = TrackingProtectionPolicy.select(
            cookiePolicy = CookiePolicy.ACCEPT_NONE,
            trackingCategories = someTrackingCategories,
            strictSocialTrackingProtection = false,
        )

        val factory = TrackingProtectionPolicyFactory(
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockTrackingContent = false,
                blockFingerprinters = true,
                blockCryptominers = false,
                blockRedirectTrackers = true,
            ),
            testContext.resources,
        )
        val actual = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        expected.assertPolicyEquals(actual, checkPrivacy = false)
    }

    @Test
    fun `GIVEN custom policy WHEN some tracking policies THEN purge cookies`() {
        val expected = TrackingProtectionPolicy.select(
            cookiePolicy = CookiePolicy.ACCEPT_NONE,
            trackingCategories = allTrackingCategories,
            cookiePurging = true,
            strictSocialTrackingProtection = true,
        )

        val factory = TrackingProtectionPolicyFactory(
            settingsForCustom(shouldBlockCookiesInCustom = true),
            testContext.resources,
        )

        val privateOnly =
            factory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)
        val normalOnly =
            factory.createTrackingProtectionPolicy(normalMode = true, privateMode = false)
        val always = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        expected.assertPolicyEquals(privateOnly, checkPrivacy = false)
        expected.assertPolicyEquals(normalOnly, checkPrivacy = false)
        expected.assertPolicyEquals(always, checkPrivacy = false)
    }

    @Test
    fun `GIVEN strict policy WHEN some tracking policies THEN purge cookies`() {
        val expected = TrackingProtectionPolicy.strict()

        val factory = TrackingProtectionPolicyFactory(
            mockSettings(
                useStrict = true,
                useTrackingProtection = true,
            ),
            testContext.resources,
        )

        val privateOnly =
            factory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)
        val normalOnly =
            factory.createTrackingProtectionPolicy(normalMode = true, privateMode = false)
        val always = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        assertEquals(privateOnly.cookiePurging, expected.cookiePurging)
        assertEquals(normalOnly.cookiePurging, expected.cookiePurging)
        assertEquals(always.cookiePurging, expected.cookiePurging)
    }

    @Test
    fun `GIVEN standard policy WHEN some tracking policies THEN purge cookies`() {
        val expected = TrackingProtectionPolicy.recommended()

        val factory = TrackingProtectionPolicyFactory(
            mockSettings(
                useStrict = false,
                useCustom = false,
                useTrackingProtection = true,
            ),
            testContext.resources,
        )

        val privateOnly =
            factory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)
        val normalOnly =
            factory.createTrackingProtectionPolicy(normalMode = true, privateMode = false)
        val always = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        assertEquals(privateOnly.cookiePurging, expected.cookiePurging)
        assertEquals(normalOnly.cookiePurging, expected.cookiePurging)
        assertEquals(always.cookiePurging, expected.cookiePurging)
    }

    private fun mockSettings(
        useStrict: Boolean = false,
        useCustom: Boolean = false,
        useTrackingProtection: Boolean = false,
    ): Settings = mockk {
        every { enabledTotalCookieProtection } returns false
        every { useStrictTrackingProtection } returns useStrict
        every { useCustomTrackingProtection } returns useCustom
        every { shouldUseTrackingProtection } returns useTrackingProtection
    }

    private fun settingsForCustom(
        shouldBlockCookiesInCustom: Boolean,
        blockTrackingContentInCustom: String = all, // ["private", "all"]
        blockCookiesSelection: String = all, // values from R.array.cookies_options_entry_values
        blockTrackingContent: Boolean = true,
        blockFingerprinters: Boolean = true,
        blockCryptominers: Boolean = true,
        blockRedirectTrackers: Boolean = true,
    ): Settings = mockSettings(useStrict = false, useCustom = true).apply {
        every { blockTrackingContentSelectionInCustomTrackingProtection } returns blockTrackingContentInCustom

        every { blockCookiesInCustomTrackingProtection } returns shouldBlockCookiesInCustom
        every { blockCookiesSelectionInCustomTrackingProtection } returns blockCookiesSelection
        every { blockTrackingContentInCustomTrackingProtection } returns blockTrackingContent
        every { blockFingerprintersInCustomTrackingProtection } returns blockFingerprinters
        every { blockCryptominersInCustomTrackingProtection } returns blockCryptominers
        every { blockRedirectTrackersInCustomTrackingProtection } returns blockRedirectTrackers
    }

    private fun TrackingProtectionPolicy.assertPolicyEquals(
        actual: TrackingProtectionPolicy,
        checkPrivacy: Boolean,
    ) {
        assertEquals(this.cookiePolicy, actual.cookiePolicy)
        val strictSocialTrackingProtection =
            if (actual.useForPrivateSessions && !actual.useForRegularSessions) {
                false
            } else {
                this.strictSocialTrackingProtection
            }
        assertEquals(strictSocialTrackingProtection, actual.strictSocialTrackingProtection)
        // E.g., atm, RECOMMENDED == AD + ANALYTICS + SOCIAL + TEST + MOZILLA_SOCIAL + CRYPTOMINING.
        // If all of these are set manually, the equality check should not fail
        if (this.trackingCategories.toInt() != actual.trackingCategories.toInt()) {
            assertArrayEquals(this.trackingCategories, actual.trackingCategories)
        }

        if (checkPrivacy) {
            assertEquals(this.useForPrivateSessions, actual.useForPrivateSessions)
            assertEquals(this.useForRegularSessions, actual.useForRegularSessions)
        }
    }

    private fun Array<TrackingProtectionPolicy.TrackingCategory>.toInt(): Int {
        return fold(initial = 0) { acc, next -> acc + next.id }
    }

    private val allTrackingCategories = arrayOf(
        TrackingProtectionPolicy.TrackingCategory.AD,
        TrackingProtectionPolicy.TrackingCategory.ANALYTICS,
        TrackingProtectionPolicy.TrackingCategory.SOCIAL,
        TrackingProtectionPolicy.TrackingCategory.MOZILLA_SOCIAL,
        TrackingProtectionPolicy.TrackingCategory.SCRIPTS_AND_SUB_RESOURCES,
        TrackingProtectionPolicy.TrackingCategory.FINGERPRINTING,
        TrackingProtectionPolicy.TrackingCategory.CRYPTOMINING,
    )
}
