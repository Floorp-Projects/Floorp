package mozilla.components.browser.search.parser

import org.json.JSONArray

fun JSONArray.asSequence(): Sequence<Any> {
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
val googleParser: Parser = { input ->
    JSONArray(input)
            .getJSONArray(1)
            .asSequence()
            .map { it as? String }
            .filterNotNull()
            .toList()
}