/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.appservices.remotesettings

import mozilla.appservices.httpconfig.RustHttpConfig
import mozilla.appservices.remotesettings.RemoteSettings
import mozilla.appservices.remotesettings.RemoteSettingsConfig
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Headers
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import java.io.File
import java.io.InputStream

@RunWith(RobolectricTestRunner::class)
class RemoteSettingsTest {

    @Rule @JvmField
    val tempDir = TemporaryFolder()

    private var fakeUrl = ""
    private var fakeStatus = 200
    private var fakeHeaders: Headers = MutableHeaders("etag" to "1000")
    private var fakeBodyStream = "".byteInputStream()
    private var fakeContentType = ""
    private var fakeBody = Response.Body(fakeBodyStream, fakeContentType)
    private var doFetch: (Request) -> Response = {
        Response(fakeUrl, fakeStatus, fakeHeaders, fakeBody)
    }

    @Before
    fun setup() {
        RustHttpConfig.setClient(
            lazyOf(object : Client() {
                override fun fetch(request: Request): Response = doFetch(request)
            }),
        )
    }

    @Test
    fun `test setting up, fetching records, and attachment downloading`() {
        val config = RemoteSettingsConfig(
            serverUrl = "http://localhost:8888",
            bucketName = "the-bucket",
            collectionName = "the-collection",
        )

        // Setup the client
        val client = RemoteSettings(config)

        // Fetch the records
        setupRecordResponse(config, responseBodyStream = recordJson.byteInputStream())
        val response = client.getRecords()
        val records = response.records
        assertEquals(records[0].fields.getString("title"), recordTitle)

        // Download an attachment
        val attachmentLocation = records[0].attachment!!.location
        val localAttachmentPath = "${tempDir.root}/path.jpg"
        setupAttachmentResponses(config, attachmentLocation)
        client.downloadAttachmentToPath(attachmentLocation, localAttachmentPath)
        val downloadedFile = File(localAttachmentPath)
        assertTrue(downloadedFile.exists())
        assertEquals(csv, downloadedFile.readText())
    }

    private fun setupAttachmentResponses(
        config: RemoteSettingsConfig,
        attachmentLocation: String,
    ) {
        val topUrl = config.serverUrl
        val attachmentUrl = "${config.serverUrl}/attachments/$attachmentLocation"
        doFetch = { req ->
            when (req.url) {
                "$topUrl/" -> {
                    Response(
                        url = req.url,
                        status = 200,
                        headers = fakeHeaders,
                        body = Response.Body(
                            stream = attachmentsInfoJson(topUrl!!).byteInputStream(),
                            null,
                        ),
                    )
                }
                attachmentUrl -> {
                    Response(
                        url = attachmentUrl,
                        status = 200,
                        headers = fakeHeaders,
                        body = Response.Body(
                            stream = csv.byteInputStream(),
                            null,
                        ),
                    )
                }
                else -> error("unexpected url")
            }
        }
    }
    private fun setupRecordResponse(
        config: RemoteSettingsConfig,
        responseBodyStream: InputStream = recordJson.byteInputStream(),
    ) {
        fakeUrl = "${config.serverUrl}/v1/buckets/${config.bucketName}/collections/${config.collectionName}/records"
        fakeBody = Response.Body(responseBodyStream, null)
    }

    private fun attachmentsInfoJson(baseUrl: String) = """
      {
        "capabilities": {
          "admin": {
            "description": "Serves the admin console.",
            "url": "https://github.com/Kinto/kinto-admin/",
            "version": "2.0.0"
          },
          "attachments": {
            "description": "Add file attachments to records",
            "url": "https://github.com/Kinto/kinto-attachment/",
            "version": "6.3.1",
            "base_url": "$baseUrl/attachments/"
          }
        }
      }
    """.trimIndent()

    private val recordTitle = "with-txt-attachment"
    private val recordJson = """
    {
      "data": [
        {
          "title": "$recordTitle",
          "content": "content",
          "attachment": {
            "filename": "text-attachment.csv",
            "location": "the-bucket/the-collection/d3a5eccc-f0ca-42c3-b0bb-c0d4408c21c9.jpg",
            "hash": "2cbd593f3fd5f1585f92265433a6696a863bc98726f03e7222135ff0d8e83543",
            "mimetype": "text/csv",
            "size": 1374325
          },
          "schema": 1677694447771,
          "id": "7403c6f9-79be-4e0c-a37a-8f2b5bd7ad58",
          "last_modified": 1677694455368
        }
      ]
    }
    """.trimIndent()

    private val csv = """
        John,Doe,120 jefferson st.,Riverside, NJ, 08075
        Jack,McGinnis,220 hobo Av.,Phila, PA,09119
        "John ""Da Man""${'"'},Repici,120 Jefferson St.,Riverside, NJ,08075
        Stephen,Tyler,"7452 Terrace ""At the Plaza"" road",SomeTown,SD, 91234
        ,Blankman,,SomeTown, SD, 00298
        "Joan ""the bone"", Anne",Jet,"9th, at Terrace plc",Desert City,CO,00123
    """.trimIndent()
}
