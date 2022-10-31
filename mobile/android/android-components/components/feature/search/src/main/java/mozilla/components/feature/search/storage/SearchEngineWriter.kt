/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.storage

import android.graphics.Bitmap
import android.util.AtomicFile
import android.util.Base64
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.search.SearchEngine
import org.w3c.dom.DOMException
import org.w3c.dom.Document
import java.io.ByteArrayOutputStream
import java.io.File
import javax.xml.parsers.DocumentBuilderFactory
import javax.xml.parsers.ParserConfigurationException
import javax.xml.transform.TransformerConfigurationException
import javax.xml.transform.TransformerException
import javax.xml.transform.TransformerFactory
import javax.xml.transform.dom.DOMSource
import javax.xml.transform.stream.StreamResult

/**
 * A simple XML writer for search engine plugins.
 */
internal class SearchEngineWriter {
    /**
     * Builds and save the XML document of [SearchEngine] to the provided [File].
     *
     * @param searchEngine the search engine to build XML with.
     * @param file the file instance to save the search engine XML.
     * @param document the document instance to build search engine XML with.
     * @return true if the XML is built and saved successfully, false otherwise.
     */
    fun saveSearchEngineXML(
        searchEngine: SearchEngine,
        file: AtomicFile,
        document: Document = DocumentBuilderFactory.newInstance().newDocumentBuilder().newDocument(),
    ): Boolean {
        return try {
            buildSearchEngineXML(searchEngine, document)
            saveXMLDocumentToFile(document, file)
            true
        } catch (e: ParserConfigurationException) {
            false
        } catch (e: DOMException) {
            false
        } catch (e: TransformerConfigurationException) {
            false
        } catch (e: TransformerException) {
            false
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @Throws(ParserConfigurationException::class, DOMException::class)
    internal fun buildSearchEngineXML(
        searchEngine: SearchEngine,
        xmlDocument: Document,
    ) {
        val rootElement = xmlDocument.createElement("OpenSearchDescription")
        rootElement.setAttribute("xmlns", "http://a9.com/-/spec/opensearch/1.1/")
        rootElement.setAttribute("xmlns:moz", "http://www.mozilla.org/2006/browser/search/")
        xmlDocument.appendChild(rootElement)

        val shortNameElement = xmlDocument.createElement("ShortName")
        shortNameElement.textContent = searchEngine.name
        rootElement.appendChild(shortNameElement)

        val descriptionElement = xmlDocument.createElement("Description")
        descriptionElement.textContent = searchEngine.name
        rootElement.appendChild(descriptionElement)

        val imageElement = xmlDocument.createElement("Image")
        imageElement.setAttribute("width", "16")
        imageElement.setAttribute("height", "16")
        imageElement.textContent = searchEngine.icon.toBase64()
        rootElement.appendChild(imageElement)

        searchEngine.resultUrls.forEach { url ->
            val urlElement = xmlDocument.createElement("Url")
            urlElement.setAttribute("type", URL_TYPE_SEARCH_HTML)
            urlElement.setAttribute("template", url)
            rootElement.appendChild(urlElement)
        }

        searchEngine.suggestUrl?.let { url ->
            val urlElement = xmlDocument.createElement("Url")
            urlElement.setAttribute("type", URL_TYPE_SUGGEST_JSON)
            val templateSearchString = url.replace("%s", "{searchTerms}")
            urlElement.setAttribute("template", templateSearchString)
            rootElement.appendChild(urlElement)
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @Throws(TransformerConfigurationException::class, TransformerException::class)
    internal fun saveXMLDocumentToFile(doc: Document, file: AtomicFile) =
        TransformerFactory.newInstance().newTransformer().transform(DOMSource(doc), StreamResult(file.baseFile))
}

private const val BITMAP_COMPRESS_QUALITY = 100
private fun Bitmap.toBase64(): String {
    val stream = ByteArrayOutputStream()
    compress(Bitmap.CompressFormat.PNG, BITMAP_COMPRESS_QUALITY, stream)
    val encodedImage = Base64.encodeToString(stream.toByteArray(), Base64.DEFAULT)
    return "data:image/png;base64,$encodedImage"
}
