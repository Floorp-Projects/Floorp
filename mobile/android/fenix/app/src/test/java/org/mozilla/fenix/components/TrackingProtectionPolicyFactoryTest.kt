package org.mozilla.fenix.components

import io.mockk.every
import io.mockk.mockk
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class TrackingProtectionPolicyFactoryTest {

    @Test
    fun `WHEN useStrictMode is true then SHOULD return strict mode`() {
        val expected = EngineSession.TrackingProtectionPolicy.strict()

        val factory = TrackingProtectionPolicyFactory(mockSettings(useStrict = true))

        val privateOnly = factory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)
        val normalOnly = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = false)
        val always = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)
        val none = factory.createTrackingProtectionPolicy(normalMode = false, privateMode = false)

        expected.assertPolicyEquals(privateOnly, checkPrivacy = false)
        expected.assertPolicyEquals(normalOnly, checkPrivacy = false)
        expected.assertPolicyEquals(always, checkPrivacy = false)
        EngineSession.TrackingProtectionPolicy.none().assertPolicyEquals(none, checkPrivacy = false)
    }

    @Test
    fun `WHEN neither use strict nor use custom is true SHOULD return recommended mode`() {
        val expected = EngineSession.TrackingProtectionPolicy.recommended()

        val factory = TrackingProtectionPolicyFactory(mockSettings(useStrict = false, useCustom = false))

        val privateOnly = factory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)
        val normalOnly = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = false)
        val always = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)
        val none = factory.createTrackingProtectionPolicy(normalMode = false, privateMode = false)

        expected.assertPolicyEquals(privateOnly, checkPrivacy = false)
        expected.assertPolicyEquals(normalOnly, checkPrivacy = false)
        expected.assertPolicyEquals(always, checkPrivacy = false)
        EngineSession.TrackingProtectionPolicy.none().assertPolicyEquals(none, checkPrivacy = false)
    }

    @Test
    fun `GIVEN custom policy WHEN should not block cookies THEN tracking policy should not block cookies`() {
        val expected = EngineSession.TrackingProtectionPolicy.select(
            cookiePolicy = EngineSession.TrackingProtectionPolicy.CookiePolicy.ACCEPT_ALL,
            trackingCategories = allTrackingCategories
        )

        val factory = TrackingProtectionPolicyFactory(settingsForCustom(shouldBlockCookiesInCustom = false))

        val privateOnly = factory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)
        val normalOnly = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = false)
        val always = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        expected.assertPolicyEquals(privateOnly, checkPrivacy = false)
        expected.assertPolicyEquals(normalOnly, checkPrivacy = false)
        expected.assertPolicyEquals(always, checkPrivacy = false)
    }

    @Test
    fun `GIVEN custom policy WHEN cookie policy block all THEN tracking policy should have cookie policy allow none`() {
        val expected = EngineSession.TrackingProtectionPolicy.select(
            cookiePolicy = EngineSession.TrackingProtectionPolicy.CookiePolicy.ACCEPT_NONE,
            trackingCategories = allTrackingCategories
        )

        val factory = TrackingProtectionPolicyFactory(settingsForCustom(shouldBlockCookiesInCustom = true, blockCookiesSelection = "all"))

        val privateOnly = factory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)
        val normalOnly = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = false)
        val always = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        expected.assertPolicyEquals(privateOnly, checkPrivacy = false)
        expected.assertPolicyEquals(normalOnly, checkPrivacy = false)
        expected.assertPolicyEquals(always, checkPrivacy = false)
    }

    @Test
    fun `GIVEN custom policy WHEN cookie policy social THEN tracking policy should have cookie policy allow non-trackers`() {
        val expected = EngineSession.TrackingProtectionPolicy.select(
            cookiePolicy = EngineSession.TrackingProtectionPolicy.CookiePolicy.ACCEPT_NON_TRACKERS,
            trackingCategories = allTrackingCategories
        )

        val factory = TrackingProtectionPolicyFactory(settingsForCustom(shouldBlockCookiesInCustom = true, blockCookiesSelection = "social"))

        val privateOnly = factory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)
        val normalOnly = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = false)
        val always = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        expected.assertPolicyEquals(privateOnly, checkPrivacy = false)
        expected.assertPolicyEquals(normalOnly, checkPrivacy = false)
        expected.assertPolicyEquals(always, checkPrivacy = false)
    }

    @Test
    fun `GIVEN custom policy WHEN cookie policy accept visited THEN tracking policy should have cookie policy allow visited`() {
        val expected = EngineSession.TrackingProtectionPolicy.select(
            cookiePolicy = EngineSession.TrackingProtectionPolicy.CookiePolicy.ACCEPT_VISITED,
            trackingCategories = allTrackingCategories
        )

        val factory = TrackingProtectionPolicyFactory(settingsForCustom(shouldBlockCookiesInCustom = true, blockCookiesSelection = "unvisited"))

        val privateOnly = factory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)
        val normalOnly = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = false)
        val always = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        expected.assertPolicyEquals(privateOnly, checkPrivacy = false)
        expected.assertPolicyEquals(normalOnly, checkPrivacy = false)
        expected.assertPolicyEquals(always, checkPrivacy = false)
    }

    @Test
    fun `GIVEN custom policy WHEN cookie policy block third party THEN tracking policy should have cookie policy allow first party`() {
        val expected = EngineSession.TrackingProtectionPolicy.select(
            cookiePolicy = EngineSession.TrackingProtectionPolicy.CookiePolicy.ACCEPT_ONLY_FIRST_PARTY,
            trackingCategories = allTrackingCategories
        )

        val factory = TrackingProtectionPolicyFactory(settingsForCustom(shouldBlockCookiesInCustom = true, blockCookiesSelection = "third-party"))

        val privateOnly = factory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)
        val normalOnly = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = false)
        val always = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        expected.assertPolicyEquals(privateOnly, checkPrivacy = false)
        expected.assertPolicyEquals(normalOnly, checkPrivacy = false)
        expected.assertPolicyEquals(always, checkPrivacy = false)
    }

    @Test
    fun `GIVEN custom policy WHEN cookie policy unrecognized THEN tracking policy should have cookie policy block all`() {
        val expected = EngineSession.TrackingProtectionPolicy.select(
            cookiePolicy = EngineSession.TrackingProtectionPolicy.CookiePolicy.ACCEPT_NONE,
            trackingCategories = allTrackingCategories
        )

        val factory = TrackingProtectionPolicyFactory(settingsForCustom(shouldBlockCookiesInCustom = true, blockCookiesSelection = "some text!"))

        val privateOnly = factory.createTrackingProtectionPolicy(normalMode = false, privateMode = true)
        val normalOnly = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = false)
        val always = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        expected.assertPolicyEquals(privateOnly, checkPrivacy = false)
        expected.assertPolicyEquals(normalOnly, checkPrivacy = false)
        expected.assertPolicyEquals(always, checkPrivacy = false)
    }

    @Test
    fun `all cookies_options_entry_values values should create policies without crashing`() {
        testContext.resources.getStringArray(R.array.cookies_options_entry_values).forEach {
            TrackingProtectionPolicyFactory(settingsForCustom(shouldBlockCookiesInCustom = true, blockCookiesSelection = it))
                .createTrackingProtectionPolicy(normalMode = true, privateMode = true)
        }
    }

    @Test
    fun `factory should construct policies with privacy settings that match their inputs`() {
        val allFactories = listOf(
            TrackingProtectionPolicyFactory(mockSettings(useStrict = true)),
            TrackingProtectionPolicyFactory(mockSettings(useStrict = false, useCustom = false))
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
        var useETPFactory = TrackingProtectionPolicyFactory(mockSettings(useTrackingProtection = true))
        var policy = useETPFactory.createTrackingProtectionPolicy()
        assertTrue(policy.useForPrivateSessions)
        assertTrue(policy.useForRegularSessions)

        useETPFactory = TrackingProtectionPolicyFactory(mockSettings(useTrackingProtection = false))
        policy = useETPFactory.createTrackingProtectionPolicy()
        assertEquals(policy, EngineSession.TrackingProtectionPolicy.none())
    }

    @Test
    fun `custom tabs should respect their privacy rules`() {
        val allSettings = listOf(
            settingsForCustom(shouldBlockCookiesInCustom = false, blockTrackingContentInCustom = "all"),
            settingsForCustom(shouldBlockCookiesInCustom = true, blockCookiesSelection = "all", blockTrackingContentInCustom = "all"),
            settingsForCustom(shouldBlockCookiesInCustom = true, blockCookiesSelection = "all", blockTrackingContentInCustom = "all"),
            settingsForCustom(shouldBlockCookiesInCustom = true, blockCookiesSelection = "unvisited", blockTrackingContentInCustom = "all"),
            settingsForCustom(shouldBlockCookiesInCustom = true, blockCookiesSelection = "third-party", blockTrackingContentInCustom = "all"),
            settingsForCustom(shouldBlockCookiesInCustom = true, blockCookiesSelection = "some text!", blockTrackingContentInCustom = "all")
            )

        val privateSettings = listOf(
            settingsForCustom(shouldBlockCookiesInCustom = false, blockTrackingContentInCustom = "private"),
            settingsForCustom(shouldBlockCookiesInCustom = true, blockCookiesSelection = "all", blockTrackingContentInCustom = "private"),
            settingsForCustom(shouldBlockCookiesInCustom = true, blockCookiesSelection = "all", blockTrackingContentInCustom = "private"),
            settingsForCustom(shouldBlockCookiesInCustom = true, blockCookiesSelection = "unvisited", blockTrackingContentInCustom = "private"),
            settingsForCustom(shouldBlockCookiesInCustom = true, blockCookiesSelection = "third-party", blockTrackingContentInCustom = "private"),
            settingsForCustom(shouldBlockCookiesInCustom = true, blockCookiesSelection = "some text!", blockTrackingContentInCustom = "private")
            )

        allSettings.map { TrackingProtectionPolicyFactory(it).createTrackingProtectionPolicy(
            normalMode = true,
            privateMode = true
        ) }
            .forEach {
                assertTrue(it.useForRegularSessions)
                assertTrue(it.useForPrivateSessions)
            }

        privateSettings.map { TrackingProtectionPolicyFactory(it).createTrackingProtectionPolicy(
            normalMode = true,
            privateMode = true
        ) }
            .forEach {
                assertFalse(it.useForRegularSessions)
                assertTrue(it.useForPrivateSessions)
            }
    }

    @Test
    fun `GIVEN custom policy WHEN default tracking policies THEN tracking policies should match default`() {
        val defaultTrackingCategories = arrayOf(
            EngineSession.TrackingProtectionPolicy.TrackingCategory.AD,
            EngineSession.TrackingProtectionPolicy.TrackingCategory.ANALYTICS,
            EngineSession.TrackingProtectionPolicy.TrackingCategory.SOCIAL,
            EngineSession.TrackingProtectionPolicy.TrackingCategory.MOZILLA_SOCIAL
        )

        val expected = EngineSession.TrackingProtectionPolicy.select(
            cookiePolicy = EngineSession.TrackingProtectionPolicy.CookiePolicy.ACCEPT_NONE,
            trackingCategories = defaultTrackingCategories
        )

        val factory = TrackingProtectionPolicyFactory(
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockTrackingContent = false,
                blockFingerprinters = false,
                blockCryptominers = false
            )
        )
        val actual = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        expected.assertPolicyEquals(actual, checkPrivacy = false)
    }

    @Test
    fun `GIVEN custom policy WHEN all tracking policies THEN tracking policies should match all`() {
        val expected = EngineSession.TrackingProtectionPolicy.select(
            cookiePolicy = EngineSession.TrackingProtectionPolicy.CookiePolicy.ACCEPT_NONE,
            trackingCategories = allTrackingCategories
        )

        val factory = TrackingProtectionPolicyFactory(
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockTrackingContent = true,
                blockFingerprinters = true,
                blockCryptominers = true
            )
        )
        val actual = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        expected.assertPolicyEquals(actual, checkPrivacy = false)
    }

    @Test
    fun `GIVEN custom policy WHEN some tracking policies THEN tracking policies should match passed policies`() {
        val someTrackingCategories = arrayOf(
            EngineSession.TrackingProtectionPolicy.TrackingCategory.AD,
            EngineSession.TrackingProtectionPolicy.TrackingCategory.ANALYTICS,
            EngineSession.TrackingProtectionPolicy.TrackingCategory.SOCIAL,
            EngineSession.TrackingProtectionPolicy.TrackingCategory.MOZILLA_SOCIAL,
            EngineSession.TrackingProtectionPolicy.TrackingCategory.FINGERPRINTING
        )

        val expected = EngineSession.TrackingProtectionPolicy.select(
            cookiePolicy = EngineSession.TrackingProtectionPolicy.CookiePolicy.ACCEPT_NONE,
            trackingCategories = someTrackingCategories
        )

        val factory = TrackingProtectionPolicyFactory(
            settingsForCustom(
                shouldBlockCookiesInCustom = true,
                blockTrackingContent = false,
                blockFingerprinters = true,
                blockCryptominers = false
            )
        )
        val actual = factory.createTrackingProtectionPolicy(normalMode = true, privateMode = true)

        expected.assertPolicyEquals(actual, checkPrivacy = false)
    }
}

private fun mockSettings(
    useStrict: Boolean = false,
    useCustom: Boolean = false,
    useTrackingProtection: Boolean = false
): Settings = mockk {
    every { useStrictTrackingProtection } returns useStrict
    every { useCustomTrackingProtection } returns useCustom
    every { shouldUseTrackingProtection } returns useTrackingProtection
}

@Suppress("LongParameterList")
private fun settingsForCustom(
    shouldBlockCookiesInCustom: Boolean,
    blockTrackingContentInCustom: String = "all", // ["private", "all"]
    blockCookiesSelection: String = "all", // values from R.array.cookies_options_entry_values
    blockTrackingContent: Boolean = true,
    blockFingerprinters: Boolean = true,
    blockCryptominers: Boolean = true
): Settings = mockSettings(useStrict = false, useCustom = true).apply {

    every { blockTrackingContentSelectionInCustomTrackingProtection } returns blockTrackingContentInCustom

    every { blockCookiesInCustomTrackingProtection } returns shouldBlockCookiesInCustom
    every { blockCookiesSelectionInCustomTrackingProtection } returns blockCookiesSelection
    every { blockTrackingContentInCustomTrackingProtection } returns blockTrackingContent
    every { blockFingerprintersInCustomTrackingProtection } returns blockFingerprinters
    every { blockCryptominersInCustomTrackingProtection } returns blockCryptominers
}

private fun EngineSession.TrackingProtectionPolicy.assertPolicyEquals(
    actual: EngineSession.TrackingProtectionPolicy,
    checkPrivacy: Boolean
) {
    assertEquals(this.cookiePolicy, actual.cookiePolicy)
    assertEquals(this.strictSocialTrackingProtection, actual.strictSocialTrackingProtection)
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

private fun Array<EngineSession.TrackingProtectionPolicy.TrackingCategory>.toInt(): Int {
    return fold(initial = 0) { acc, next -> acc + next.id }
}

private val allTrackingCategories = arrayOf(
    EngineSession.TrackingProtectionPolicy.TrackingCategory.AD,
    EngineSession.TrackingProtectionPolicy.TrackingCategory.ANALYTICS,
    EngineSession.TrackingProtectionPolicy.TrackingCategory.SOCIAL,
    EngineSession.TrackingProtectionPolicy.TrackingCategory.MOZILLA_SOCIAL,
    EngineSession.TrackingProtectionPolicy.TrackingCategory.SCRIPTS_AND_SUB_RESOURCES,
    EngineSession.TrackingProtectionPolicy.TrackingCategory.FINGERPRINTING,
    EngineSession.TrackingProtectionPolicy.TrackingCategory.CRYPTOMINING
)
