package org.mozilla.geckoview.test.util

import android.content.Context
import android.content.res.AssetManager
import android.os.SystemClock
import android.webkit.MimeTypeMap
import com.koushikdutta.async.ByteBufferList
import com.koushikdutta.async.http.server.AsyncHttpServer
import com.koushikdutta.async.http.server.AsyncHttpServerRequest
import com.koushikdutta.async.http.server.AsyncHttpServerResponse
import com.koushikdutta.async.http.server.HttpServerRequestCallback
import com.koushikdutta.async.util.TaggedList
import org.json.JSONObject
import java.io.FileNotFoundException
import java.math.BigInteger
import java.security.MessageDigest
import java.util.* // ktlint-disable no-wildcard-imports

class TestServer @JvmOverloads constructor(
    context: Context,
    private val customHeaders: Map<String, String>? = null,
    private val responseModifiers: Map<String, ResponseModifier>? = null,
) {
    private val server = AsyncHttpServer()
    private val assets: AssetManager
    private val stallingResponses = Vector<AsyncHttpServerResponse>()

    init {
        assets = context.resources.assets

        val anything = { request: AsyncHttpServerRequest, response: AsyncHttpServerResponse ->
            val obj = JSONObject()

            obj.put("method", request.method)

            val headers = JSONObject()
            for (key in request.headers.multiMap.keys) {
                val values = request.headers.multiMap.get(key) as TaggedList<String>
                headers.put(values.tag(), values.joinToString(", "))
            }

            obj.put("headers", headers)

            if (request.method == "POST") {
                obj.put("data", request.getBody())
            }

            response.send(obj)
        }

        server.post("/anything", anything)
        server.get("/anything", anything)

        val assetsCallback = HttpServerRequestCallback { request, response ->
            try {
                val mimeType = MimeTypeMap.getSingleton()
                    .getMimeTypeFromExtension(MimeTypeMap.getFileExtensionFromUrl(request.path))
                val name = request.path.substring("/assets/".count())
                val asset = assets.open(name).readBytes()

                customHeaders?.forEach { (header, value) ->
                    response.headers.set(header, value)
                }

                responseModifiers?.get(request.path)?.let { modifier ->
                    response.send(mimeType, modifier.transformResponse(asset.decodeToString()))
                    return@HttpServerRequestCallback
                }

                response.send(mimeType, asset)
            } catch (e: FileNotFoundException) {
                response.code(404)
                response.end()
            }
        }

        server.get("/assets/.*", assetsCallback)
        server.post("/assets/.*", assetsCallback)

        server.get("/status/.*") { request, response ->
            val statusCode = request.path.substring("/status/".count()).toInt()
            response.code(statusCode)
            response.end()
        }

        server.get("/redirect-to.*") { request, response ->
            response.redirect(request.query.getString("url"))
        }

        server.get("/redirect/.*") { request, response ->
            val count = request.path.split('/').last().toInt() - 1
            if (count > 0) {
                response.redirect("/redirect/$count")
            }

            response.end()
        }

        server.get("/basic-auth/.*") { _, response ->
            response.code(401)
            response.headers.set("WWW-Authenticate", "Basic realm=\"Fake Realm\"")
            response.end()
        }

        server.get("/cookies") { request, response ->
            val cookiesObj = JSONObject()

            request.headers.get("cookie")?.split(";")?.forEach {
                val parts = it.trim().split('=')
                cookiesObj.put(parts[0], parts[1])
            }

            val obj = JSONObject()
            obj.put("cookies", cookiesObj)
            response.send(obj)
        }

        server.get("/cookies/set/.*") { request, response ->
            val parts = request.path.substring("/cookies/set/".count()).split('/')

            response.headers.set("Set-Cookie", "${parts[0]}=${parts[1]}; Path=/")
            response.headers.set("Location", "/cookies")
            response.code(302)
            response.end()
        }

        server.get("/bytes/.*") { request, response ->
            val count = request.path.split("/").last().toInt()
            val random = Random(System.currentTimeMillis())
            val payload = ByteArray(count)
            random.nextBytes(payload)

            val digest = MessageDigest.getInstance("SHA-256").digest(payload)
            response.headers.set("X-SHA-256", String.format("%064x", BigInteger(1, digest)))
            response.send("application/octet-stream", payload)
        }

        server.get("/trickle/.*") { request, response ->
            val count = request.path.split("/").last().toInt()

            response.setContentType("application/octet-stream")
            response.headers.set("Content-Length", "$count")
            response.writeHead()

            val payload = byteArrayOf(1)
            for (i in 1..count) {
                response.write(ByteBufferList(payload))
                SystemClock.sleep(250)
            }

            response.end()
        }

        server.get("/stall/.*") { _, response ->
            // keep trickling data for a long time (until we are stopped)
            stallingResponses.add(response)

            val count = 100
            response.setContentType("InstallException")
            response.headers.set("Content-Length", "$count")
            response.writeHead()

            val payload = byteArrayOf(1)
            for (i in 1..count - 1) {
                response.write(ByteBufferList(payload))
                SystemClock.sleep(250)
            }

            stallingResponses.remove(response)
            response.end()
        }
    }

    fun start(port: Int) {
        server.listen(port)
    }

    fun stop() {
        for (response in stallingResponses) {
            response.end()
        }
        server.stop()
    }

    fun interface ResponseModifier {
        abstract fun transformResponse(response: String): String
    }
}
