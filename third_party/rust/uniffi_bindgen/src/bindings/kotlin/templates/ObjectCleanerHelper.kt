
// The cleaner interface for Object finalization code to run.
// This is the entry point to any implementation that we're using.
//
// The cleaner registers objects and returns cleanables, so now we are
// defining a `UniffiCleaner` with a `UniffiClenaer.Cleanable` to abstract the
// different implmentations available at compile time.
interface UniffiCleaner {
    interface Cleanable {
        fun clean()
    }

    fun register(value: Any, cleanUpTask: Runnable): UniffiCleaner.Cleanable

    companion object
}

// The fallback Jna cleaner, which is available for both Android, and the JVM.
private class UniffiJnaCleaner : UniffiCleaner {
    private val cleaner = com.sun.jna.internal.Cleaner.getCleaner()

    override fun register(value: Any, cleanUpTask: Runnable): UniffiCleaner.Cleanable =
        UniffiJnaCleanable(cleaner.register(value, cleanUpTask))
}

private class UniffiJnaCleanable(
    private val cleanable: com.sun.jna.internal.Cleaner.Cleanable,
) : UniffiCleaner.Cleanable {
    override fun clean() = cleanable.clean()
}

// We decide at uniffi binding generation time whether we were
// using Android or not.
// There are further runtime checks to chose the correct implementation
// of the cleaner.
{% if config.android_cleaner() %}
{%-   include "ObjectCleanerHelperAndroid.kt" %}
{%- else %}
{%-   include "ObjectCleanerHelperJvm.kt" %}
{%- endif %}
