package org.mozilla.fenix.ads

data class SearchProviderCookie(
    val extraCodeParam: String,
    val extraCodePrefixes: List<String>,
    val host: String,
    val name: String,
    val codeParam: String,
    val codePrefixes: List<String>
)
