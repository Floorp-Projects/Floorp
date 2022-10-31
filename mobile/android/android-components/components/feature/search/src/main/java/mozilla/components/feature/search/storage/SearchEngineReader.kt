/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.storage

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import android.util.AtomicFile
import android.util.Base64
import mozilla.components.browser.state.search.SearchEngine
import org.xmlpull.v1.XmlPullParser
import org.xmlpull.v1.XmlPullParserException
import org.xmlpull.v1.XmlPullParserFactory
import java.io.IOException
import java.io.InputStream
import java.io.InputStreamReader
import java.nio.charset.StandardCharsets

internal const val URL_TYPE_SUGGEST_JSON = "application/x-suggestions+json"
internal const val URL_TYPE_SEARCH_HTML = "text/html"
internal const val URL_REL_MOBILE = "mobile"
internal const val IMAGE_URI_PREFIX = "data:image/png;base64,"

/**
 * A simple XML reader for search engine plugins.
 *
 * @param type the [SearchEngine.Type] that the read [SearchEngine]s will get assigned.
 */
internal class SearchEngineReader(
    private val type: SearchEngine.Type,
) {
    private class SearchEngineBuilder(
        private val type: SearchEngine.Type,
        private val identifier: String,
    ) {
        var resultsUrls: MutableList<String> = mutableListOf()
        var suggestUrl: String? = null
        var name: String? = null
        var icon: Bitmap? = null

        fun toSearchEngine() = SearchEngine(
            id = identifier,
            name = name!!,
            icon = icon!!,
            type = type,
            resultUrls = resultsUrls,
            suggestUrl = suggestUrl,
        )
    }

    /**
     * Loads [SearchEngine] from a provided [file]
     */
    fun loadFile(identifier: String, file: AtomicFile): SearchEngine {
        return loadStream(identifier, file.openRead())
    }

    /**
     * Loads a <code>SearchEngine</code> from the given <code>stream</code> and assigns it the given
     * <code>identifier</code>.
     */
    @Throws(IOException::class, XmlPullParserException::class)
    fun loadStream(identifier: String, stream: InputStream): SearchEngine {
        val builder = SearchEngineBuilder(type, identifier)

        val parser = XmlPullParserFactory.newInstance().newPullParser()
        parser.setInput(InputStreamReader(stream, StandardCharsets.UTF_8))
        parser.next()

        readSearchPlugin(parser, builder)

        return builder.toSearchEngine()
    }

    @Throws(XmlPullParserException::class, IOException::class)
    @Suppress("ComplexMethod")
    private fun readSearchPlugin(parser: XmlPullParser, builder: SearchEngineBuilder) {
        if (XmlPullParser.START_TAG != parser.eventType) {
            throw XmlPullParserException("Expected start tag: " + parser.positionDescription)
        }

        val name = parser.name
        if ("SearchPlugin" != name && "OpenSearchDescription" != name) {
            throw XmlPullParserException(
                "Expected <SearchPlugin> or <OpenSearchDescription> as root tag: ${parser.positionDescription}",
            )
        }

        while (parser.next() != XmlPullParser.END_TAG) {
            if (parser.eventType != XmlPullParser.START_TAG) {
                continue
            }

            when (parser.name) {
                "ShortName" -> readShortName(parser, builder)
                "Url" -> readUrl(parser, builder)
                "Image" -> readImage(parser, builder)
                else -> skip(parser)
            }
        }
    }

    @Throws(XmlPullParserException::class, IOException::class)
    private fun readUrl(parser: XmlPullParser, builder: SearchEngineBuilder) {
        parser.require(XmlPullParser.START_TAG, null, "Url")

        val type = parser.getAttributeValue(null, "type")
        val template = parser.getAttributeValue(null, "template")
        val rel = parser.getAttributeValue(null, "rel")

        val url = readUri(parser, template).toString()

        if (type == URL_TYPE_SEARCH_HTML) {
            // Prefer mobile URIs.
            if (rel != null && rel == URL_REL_MOBILE) {
                builder.resultsUrls.add(0, url)
            } else {
                builder.resultsUrls.add(url)
            }
        } else if (type == URL_TYPE_SUGGEST_JSON) {
            builder.suggestUrl = url
        }
    }

    @Throws(XmlPullParserException::class, IOException::class)
    private fun readUri(parser: XmlPullParser, template: String): Uri {
        var uri = Uri.parse(template)

        while (parser.next() != XmlPullParser.END_TAG) {
            if (parser.eventType != XmlPullParser.START_TAG) {
                continue
            }

            if (parser.name == "Param") {
                val name = parser.getAttributeValue(null, "name")
                val value = parser.getAttributeValue(null, "value")
                uri = uri.buildUpon().appendQueryParameter(name, value).build()
                parser.nextTag()
            } else {
                skip(parser)
            }
        }

        return uri
    }

    @Throws(XmlPullParserException::class, IOException::class)
    private fun skip(parser: XmlPullParser) {
        if (parser.eventType != XmlPullParser.START_TAG) {
            throw IllegalStateException()
        }
        var depth = 1
        while (depth != 0) {
            when (parser.next()) {
                XmlPullParser.END_TAG -> depth--
                XmlPullParser.START_TAG -> depth++
                // else: Do nothing - we're skipping content
            }
        }
    }

    @Throws(IOException::class, XmlPullParserException::class)
    private fun readShortName(parser: XmlPullParser, builder: SearchEngineBuilder) {
        parser.require(XmlPullParser.START_TAG, null, "ShortName")
        if (parser.next() == XmlPullParser.TEXT) {
            builder.name = parser.text
            parser.nextTag()
        }
    }

    @Throws(IOException::class, XmlPullParserException::class)
    private fun readImage(parser: XmlPullParser, builder: SearchEngineBuilder) {
        parser.require(XmlPullParser.START_TAG, null, "Image")

        if (parser.next() != XmlPullParser.TEXT) {
            return
        }

        val uri = parser.text
        if (!uri.startsWith(IMAGE_URI_PREFIX)) {
            return
        }

        val raw = Base64.decode(uri.substring(IMAGE_URI_PREFIX.length), Base64.DEFAULT)

        builder.icon = BitmapFactory.decodeByteArray(raw, 0, raw.size)

        parser.nextTag()
    }
}
