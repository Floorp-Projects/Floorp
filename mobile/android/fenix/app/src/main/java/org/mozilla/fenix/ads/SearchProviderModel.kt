package org.mozilla.fenix.ads

data class SearchProviderModel(
    val name: String,
    val regexp: String,
    val queryParam: String,
    val codeParam: String = "",
    val codePrefixes: List<String> = ArrayList(),
    val followOnParams: List<String> = ArrayList(),
    val extraAdServersRegexps: List<String> = ArrayList(),
    val followOnCookies: List<SearchProviderCookie> = ArrayList()
)
