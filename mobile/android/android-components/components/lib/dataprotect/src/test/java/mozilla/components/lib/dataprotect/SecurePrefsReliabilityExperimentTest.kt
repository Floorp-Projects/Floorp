package mozilla.components.lib.dataprotect

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.lib.dataprotect.SecurePrefsReliabilityExperiment.Companion.Actions
import mozilla.components.lib.dataprotect.SecurePrefsReliabilityExperiment.Companion.Values
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.FactProcessor
import mozilla.components.support.base.facts.Facts
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.reset
import org.mockito.Mockito.times
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
class SecurePrefsReliabilityExperimentTest {
    @Config(sdk = [21])
    @Test
    fun `working first run and rerurns emit correct facts`() {
        val processor: FactProcessor = mock()

        Facts.registerProcessor(processor)

        triggerAndAssertFacts(
            processor,
            Actions.GET to Values.SUCCESS_MISSING,
            Actions.WRITE to Values.SUCCESS_WRITE,
        )

        triggerAndAssertFacts(
            processor,
            Actions.GET to Values.SUCCESS_PRESENT,
        )

        triggerAndAssertFacts(
            processor,
            Actions.GET to Values.SUCCESS_PRESENT,
        )
    }

    @Config(sdk = [21])
    @Test
    fun `corrupt value returned`() {
        val processor: FactProcessor = mock()

        Facts.registerProcessor(processor)

        triggerAndAssertFacts(
            processor,
            Actions.GET to Values.SUCCESS_MISSING,
            Actions.WRITE to Values.SUCCESS_WRITE,
        )

        // Now, let's corrupt the value manually
        val securePrefs = SecureAbove22Preferences(
            testContext,
            SecurePrefsReliabilityExperiment.SECURE_PREFS_NAME,
        )
        securePrefs.putString(SecurePrefsReliabilityExperiment.PREF_KEY, "wrong test string")

        triggerAndAssertFacts(
            processor,
            Actions.GET to Values.CORRUPTED,
            Actions.RESET to Values.SUCCESS_RESET,
        )

        // ... and we should be reset now:
        triggerAndAssertFacts(
            processor,
            Actions.GET to Values.SUCCESS_MISSING,
            Actions.WRITE to Values.SUCCESS_WRITE,
        )
    }

    @Config(sdk = [21])
    @Test
    fun `lost value`() {
        val processor: FactProcessor = mock()

        Facts.registerProcessor(processor)

        triggerAndAssertFacts(
            processor,
            Actions.GET to Values.SUCCESS_MISSING,
            Actions.WRITE to Values.SUCCESS_WRITE,
        )

        // Now, let's corrupt the store manually
        val securePrefs = SecureAbove22Preferences(
            testContext,
            SecurePrefsReliabilityExperiment.SECURE_PREFS_NAME,
        )
        securePrefs.clear()

        // loss is detected:
        triggerAndAssertFacts(
            processor,
            Actions.GET to Values.LOST,
            Actions.RESET to Values.SUCCESS_RESET,
        )

        // we should be reset now:
        triggerAndAssertFacts(
            processor,
            Actions.GET to Values.SUCCESS_MISSING,
            Actions.WRITE to Values.SUCCESS_WRITE,
        )

        triggerAndAssertFacts(
            processor,
            Actions.GET to Values.SUCCESS_PRESENT,
        )
    }

    @Config(sdk = [21])
    @Test
    fun `value present unexpectedly`() {
        val processor: FactProcessor = mock()

        Facts.registerProcessor(processor)

        // First, let's add the correct value manually:
        val securePrefs = SecureAbove22Preferences(
            testContext,
            SecurePrefsReliabilityExperiment.SECURE_PREFS_NAME,
        )
        securePrefs.putString(SecurePrefsReliabilityExperiment.PREF_KEY, SecurePrefsReliabilityExperiment.PREF_VALUE)

        triggerAndAssertFacts(
            processor,
            Actions.GET to Values.PRESENT_UNEXPECTED,
            Actions.RESET to Values.SUCCESS_RESET,
        )

        // Let's try an incorrect value, as well:
        securePrefs.putString(SecurePrefsReliabilityExperiment.PREF_KEY, "bad string")

        triggerAndAssertFacts(
            processor,
            Actions.GET to Values.PRESENT_UNEXPECTED,
            Actions.RESET to Values.SUCCESS_RESET,
        )

        // subsequently, it's all good:
        triggerAndAssertFacts(
            processor,
            Actions.GET to Values.SUCCESS_MISSING,
            Actions.WRITE to Values.SUCCESS_WRITE,
        )

        triggerAndAssertFacts(
            processor,
            Actions.GET to Values.SUCCESS_PRESENT,
        )
    }

    @Test
    fun `initialization failure`() {
        // AndroidKeyStore isn't available in the test environment.
        // This test runs against our target sdk version, so the experiment code will attempt to init
        // the AndroidKeyStore, that won't be available.
        val processor: FactProcessor = mock()

        Facts.registerProcessor(processor)

        SecurePrefsReliabilityExperiment(testContext)()

        val captor = argumentCaptor<Fact>()
        Mockito.verify(processor).process(captor.capture())

        assertEquals(1, captor.allValues.size)
        assertExperimentFact(
            captor.allValues[0],
            Actions.GET,
            Values.FAIL,
            mapOf("javaClass" to "java.security.KeyStoreException"),
        )
    }

    private fun triggerAndAssertFacts(processor: FactProcessor, vararg factPairs: Pair<String, Values>) {
        with(argumentCaptor<Fact>()) {
            SecurePrefsReliabilityExperiment(testContext)()
            Mockito.verify(processor, times(factPairs.size)).process(this.capture())
            assertEquals(factPairs.size, this.allValues.size)
            factPairs.forEachIndexed { index, pair ->
                assertExperimentFact(this.allValues[index], pair.first, pair.second)
            }
        }
        reset(processor)
    }

    private fun assertExperimentFact(
        fact: Fact,
        item: String,
        value: Values,
        metadata: Map<String, Any>? = null,
    ) {
        assertEquals(Component.LIB_DATAPROTECT, fact.component)
        assertEquals(Action.IMPLEMENTATION_DETAIL, fact.action)
        assertEquals(item, fact.item)
        assertEquals("${value.v}", fact.value)
        assertEquals(metadata, fact.metadata)
    }
}
