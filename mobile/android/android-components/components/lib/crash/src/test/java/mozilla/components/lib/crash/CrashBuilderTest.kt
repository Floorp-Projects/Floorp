package mozilla.components.lib.crash

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.support.test.mock
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class CrashBuilderTest {

    private lateinit var runtimeTagProviders: MutableList<RuntimeTagProvider>
    private lateinit var objUnderTest: CrashBuilder

    @Before
    fun setup() {
        runtimeTagProviders = mutableListOf()
        objUnderTest = CrashBuilder(runtimeTagProviders)
    }


    @Test
    fun `building uncaught exception crash has throwable and breadcrumbs`() {
        val throwable = Throwable()
        val breadcrumbs = listOf(mock<Breadcrumb>(), mock<Breadcrumb>())

        val result = objUnderTest.uncaughtExceptionCrash(throwable, breadcrumbs)

        assert(result.throwable == throwable)
        assert(result.breadcrumbs == breadcrumbs)
    }

    @Test
    fun `building uncaught exception crash has empty breadcrumbs`() {
        val result = objUnderTest.uncaughtExceptionCrash(Throwable(), emptyList())

        assert(result.breadcrumbs == emptyList<Breadcrumb>())
    }

    @Test
    fun `building uncaught exception crash has correct runtime tags`() {
        runtimeTagProviders.addAll(
            listOf(
                object : RuntimeTagProvider {
                    override fun invoke(): Map<String, String> {
                        return mapOf("one" to "two", "three" to "four")
                    }
                },
                object : RuntimeTagProvider {
                    override fun invoke(): Map<String, String> {
                        return mapOf("five" to "six")
                    }
                },
            ),
        )

        assert(
            objUnderTest.uncaughtExceptionCrash(Throwable(), emptyList())
                .runtimeTags == mapOf("one" to "two", "three" to "four", "five" to "six"),
        )
    }

    @Test
    fun `building uncaught exception crash with no RuntimeTagProviders has no runtime tags`() {
        assert(
            objUnderTest.uncaughtExceptionCrash(Throwable(), emptyList())
                .runtimeTags.isEmpty(),
        )
    }

    @Test
    fun `building uncaught exception crash with empty RuntimeTagProviders has no runtime tags`() {
        runtimeTagProviders.add(
            object : RuntimeTagProvider {
                override fun invoke(): Map<String, String> = emptyMap()
            },
        )
        assert(
            objUnderTest.uncaughtExceptionCrash(Throwable(), emptyList())
                .runtimeTags.isEmpty(),
        )
    }
}
