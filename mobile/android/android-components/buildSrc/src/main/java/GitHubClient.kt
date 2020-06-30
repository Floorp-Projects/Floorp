/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import java.net.HttpURLConnection
import java.net.URL
import java.nio.charset.StandardCharsets

class GitHubClient(token: String) {

    private val tokenHeader = "Authorization" to "Bearer $token"

    companion object {
        const val GITHUB_BASE_API = "https://api.github.com/repos"
    }

    fun createPullRequest(owner: String, repoName: String, bodyJson: String): Pair<Boolean, String> {
        val url = "$GITHUB_BASE_API/$owner/$repoName/pulls"

        return httpPOST(url, bodyJson, tokenHeader)
    }

    fun createIssue(owner: String, repoName: String, bodyJson: String): Pair<Boolean, String> {
        val url = "$GITHUB_BASE_API/$owner/$repoName/issues"

        return httpPOST(url, bodyJson, tokenHeader)
    }

    @Suppress("TooGenericExceptionCaught")
    private fun httpPOST(urlString: String, json: String, vararg headers: Pair<String, String>): Pair<Boolean, String> {
        val url = URL(urlString)
        val http = url.openConnection() as HttpURLConnection

        http.requestMethod = "POST"
        http.doOutput = true
        http.setRequestProperty("Content-Type", "application/json; charset=UTF-8")

        headers.forEach {
            http.setRequestProperty(it.first, it.second)
        }

        http.outputStream.use { os ->
            os.write(json.toByteArray(StandardCharsets.UTF_8))
        }

        var responseSuccessful = true
        val textResponse = try {
            http.inputStream.bufferedReader().readText()
        } catch (e: Exception) {
            responseSuccessful = false
            http.errorStream.bufferedReader().readText()
        }
        return responseSuccessful to textResponse
    }
}
