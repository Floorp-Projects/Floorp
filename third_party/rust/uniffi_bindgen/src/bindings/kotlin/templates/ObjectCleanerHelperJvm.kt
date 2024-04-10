private fun UniffiCleaner.Companion.create(): UniffiCleaner =
    try {
        // For safety's sake: if the library hasn't been run in android_cleaner = true
        // mode, but is being run on Android, then we still need to think about
        // Android API versions.
        // So we check if java.lang.ref.Cleaner is there, and use that…
        java.lang.Class.forName("java.lang.ref.Cleaner")
        JavaLangRefCleaner()
    } catch (e: ClassNotFoundException) {
        // … otherwise, fallback to the JNA cleaner.
        UniffiJnaCleaner()
    }

private class JavaLangRefCleaner : UniffiCleaner {
    val cleaner = java.lang.ref.Cleaner.create()

    override fun register(value: Any, cleanUpTask: Runnable): UniffiCleaner.Cleanable =
        JavaLangRefCleanable(cleaner.register(value, cleanUpTask))
}

private class JavaLangRefCleanable(
    val cleanable: java.lang.ref.Cleaner.Cleanable
) : UniffiCleaner.Cleanable {
    override fun clean() = cleanable.clean()
}
