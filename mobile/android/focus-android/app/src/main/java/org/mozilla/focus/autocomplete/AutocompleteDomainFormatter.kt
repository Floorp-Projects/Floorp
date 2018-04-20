package org.mozilla.focus.autocomplete

object AutocompleteDomainFormatter {
    private const val HOST_INDEX = 3
    private val urlMatcher = Regex("""(https?://)?(www.)?(.+)?""")

    fun format(url: String): String {
        val result = urlMatcher.find(url)

        return result?.let {
            it.groups[HOST_INDEX]?.value
        } ?: url
    }
}
