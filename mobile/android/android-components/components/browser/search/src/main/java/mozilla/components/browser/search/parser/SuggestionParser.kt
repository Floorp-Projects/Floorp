package mozilla.components.browser.search.parser

import mozilla.components.browser.search.SearchEngine
import org.json.JSONArray
import org.json.JSONObject

private fun JSONArray.asSequence(): Sequence<Any> {
    return object : Sequence<Any> {

        override fun iterator() = object : Iterator<Any> {
            val it = (0 until this@asSequence.length()).iterator()

            override fun next(): Any {
                val i = it.next()
                return this@asSequence.get(i)
            }

            override fun hasNext() = it.hasNext()
        }
    }
}

typealias Parser = (String) -> List<String>

private fun fromArrayResult(resultsIndex: Int): Parser {
    return { input ->
        JSONArray(input)
                .getJSONArray(resultsIndex)
                .asSequence()
                .map { it as? String }
                .filterNotNull()
                .toList()
    }
}

private fun fromObjectResult(resultsKey: String): Parser {
    return { input ->
        JSONObject(input)
                .getJSONArray(resultsKey)
                .asSequence()
                .map { it as? String }
                .filterNotNull()
                .toList()
    }
}

private fun qwantParserBuilder(): Parser {
    return { input ->
        JSONObject(input)
                .getJSONObject("data")
                .getJSONArray("items")
                .asSequence()
                .map { it as? JSONObject }
                .map { it?.getString("value") }
                .filterNotNull()
                .toList()
    }
}

val defaultParser = fromArrayResult(1)
val azerdictParser = fromObjectResult("suggestions")
val daumParser = fromObjectResult("items")
val qwantParser = qwantParserBuilder()

fun selectParser(searchEngine: SearchEngine): Parser = when(searchEngine.name) {
    "Azerdict" -> azerdictParser
    "다음지도" -> daumParser
    "Qwant" -> qwantParser
    else -> defaultParser
}