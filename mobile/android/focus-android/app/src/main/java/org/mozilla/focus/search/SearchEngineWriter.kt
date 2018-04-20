/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search

import android.graphics.Bitmap
import android.util.Log
import org.mozilla.focus.utils.BitmapUtils
import org.w3c.dom.Document
import java.io.StringWriter
import javax.xml.parsers.DocumentBuilderFactory
import javax.xml.parsers.ParserConfigurationException
import javax.xml.transform.OutputKeys
import javax.xml.transform.TransformerConfigurationException
import javax.xml.transform.TransformerException
import javax.xml.transform.TransformerFactory
import javax.xml.transform.dom.DOMSource
import javax.xml.transform.stream.StreamResult

internal class SearchEngineWriter {
    companion object {
        private const val LOG_TAG = "SearchEngineWriter"

        fun buildSearchEngineXML(engineName: String, searchQuery: String, iconBitmap: Bitmap): String? {
            try {
                val document = DocumentBuilderFactory.newInstance().newDocumentBuilder().newDocument()
                val rootElement = document!!.createElement("OpenSearchDescription")
                rootElement.setAttribute("xmlns", "http://a9.com/-/spec/opensearch/1.1/")
                document.appendChild(rootElement)

                val shortNameElement = document.createElement("ShortName")
                shortNameElement.setTextContent(engineName)
                rootElement.appendChild(shortNameElement)

                val imageElement = document.createElement("Image")
                imageElement.setAttribute("width", "16")
                imageElement.setAttribute("height", "16")
                imageElement.setTextContent(BitmapUtils.getBase64EncodedDataUriFromBitmap(iconBitmap))
                rootElement.appendChild(imageElement)

                val descriptionElement = document.createElement("Description")
                descriptionElement.setTextContent(engineName)
                rootElement.appendChild(descriptionElement)

                val urlElement = document.createElement("Url")
                urlElement.setAttribute("type", "text/html")

                // Simple implementation that assumes "%s" terminator from UrlUtils.isValidSearchEngineQueryUrl
                val templateSearchString = searchQuery.substring(0, searchQuery.length - 2) + "{searchTerms}"
                urlElement.setAttribute("template", templateSearchString)
                rootElement.appendChild(urlElement)

                return xmlToString(document)
            } catch (e: ParserConfigurationException) {
                Log.e(LOG_TAG, "Couldn't create new Document for building search engine XML", e)
                return null
            }
        }

        private fun xmlToString(doc: Document): String? {
            val writer = StringWriter()
            try {
                val tf = TransformerFactory.newInstance().newTransformer()
                tf.setOutputProperty(OutputKeys.ENCODING, "UTF-8")
                tf.transform(DOMSource(doc), StreamResult(writer))
            } catch (e: TransformerConfigurationException) {
                return null
            } catch (e: TransformerException) {
                return null
            }

            return writer.toString()
        }
    }
}
