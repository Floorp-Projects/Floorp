/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package plugins

import okhttp3.OkHttpClient
import okhttp3.Request
import okio.ByteString
import okio.Okio
import org.gradle.api.Plugin
import org.gradle.api.Project
import java.io.File
import java.util.TreeSet

/**
 * Gradle plugin to update the public suffix list used by the `lib-publicsuffixlist` component.
 *
 * Base on PublicSuffixListGenerator from OkHttp:
 * https://github.com/square/okhttp/blob/master/okhttp/src/test/java/okhttp3/internal/publicsuffix/PublicSuffixListGenerator.java
 */
class PublicSuffixListPlugin : Plugin<Project> {
    override fun apply(project: Project) {
        project.tasks.register("updatePSL") {
            doLast {
                val filename = project.projectDir.absolutePath + "/src/main/assets/publicsuffixes"
                updatePublicSuffixList(filename)
            }
        }
    }

    private fun updatePublicSuffixList(destination: String) {
        val list = fetchPublicSuffixList()
        writeListToDisk(destination, list)
    }

    private fun writeListToDisk(destination: String, data: PublicSuffixListData) {
        val fileSink = Okio.sink(File(destination))

        Okio.buffer(fileSink).use { sink ->
            sink.writeInt(data.totalRuleBytes)

            for (domain in data.sortedRules) {
                sink.write(domain).writeByte('\n'.toInt())
            }

            sink.writeInt(data.totalExceptionRuleBytes)

            for (domain in data.sortedExceptionRules) {
                sink.write(domain).writeByte('\n'.toInt())
            }
        }
    }

    private fun fetchPublicSuffixList(): PublicSuffixListData {
        val client = OkHttpClient.Builder().build()

        val request = Request.Builder()
            .url("https://publicsuffix.org/list/public_suffix_list.dat")
            .build()

        client.newCall(request).execute().use { response ->
            val source = response.body()!!.source()

            val data = PublicSuffixListData()

            while (!source.exhausted()) {
                val line = source.readUtf8LineStrict()

                if (line.trim { it <= ' ' }.isEmpty() || line.startsWith("//")) {
                    continue
                }

                if (line.contains(WILDCARD_CHAR)) {
                    assertWildcardRule(line)
                }

                var rule = ByteString.encodeUtf8(line)

                if (rule.startsWith(EXCEPTION_RULE_MARKER)) {
                    rule = rule.substring(1)
                    // We use '\n' for end of value.
                    data.totalExceptionRuleBytes += rule.size() + 1
                    data.sortedExceptionRules.add(rule)
                } else {
                    data.totalRuleBytes += rule.size() + 1 // We use '\n' for end of value.
                    data.sortedRules.add(rule)
                }
            }

            return data
        }
    }

    @Suppress("TooGenericExceptionThrown", "ThrowsCount")
    private fun assertWildcardRule(rule: String) {
        if (rule.indexOf(WILDCARD_CHAR) != 0) {
            throw RuntimeException("Wildcard is not not in leftmost position")
        }

        if (rule.indexOf(WILDCARD_CHAR, 1) != -1) {
            throw RuntimeException("Rule contains multiple wildcards")
        }

        if (rule.length == 1) {
            throw RuntimeException("Rule wildcards the first level")
        }
    }

    companion object {
        private const val WILDCARD_CHAR = "*"
        private val EXCEPTION_RULE_MARKER = ByteString.encodeUtf8("!")
    }
}

data class PublicSuffixListData(
    var totalRuleBytes: Int = 0,
    var totalExceptionRuleBytes: Int = 0,

    val sortedRules: TreeSet<ByteString> = TreeSet(),
    val sortedExceptionRules: TreeSet<ByteString> = TreeSet(),
)
