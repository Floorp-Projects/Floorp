package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.hamcrest.Matchers.* // ktlint-disable no-wildcard-imports
import org.json.JSONObject
import org.junit.Test
import org.junit.runner.RunWith
import java.io.BufferedReader
import java.io.ByteArrayInputStream
import java.io.InputStreamReader
import java.util.zip.GZIPInputStream

@RunWith(AndroidJUnit4::class)
class ProfilerControllerTest : BaseSessionTest() {

    @Test
    fun startAndStopProfiler() {
        sessionRule.runtime.profilerController.startProfiler(arrayOf<String>(), arrayOf<String>())
        val result = sessionRule.runtime.profilerController.stopProfiler()
        val byteArray = sessionRule.waitForResult(result)
        val head = (byteArray[0].toInt() and 0xff) or (byteArray[1].toInt() shl 8 and 0xff00)
        assertThat(
            "Header of byte array should be the same as the GZIP one",
            head,
            equalTo(GZIPInputStream.GZIP_MAGIC),
        )

        val profileString = StringBuilder()
        val gzipInputStream = GZIPInputStream(ByteArrayInputStream(byteArray))
        val bufferedReader = BufferedReader(InputStreamReader(gzipInputStream))

        var line = bufferedReader.readLine()
        while (line != null) {
            profileString.append(line)
            line = bufferedReader.readLine()
        }

        val json = JSONObject(profileString.toString())
        assertThat(
            "profile JSON object must not be empty",
            json.length(),
            greaterThan(0),
        )
    }
}
