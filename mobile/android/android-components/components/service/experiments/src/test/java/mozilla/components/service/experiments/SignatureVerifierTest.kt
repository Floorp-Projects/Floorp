/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import okhttp3.mockwebserver.Dispatcher
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import okhttp3.mockwebserver.RecordedRequest
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import java.util.Calendar
import java.util.Date

@RunWith(AndroidJUnit4::class)
class SignatureVerifierTest {
    private lateinit var server: MockWebServer

    @Before
    fun setUp() {
        server = MockWebServer()
    }

    @After
    fun tearDown() {
        server.shutdown()
    }

    private fun defaultDate(): Date {
        val calendar = Calendar.getInstance()
        calendar.set(Calendar.YEAR, 2018)
        calendar.set(Calendar.MONTH, 6)
        calendar.set(Calendar.DAY_OF_MONTH, 13)
        return calendar.time
    }

    @Test(expected = ExperimentDownloadException::class)
    fun validSignatureOneCertificate() {
        val url = server.url("/").url().toString()
        val metadataBody = """
            {
              "data": {
                "signature": {
                  "x5u": "$url",
                  "signature": "kRhyWZdLyjligYHSFhzhbyzUXBoUwoTPvyt9V0e-E7LKGgUYF2MVfqpA2zfIEDdqrImcMABVGHLUx9Nk614zciRBQ-gyaKA5SL2pPdZvoQXk_LLsPhEBgG4VDnxG4SBL"
                }
              }
            }
        """.trimIndent()
        val certChainBody = "-----BEGIN CERTIFICATE-----\n" +
            "MIIGYTCCBEmgAwIBAgIBATANBgkqhkiG9w0BAQwFADB9MQswCQYDVQQGEwJVUzEc\n" +
            "MBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEvMC0GA1UECxMmTW96aWxsYSBB\n" +
            "TU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2UxHzAdBgNVBAMTFnJvb3QtY2Et\n" +
            "cHJvZHVjdGlvbi1hbW8wHhcNMTUwMzE3MjI1MzU3WhcNMjUwMzE0MjI1MzU3WjB9\n" +
            "MQswCQYDVQQGEwJVUzEcMBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEvMC0G\n" +
            "A1UECxMmTW96aWxsYSBBTU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2UxHzAd\n" +
            "BgNVBAMTFnJvb3QtY2EtcHJvZHVjdGlvbi1hbW8wggIgMA0GCSqGSIb3DQEBAQUA\n" +
            "A4ICDQAwggIIAoICAQC0u2HXXbrwy36+MPeKf5jgoASMfMNz7mJWBecJgvlTf4hH\n" +
            "JbLzMPsIUauzI9GEpLfHdZ6wzSyFOb4AM+D1mxAWhuZJ3MDAJOf3B1Rs6QorHrl8\n" +
            "qqlNtPGqepnpNJcLo7JsSqqE3NUm72MgqIHRgTRsqUs+7LIPGe7262U+N/T0LPYV\n" +
            "Le4rZ2RDHoaZhYY7a9+49mHOI/g2YFB+9yZjE+XdplT2kBgA4P8db7i7I0tIi4b0\n" +
            "B0N6y9MhL+CRZJyxdFe2wBykJX14LsheKsM1azHjZO56SKNrW8VAJTLkpRxCmsiT\n" +
            "r08fnPyDKmaeZ0BtsugicdipcZpXriIGmsZbI12q5yuwjSELdkDV6Uajo2n+2ws5\n" +
            "uXrP342X71WiWhC/dF5dz1LKtjBdmUkxaQMOP/uhtXEKBrZo1ounDRQx1j7+SkQ4\n" +
            "BEwjB3SEtr7XDWGOcOIkoJZWPACfBLC3PJCBWjTAyBlud0C5n3Cy9regAAnOIqI1\n" +
            "t16GU2laRh7elJ7gPRNgQgwLXeZcFxw6wvyiEcmCjOEQ6PM8UQjthOsKlszMhlKw\n" +
            "vjyOGDoztkqSBy/v+Asx7OW2Q7rlVfKarL0mREZdSMfoy3zTgtMVCM0vhNl6zcvf\n" +
            "5HNNopoEdg5yuXo2chZ1p1J+q86b0G5yJRMeT2+iOVY2EQ37tHrqUURncCy4uwIB\n" +
            "A6OB7TCB6jAMBgNVHRMEBTADAQH/MA4GA1UdDwEB/wQEAwIBBjAWBgNVHSUBAf8E\n" +
            "DDAKBggrBgEFBQcDAzCBkgYDVR0jBIGKMIGHoYGBpH8wfTELMAkGA1UEBhMCVVMx\n" +
            "HDAaBgNVBAoTE01vemlsbGEgQ29ycG9yYXRpb24xLzAtBgNVBAsTJk1vemlsbGEg\n" +
            "QU1PIFByb2R1Y3Rpb24gU2lnbmluZyBTZXJ2aWNlMR8wHQYDVQQDExZyb290LWNh\n" +
            "LXByb2R1Y3Rpb24tYW1vggEBMB0GA1UdDgQWBBSzvOpYdKvhbngqsqucIx6oYyyX\n" +
            "tzANBgkqhkiG9w0BAQwFAAOCAgEAaNSRYAaECAePQFyfk12kl8UPLh8hBNidP2H6\n" +
            "KT6O0vCVBjxmMrwr8Aqz6NL+TgdPmGRPDDLPDpDJTdWzdj7khAjxqWYhutACTew5\n" +
            "eWEaAzyErbKQl+duKvtThhV2p6F6YHJ2vutu4KIciOMKB8dslIqIQr90IX2Usljq\n" +
            "8Ttdyf+GhUmazqLtoB0GOuESEqT4unX6X7vSGu1oLV20t7t5eCnMMYD67ZBn0YIU\n" +
            "/cm/+pan66hHrja+NeDGF8wabJxdqKItCS3p3GN1zUGuJKrLykxqbOp/21byAGog\n" +
            "Z1amhz6NHUcfE6jki7sM7LHjPostU5ZWs3PEfVVgha9fZUhOrIDsyXEpCWVa3481\n" +
            "LlAq3GiUMKZ5DVRh9/Nvm4NwrTfB3QkQQJCwfXvO9pwnPKtISYkZUqhEqvXk5nBg\n" +
            "QCkDSLDjXTx39naBBGIVIqBtKKuVTla9enngdq692xX/CgO6QJVrwpqdGjebj5P8\n" +
            "5fNZPABzTezG3Uls5Vp+4iIWVAEDkK23cUj3c/HhE+Oo7kxfUeu5Y1ZV3qr61+6t\n" +
            "ZARKjbu1TuYQHf0fs+GwID8zeLc2zJL7UzcHFwwQ6Nda9OJN4uPAuC/BKaIpxCLL\n" +
            "26b24/tRam4SJjqpiq20lynhUrmTtt6hbG3E1Hpy3bmkt2DYnuMFwEx2gfXNcnbT\n" +
            "wNuvFqc=\n" +
            "-----END CERTIFICATE-----"
        testSignature(metadataBody, certChainBody, false)
    }

    /*
     * Temporarily disabled.
     * Renable this per:
    @Test
    fun validSignatureCorrect() {
        val url = server.url("/").url().toString()
        val metadataBody = """
            {
              "data": {
                "signature": {
                  "x5u": "$url",
                  "signature": "kRhyWZdLyjligYHSFhzhbyzUXBoUwoTPvyt9V0e-E7LKGgUYF2MVfqpA2zfIEDdqrImcMABVGHLUx9Nk614zciRBQ-gyaKA5SL2pPdZvoQXk_LLsPhEBgG4VDnxG4SBL"
                }
              }
            }
        """.trimIndent()
        val certChainBody = "-----BEGIN CERTIFICATE-----\n" +
            "MIIEpTCCBCygAwIBAgIEAQAAKTAKBggqhkjOPQQDAzCBpjELMAkGA1UEBhMCVVMx\n" +
            "HDAaBgNVBAoTE01vemlsbGEgQ29ycG9yYXRpb24xLzAtBgNVBAsTJk1vemlsbGEg\n" +
            "QU1PIFByb2R1Y3Rpb24gU2lnbmluZyBTZXJ2aWNlMSUwIwYDVQQDExxDb250ZW50\n" +
            "IFNpZ25pbmcgSW50ZXJtZWRpYXRlMSEwHwYJKoZIhvcNAQkBFhJmb3hzZWNAbW96\n" +
            "aWxsYS5jb20wIhgPMjAxODAyMTgwMDAwMDBaGA8yMDE4MDYxODAwMDAwMFowgbEx\n" +
            "CzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpDYWxpZm9ybmlhMRwwGgYDVQQKExNNb3pp\n" +
            "bGxhIENvcnBvcmF0aW9uMRcwFQYDVQQLEw5DbG91ZCBTZXJ2aWNlczExMC8GA1UE\n" +
            "AxMoZmVubmVjLWRsYy5jb250ZW50LXNpZ25hdHVyZS5tb3ppbGxhLm9yZzEjMCEG\n" +
            "CSqGSIb3DQEJARYUc2VjdXJpdHlAbW96aWxsYS5vcmcwdjAQBgcqhkjOPQIBBgUr\n" +
            "gQQAIgNiAATpKfWqAyDsh2ISzBycb8Y7JqLygByKI9vI2WZ2VWaGTYfmB1tQ8PFj\n" +
            "vZrtDqZeO8Dhs2KiMrvs/uoziM2zselYQhd0mz5z3dMui6BD6SPKB83K7xn2r2mW\n" +
            "plAZxuwnPKujggIYMIICFDAdBgNVHQ4EFgQUTp9+0KSp+vkzbEcXAENBCIyI9eow\n" +
            "gaoGA1UdIwSBojCBn4AUiHVymVvwUPJguD2xCZYej3l5nu6hgYGkfzB9MQswCQYD\n" +
            "VQQGEwJVUzEcMBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEvMC0GA1UECxMm\n" +
            "TW96aWxsYSBBTU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2UxHzAdBgNVBAMT\n" +
            "FnJvb3QtY2EtcHJvZHVjdGlvbi1hbW+CAxAABjAMBgNVHRMBAf8EAjAAMA4GA1Ud\n" +
            "DwEB/wQEAwIHgDAWBgNVHSUBAf8EDDAKBggrBgEFBQcDAzBFBgNVHR8EPjA8MDqg\n" +
            "OKA2hjRodHRwczovL2NvbnRlbnQtc2lnbmF0dXJlLmNkbi5tb3ppbGxhLm5ldC9j\n" +
            "YS9jcmwucGVtMEMGCWCGSAGG+EIBBAQ2FjRodHRwczovL2NvbnRlbnQtc2lnbmF0\n" +
            "dXJlLmNkbi5tb3ppbGxhLm5ldC9jYS9jcmwucGVtME8GCCsGAQUFBwEBBEMwQTA/\n" +
            "BggrBgEFBQcwAoYzaHR0cHM6Ly9jb250ZW50LXNpZ25hdHVyZS5jZG4ubW96aWxs\n" +
            "YS5uZXQvY2EvY2EucGVtMDMGA1UdEQQsMCqCKGZlbm5lYy1kbGMuY29udGVudC1z\n" +
            "aWduYXR1cmUubW96aWxsYS5vcmcwCgYIKoZIzj0EAwMDZwAwZAIwXVe1A+r/yVwR\n" +
            "DtecWq2DOOIIbq6jPzz/L6GpAw1KHnVMVBnOyrsFNmPyGQb1H60bAjB0dxyh14JB\n" +
            "FCdPO01+y8I7nGRaWqjkmp/GLrdoginSppQYZInYTZagcfy/nIr5fKk=\n" +
            "-----END CERTIFICATE-----\n" +
            "-----BEGIN CERTIFICATE-----\n" +
            "MIIFfjCCA2agAwIBAgIDEAAGMA0GCSqGSIb3DQEBDAUAMH0xCzAJBgNVBAYTAlVT\n" +
            "MRwwGgYDVQQKExNNb3ppbGxhIENvcnBvcmF0aW9uMS8wLQYDVQQLEyZNb3ppbGxh\n" +
            "IEFNTyBQcm9kdWN0aW9uIFNpZ25pbmcgU2VydmljZTEfMB0GA1UEAxMWcm9vdC1j\n" +
            "YS1wcm9kdWN0aW9uLWFtbzAeFw0xNzA1MDQwMDEyMzlaFw0xOTA1MDQwMDEyMzla\n" +
            "MIGmMQswCQYDVQQGEwJVUzEcMBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEv\n" +
            "MC0GA1UECxMmTW96aWxsYSBBTU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2Ux\n" +
            "JTAjBgNVBAMTHENvbnRlbnQgU2lnbmluZyBJbnRlcm1lZGlhdGUxITAfBgkqhkiG\n" +
            "9w0BCQEWEmZveHNlY0Btb3ppbGxhLmNvbTB2MBAGByqGSM49AgEGBSuBBAAiA2IA\n" +
            "BMCmt4C33KfMzsyKokc9SXmMSxozksQglhoGAA1KjlgqEOzcmKEkxtvnGWOA9FLo\n" +
            "A6U7Wmy+7sqmvmjLboAPQc4G0CEudn5Nfk36uEqeyiyKwKSAT+pZsqS4/maXIC7s\n" +
            "DqOCAYkwggGFMAwGA1UdEwQFMAMBAf8wDgYDVR0PAQH/BAQDAgEGMBYGA1UdJQEB\n" +
            "/wQMMAoGCCsGAQUFBwMDMB0GA1UdDgQWBBSIdXKZW/BQ8mC4PbEJlh6PeXme7jCB\n" +
            "qAYDVR0jBIGgMIGdgBSzvOpYdKvhbngqsqucIx6oYyyXt6GBgaR/MH0xCzAJBgNV\n" +
            "BAYTAlVTMRwwGgYDVQQKExNNb3ppbGxhIENvcnBvcmF0aW9uMS8wLQYDVQQLEyZN\n" +
            "b3ppbGxhIEFNTyBQcm9kdWN0aW9uIFNpZ25pbmcgU2VydmljZTEfMB0GA1UEAxMW\n" +
            "cm9vdC1jYS1wcm9kdWN0aW9uLWFtb4IBATAzBglghkgBhvhCAQQEJhYkaHR0cDov\n" +
            "L2FkZG9ucy5hbGxpem9tLm9yZy9jYS9jcmwucGVtME4GA1UdHgRHMEWgQzAggh4u\n" +
            "Y29udGVudC1zaWduYXR1cmUubW96aWxsYS5vcmcwH4IdY29udGVudC1zaWduYXR1\n" +
            "cmUubW96aWxsYS5vcmcwDQYJKoZIhvcNAQEMBQADggIBAKWhLjJB8XmW3VfLvyLF\n" +
            "OOUNeNs7Aju+EZl1PMVXf+917LB//FcJKUQLcEo86I6nC3umUNl+kaq4d3yPDpMV\n" +
            "4DKLHgGmegRsvAyNFQfd64TTxzyfoyfNWH8uy5vvxPmLvWb+jXCoMNF5FgFWEVon\n" +
            "5GDEK8hoHN/DMVe0jveeJhUSuiUpJhMzEf6Vbo0oNgfaRAZKO+VOY617nkTOPnVF\n" +
            "LSEcUPIdE8pcd+QP1t/Ysx+mAfkxAbt+5K298s2bIRLTyNUj1eBtTcCbBbFyWsly\n" +
            "rSMkJihFAWU2MVKqvJ74YI3uNhFzqJ/AAUAPoet14q+ViYU+8a1lqEWj7y8foF3r\n" +
            "m0ZiQpuHULiYCO4y4NR7g5ijj6KsbruLv3e9NyUAIRBHOZEKOA7EiFmWJgqH1aZv\n" +
            "/eS7aQ9HMtPKrlbEwUjV0P3K2U2ljs0rNvO8KO9NKQmocXaRpLm+s8PYBGxby92j\n" +
            "5eelLq55028BSzhJJc6G+cRT9Hlxf1cg2qtqcVJa8i8wc2upCaGycZIlBSX4gj/4\n" +
            "k9faY4qGuGnuEdzAyvIXWMSkb8jiNHQfZrebSr00vShkUEKOLmfFHbkwIaWNK0+2\n" +
            "2c3RL4tDnM5u0kvdgWf0B742JskkxqqmEeZVofsOZJLOhXxO9NO/S0hM16/vf/tl\n" +
            "Tnsnhv0nxUR0B9wxN7XmWmq4\n" +
            "-----END CERTIFICATE-----\n" +
            "-----BEGIN CERTIFICATE-----\n" +
            "MIIGYTCCBEmgAwIBAgIBATANBgkqhkiG9w0BAQwFADB9MQswCQYDVQQGEwJVUzEc\n" +
            "MBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEvMC0GA1UECxMmTW96aWxsYSBB\n" +
            "TU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2UxHzAdBgNVBAMTFnJvb3QtY2Et\n" +
            "cHJvZHVjdGlvbi1hbW8wHhcNMTUwMzE3MjI1MzU3WhcNMjUwMzE0MjI1MzU3WjB9\n" +
            "MQswCQYDVQQGEwJVUzEcMBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEvMC0G\n" +
            "A1UECxMmTW96aWxsYSBBTU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2UxHzAd\n" +
            "BgNVBAMTFnJvb3QtY2EtcHJvZHVjdGlvbi1hbW8wggIgMA0GCSqGSIb3DQEBAQUA\n" +
            "A4ICDQAwggIIAoICAQC0u2HXXbrwy36+MPeKf5jgoASMfMNz7mJWBecJgvlTf4hH\n" +
            "JbLzMPsIUauzI9GEpLfHdZ6wzSyFOb4AM+D1mxAWhuZJ3MDAJOf3B1Rs6QorHrl8\n" +
            "qqlNtPGqepnpNJcLo7JsSqqE3NUm72MgqIHRgTRsqUs+7LIPGe7262U+N/T0LPYV\n" +
            "Le4rZ2RDHoaZhYY7a9+49mHOI/g2YFB+9yZjE+XdplT2kBgA4P8db7i7I0tIi4b0\n" +
            "B0N6y9MhL+CRZJyxdFe2wBykJX14LsheKsM1azHjZO56SKNrW8VAJTLkpRxCmsiT\n" +
            "r08fnPyDKmaeZ0BtsugicdipcZpXriIGmsZbI12q5yuwjSELdkDV6Uajo2n+2ws5\n" +
            "uXrP342X71WiWhC/dF5dz1LKtjBdmUkxaQMOP/uhtXEKBrZo1ounDRQx1j7+SkQ4\n" +
            "BEwjB3SEtr7XDWGOcOIkoJZWPACfBLC3PJCBWjTAyBlud0C5n3Cy9regAAnOIqI1\n" +
            "t16GU2laRh7elJ7gPRNgQgwLXeZcFxw6wvyiEcmCjOEQ6PM8UQjthOsKlszMhlKw\n" +
            "vjyOGDoztkqSBy/v+Asx7OW2Q7rlVfKarL0mREZdSMfoy3zTgtMVCM0vhNl6zcvf\n" +
            "5HNNopoEdg5yuXo2chZ1p1J+q86b0G5yJRMeT2+iOVY2EQ37tHrqUURncCy4uwIB\n" +
            "A6OB7TCB6jAMBgNVHRMEBTADAQH/MA4GA1UdDwEB/wQEAwIBBjAWBgNVHSUBAf8E\n" +
            "DDAKBggrBgEFBQcDAzCBkgYDVR0jBIGKMIGHoYGBpH8wfTELMAkGA1UEBhMCVVMx\n" +
            "HDAaBgNVBAoTE01vemlsbGEgQ29ycG9yYXRpb24xLzAtBgNVBAsTJk1vemlsbGEg\n" +
            "QU1PIFByb2R1Y3Rpb24gU2lnbmluZyBTZXJ2aWNlMR8wHQYDVQQDExZyb290LWNh\n" +
            "LXByb2R1Y3Rpb24tYW1vggEBMB0GA1UdDgQWBBSzvOpYdKvhbngqsqucIx6oYyyX\n" +
            "tzANBgkqhkiG9w0BAQwFAAOCAgEAaNSRYAaECAePQFyfk12kl8UPLh8hBNidP2H6\n" +
            "KT6O0vCVBjxmMrwr8Aqz6NL+TgdPmGRPDDLPDpDJTdWzdj7khAjxqWYhutACTew5\n" +
            "eWEaAzyErbKQl+duKvtThhV2p6F6YHJ2vutu4KIciOMKB8dslIqIQr90IX2Usljq\n" +
            "8Ttdyf+GhUmazqLtoB0GOuESEqT4unX6X7vSGu1oLV20t7t5eCnMMYD67ZBn0YIU\n" +
            "/cm/+pan66hHrja+NeDGF8wabJxdqKItCS3p3GN1zUGuJKrLykxqbOp/21byAGog\n" +
            "Z1amhz6NHUcfE6jki7sM7LHjPostU5ZWs3PEfVVgha9fZUhOrIDsyXEpCWVa3481\n" +
            "LlAq3GiUMKZ5DVRh9/Nvm4NwrTfB3QkQQJCwfXvO9pwnPKtISYkZUqhEqvXk5nBg\n" +
            "QCkDSLDjXTx39naBBGIVIqBtKKuVTla9enngdq692xX/CgO6QJVrwpqdGjebj5P8\n" +
            "5fNZPABzTezG3Uls5Vp+4iIWVAEDkK23cUj3c/HhE+Oo7kxfUeu5Y1ZV3qr61+6t\n" +
            "ZARKjbu1TuYQHf0fs+GwID8zeLc2zJL7UzcHFwwQ6Nda9OJN4uPAuC/BKaIpxCLL\n" +
            "26b24/tRam4SJjqpiq20lynhUrmTtt6hbG3E1Hpy3bmkt2DYnuMFwEx2gfXNcnbT\n" +
            "wNuvFqc=\n" +
            "-----END CERTIFICATE-----"
        testSignature(metadataBody, certChainBody, true)
    }
    */

    @Test
    fun validSignatureIncorrect() {
        val url = server.url("/").url().toString()
        val metadataBody = """
            {
              "data": {
                "signature": {
                  "x5u": "$url",
                  "signature": "kRyhWZdLyjligYHSFhzhbyzUXBoUwoTPvyt9V0e-E7LKGgUYF2MVfqpA2zfIEDdqrImcMABVGHLUx9Nk614zciRBQ-gyaKA5SL2pPdZvoQXk_LLsPhEBgG4VDnxG4SBL"
                }
              }
            }
        """.trimIndent()
        val certChainBody = "-----BEGIN CERTIFICATE-----\n" +
            "MIIEpTCCBCygAwIBAgIEAQAAKTAKBggqhkjOPQQDAzCBpjELMAkGA1UEBhMCVVMx\n" +
            "HDAaBgNVBAoTE01vemlsbGEgQ29ycG9yYXRpb24xLzAtBgNVBAsTJk1vemlsbGEg\n" +
            "QU1PIFByb2R1Y3Rpb24gU2lnbmluZyBTZXJ2aWNlMSUwIwYDVQQDExxDb250ZW50\n" +
            "IFNpZ25pbmcgSW50ZXJtZWRpYXRlMSEwHwYJKoZIhvcNAQkBFhJmb3hzZWNAbW96\n" +
            "aWxsYS5jb20wIhgPMjAxODAyMTgwMDAwMDBaGA8yMDE4MDYxODAwMDAwMFowgbEx\n" +
            "CzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpDYWxpZm9ybmlhMRwwGgYDVQQKExNNb3pp\n" +
            "bGxhIENvcnBvcmF0aW9uMRcwFQYDVQQLEw5DbG91ZCBTZXJ2aWNlczExMC8GA1UE\n" +
            "AxMoZmVubmVjLWRsYy5jb250ZW50LXNpZ25hdHVyZS5tb3ppbGxhLm9yZzEjMCEG\n" +
            "CSqGSIb3DQEJARYUc2VjdXJpdHlAbW96aWxsYS5vcmcwdjAQBgcqhkjOPQIBBgUr\n" +
            "gQQAIgNiAATpKfWqAyDsh2ISzBycb8Y7JqLygByKI9vI2WZ2VWaGTYfmB1tQ8PFj\n" +
            "vZrtDqZeO8Dhs2KiMrvs/uoziM2zselYQhd0mz5z3dMui6BD6SPKB83K7xn2r2mW\n" +
            "plAZxuwnPKujggIYMIICFDAdBgNVHQ4EFgQUTp9+0KSp+vkzbEcXAENBCIyI9eow\n" +
            "gaoGA1UdIwSBojCBn4AUiHVymVvwUPJguD2xCZYej3l5nu6hgYGkfzB9MQswCQYD\n" +
            "VQQGEwJVUzEcMBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEvMC0GA1UECxMm\n" +
            "TW96aWxsYSBBTU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2UxHzAdBgNVBAMT\n" +
            "FnJvb3QtY2EtcHJvZHVjdGlvbi1hbW+CAxAABjAMBgNVHRMBAf8EAjAAMA4GA1Ud\n" +
            "DwEB/wQEAwIHgDAWBgNVHSUBAf8EDDAKBggrBgEFBQcDAzBFBgNVHR8EPjA8MDqg\n" +
            "OKA2hjRodHRwczovL2NvbnRlbnQtc2lnbmF0dXJlLmNkbi5tb3ppbGxhLm5ldC9j\n" +
            "YS9jcmwucGVtMEMGCWCGSAGG+EIBBAQ2FjRodHRwczovL2NvbnRlbnQtc2lnbmF0\n" +
            "dXJlLmNkbi5tb3ppbGxhLm5ldC9jYS9jcmwucGVtME8GCCsGAQUFBwEBBEMwQTA/\n" +
            "BggrBgEFBQcwAoYzaHR0cHM6Ly9jb250ZW50LXNpZ25hdHVyZS5jZG4ubW96aWxs\n" +
            "YS5uZXQvY2EvY2EucGVtMDMGA1UdEQQsMCqCKGZlbm5lYy1kbGMuY29udGVudC1z\n" +
            "aWduYXR1cmUubW96aWxsYS5vcmcwCgYIKoZIzj0EAwMDZwAwZAIwXVe1A+r/yVwR\n" +
            "DtecWq2DOOIIbq6jPzz/L6GpAw1KHnVMVBnOyrsFNmPyGQb1H60bAjB0dxyh14JB\n" +
            "FCdPO01+y8I7nGRaWqjkmp/GLrdoginSppQYZInYTZagcfy/nIr5fKk=\n" +
            "-----END CERTIFICATE-----\n" +
            "-----BEGIN CERTIFICATE-----\n" +
            "MIIFfjCCA2agAwIBAgIDEAAGMA0GCSqGSIb3DQEBDAUAMH0xCzAJBgNVBAYTAlVT\n" +
            "MRwwGgYDVQQKExNNb3ppbGxhIENvcnBvcmF0aW9uMS8wLQYDVQQLEyZNb3ppbGxh\n" +
            "IEFNTyBQcm9kdWN0aW9uIFNpZ25pbmcgU2VydmljZTEfMB0GA1UEAxMWcm9vdC1j\n" +
            "YS1wcm9kdWN0aW9uLWFtbzAeFw0xNzA1MDQwMDEyMzlaFw0xOTA1MDQwMDEyMzla\n" +
            "MIGmMQswCQYDVQQGEwJVUzEcMBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEv\n" +
            "MC0GA1UECxMmTW96aWxsYSBBTU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2Ux\n" +
            "JTAjBgNVBAMTHENvbnRlbnQgU2lnbmluZyBJbnRlcm1lZGlhdGUxITAfBgkqhkiG\n" +
            "9w0BCQEWEmZveHNlY0Btb3ppbGxhLmNvbTB2MBAGByqGSM49AgEGBSuBBAAiA2IA\n" +
            "BMCmt4C33KfMzsyKokc9SXmMSxozksQglhoGAA1KjlgqEOzcmKEkxtvnGWOA9FLo\n" +
            "A6U7Wmy+7sqmvmjLboAPQc4G0CEudn5Nfk36uEqeyiyKwKSAT+pZsqS4/maXIC7s\n" +
            "DqOCAYkwggGFMAwGA1UdEwQFMAMBAf8wDgYDVR0PAQH/BAQDAgEGMBYGA1UdJQEB\n" +
            "/wQMMAoGCCsGAQUFBwMDMB0GA1UdDgQWBBSIdXKZW/BQ8mC4PbEJlh6PeXme7jCB\n" +
            "qAYDVR0jBIGgMIGdgBSzvOpYdKvhbngqsqucIx6oYyyXt6GBgaR/MH0xCzAJBgNV\n" +
            "BAYTAlVTMRwwGgYDVQQKExNNb3ppbGxhIENvcnBvcmF0aW9uMS8wLQYDVQQLEyZN\n" +
            "b3ppbGxhIEFNTyBQcm9kdWN0aW9uIFNpZ25pbmcgU2VydmljZTEfMB0GA1UEAxMW\n" +
            "cm9vdC1jYS1wcm9kdWN0aW9uLWFtb4IBATAzBglghkgBhvhCAQQEJhYkaHR0cDov\n" +
            "L2FkZG9ucy5hbGxpem9tLm9yZy9jYS9jcmwucGVtME4GA1UdHgRHMEWgQzAggh4u\n" +
            "Y29udGVudC1zaWduYXR1cmUubW96aWxsYS5vcmcwH4IdY29udGVudC1zaWduYXR1\n" +
            "cmUubW96aWxsYS5vcmcwDQYJKoZIhvcNAQEMBQADggIBAKWhLjJB8XmW3VfLvyLF\n" +
            "OOUNeNs7Aju+EZl1PMVXf+917LB//FcJKUQLcEo86I6nC3umUNl+kaq4d3yPDpMV\n" +
            "4DKLHgGmegRsvAyNFQfd64TTxzyfoyfNWH8uy5vvxPmLvWb+jXCoMNF5FgFWEVon\n" +
            "5GDEK8hoHN/DMVe0jveeJhUSuiUpJhMzEf6Vbo0oNgfaRAZKO+VOY617nkTOPnVF\n" +
            "LSEcUPIdE8pcd+QP1t/Ysx+mAfkxAbt+5K298s2bIRLTyNUj1eBtTcCbBbFyWsly\n" +
            "rSMkJihFAWU2MVKqvJ74YI3uNhFzqJ/AAUAPoet14q+ViYU+8a1lqEWj7y8foF3r\n" +
            "m0ZiQpuHULiYCO4y4NR7g5ijj6KsbruLv3e9NyUAIRBHOZEKOA7EiFmWJgqH1aZv\n" +
            "/eS7aQ9HMtPKrlbEwUjV0P3K2U2ljs0rNvO8KO9NKQmocXaRpLm+s8PYBGxby92j\n" +
            "5eelLq55028BSzhJJc6G+cRT9Hlxf1cg2qtqcVJa8i8wc2upCaGycZIlBSX4gj/4\n" +
            "k9faY4qGuGnuEdzAyvIXWMSkb8jiNHQfZrebSr00vShkUEKOLmfFHbkwIaWNK0+2\n" +
            "2c3RL4tDnM5u0kvdgWf0B742JskkxqqmEeZVofsOZJLOhXxO9NO/S0hM16/vf/tl\n" +
            "Tnsnhv0nxUR0B9wxN7XmWmq4\n" +
            "-----END CERTIFICATE-----\n" +
            "-----BEGIN CERTIFICATE-----\n" +
            "MIIGYTCCBEmgAwIBAgIBATANBgkqhkiG9w0BAQwFADB9MQswCQYDVQQGEwJVUzEc\n" +
            "MBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEvMC0GA1UECxMmTW96aWxsYSBB\n" +
            "TU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2UxHzAdBgNVBAMTFnJvb3QtY2Et\n" +
            "cHJvZHVjdGlvbi1hbW8wHhcNMTUwMzE3MjI1MzU3WhcNMjUwMzE0MjI1MzU3WjB9\n" +
            "MQswCQYDVQQGEwJVUzEcMBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEvMC0G\n" +
            "A1UECxMmTW96aWxsYSBBTU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2UxHzAd\n" +
            "BgNVBAMTFnJvb3QtY2EtcHJvZHVjdGlvbi1hbW8wggIgMA0GCSqGSIb3DQEBAQUA\n" +
            "A4ICDQAwggIIAoICAQC0u2HXXbrwy36+MPeKf5jgoASMfMNz7mJWBecJgvlTf4hH\n" +
            "JbLzMPsIUauzI9GEpLfHdZ6wzSyFOb4AM+D1mxAWhuZJ3MDAJOf3B1Rs6QorHrl8\n" +
            "qqlNtPGqepnpNJcLo7JsSqqE3NUm72MgqIHRgTRsqUs+7LIPGe7262U+N/T0LPYV\n" +
            "Le4rZ2RDHoaZhYY7a9+49mHOI/g2YFB+9yZjE+XdplT2kBgA4P8db7i7I0tIi4b0\n" +
            "B0N6y9MhL+CRZJyxdFe2wBykJX14LsheKsM1azHjZO56SKNrW8VAJTLkpRxCmsiT\n" +
            "r08fnPyDKmaeZ0BtsugicdipcZpXriIGmsZbI12q5yuwjSELdkDV6Uajo2n+2ws5\n" +
            "uXrP342X71WiWhC/dF5dz1LKtjBdmUkxaQMOP/uhtXEKBrZo1ounDRQx1j7+SkQ4\n" +
            "BEwjB3SEtr7XDWGOcOIkoJZWPACfBLC3PJCBWjTAyBlud0C5n3Cy9regAAnOIqI1\n" +
            "t16GU2laRh7elJ7gPRNgQgwLXeZcFxw6wvyiEcmCjOEQ6PM8UQjthOsKlszMhlKw\n" +
            "vjyOGDoztkqSBy/v+Asx7OW2Q7rlVfKarL0mREZdSMfoy3zTgtMVCM0vhNl6zcvf\n" +
            "5HNNopoEdg5yuXo2chZ1p1J+q86b0G5yJRMeT2+iOVY2EQ37tHrqUURncCy4uwIB\n" +
            "A6OB7TCB6jAMBgNVHRMEBTADAQH/MA4GA1UdDwEB/wQEAwIBBjAWBgNVHSUBAf8E\n" +
            "DDAKBggrBgEFBQcDAzCBkgYDVR0jBIGKMIGHoYGBpH8wfTELMAkGA1UEBhMCVVMx\n" +
            "HDAaBgNVBAoTE01vemlsbGEgQ29ycG9yYXRpb24xLzAtBgNVBAsTJk1vemlsbGEg\n" +
            "QU1PIFByb2R1Y3Rpb24gU2lnbmluZyBTZXJ2aWNlMR8wHQYDVQQDExZyb290LWNh\n" +
            "LXByb2R1Y3Rpb24tYW1vggEBMB0GA1UdDgQWBBSzvOpYdKvhbngqsqucIx6oYyyX\n" +
            "tzANBgkqhkiG9w0BAQwFAAOCAgEAaNSRYAaECAePQFyfk12kl8UPLh8hBNidP2H6\n" +
            "KT6O0vCVBjxmMrwr8Aqz6NL+TgdPmGRPDDLPDpDJTdWzdj7khAjxqWYhutACTew5\n" +
            "eWEaAzyErbKQl+duKvtThhV2p6F6YHJ2vutu4KIciOMKB8dslIqIQr90IX2Usljq\n" +
            "8Ttdyf+GhUmazqLtoB0GOuESEqT4unX6X7vSGu1oLV20t7t5eCnMMYD67ZBn0YIU\n" +
            "/cm/+pan66hHrja+NeDGF8wabJxdqKItCS3p3GN1zUGuJKrLykxqbOp/21byAGog\n" +
            "Z1amhz6NHUcfE6jki7sM7LHjPostU5ZWs3PEfVVgha9fZUhOrIDsyXEpCWVa3481\n" +
            "LlAq3GiUMKZ5DVRh9/Nvm4NwrTfB3QkQQJCwfXvO9pwnPKtISYkZUqhEqvXk5nBg\n" +
            "QCkDSLDjXTx39naBBGIVIqBtKKuVTla9enngdq692xX/CgO6QJVrwpqdGjebj5P8\n" +
            "5fNZPABzTezG3Uls5Vp+4iIWVAEDkK23cUj3c/HhE+Oo7kxfUeu5Y1ZV3qr61+6t\n" +
            "ZARKjbu1TuYQHf0fs+GwID8zeLc2zJL7UzcHFwwQ6Nda9OJN4uPAuC/BKaIpxCLL\n" +
            "26b24/tRam4SJjqpiq20lynhUrmTtt6hbG3E1Hpy3bmkt2DYnuMFwEx2gfXNcnbT\n" +
            "wNuvFqc=\n" +
            "-----END CERTIFICATE-----"
        testSignature(metadataBody, certChainBody, false)
    }

    @Test(expected = ExperimentDownloadException::class)
    fun validSignatureInvalidChainOrder() {
        val url = server.url("/").url().toString()
        val metadataBody = """
            {
              "data": {
                "signature": {
                  "x5u": "$url",
                  "signature": "kRhyWZdLyjligYHSFhzhbyzUXBoUwoTPvyt9V0e-E7LKGgUYF2MVfqpA2zfIEDdqrImcMABVGHLUx9Nk614zciRBQ-gyaKA5SL2pPdZvoQXk_LLsPhEBgG4VDnxG4SBL"
                }
              }
            }
        """.trimIndent()
        val certChainBody = "-----BEGIN CERTIFICATE-----\n" +
            "MIIEpTCCBCygAwIBAgIEAQAAKTAKBggqhkjOPQQDAzCBpjELMAkGA1UEBhMCVVMx\n" +
            "HDAaBgNVBAoTE01vemlsbGEgQ29ycG9yYXRpb24xLzAtBgNVBAsTJk1vemlsbGEg\n" +
            "QU1PIFByb2R1Y3Rpb24gU2lnbmluZyBTZXJ2aWNlMSUwIwYDVQQDExxDb250ZW50\n" +
            "IFNpZ25pbmcgSW50ZXJtZWRpYXRlMSEwHwYJKoZIhvcNAQkBFhJmb3hzZWNAbW96\n" +
            "aWxsYS5jb20wIhgPMjAxODAyMTgwMDAwMDBaGA8yMDE4MDYxODAwMDAwMFowgbEx\n" +
            "CzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpDYWxpZm9ybmlhMRwwGgYDVQQKExNNb3pp\n" +
            "bGxhIENvcnBvcmF0aW9uMRcwFQYDVQQLEw5DbG91ZCBTZXJ2aWNlczExMC8GA1UE\n" +
            "AxMoZmVubmVjLWRsYy5jb250ZW50LXNpZ25hdHVyZS5tb3ppbGxhLm9yZzEjMCEG\n" +
            "CSqGSIb3DQEJARYUc2VjdXJpdHlAbW96aWxsYS5vcmcwdjAQBgcqhkjOPQIBBgUr\n" +
            "gQQAIgNiAATpKfWqAyDsh2ISzBycb8Y7JqLygByKI9vI2WZ2VWaGTYfmB1tQ8PFj\n" +
            "vZrtDqZeO8Dhs2KiMrvs/uoziM2zselYQhd0mz5z3dMui6BD6SPKB83K7xn2r2mW\n" +
            "plAZxuwnPKujggIYMIICFDAdBgNVHQ4EFgQUTp9+0KSp+vkzbEcXAENBCIyI9eow\n" +
            "gaoGA1UdIwSBojCBn4AUiHVymVvwUPJguD2xCZYej3l5nu6hgYGkfzB9MQswCQYD\n" +
            "VQQGEwJVUzEcMBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEvMC0GA1UECxMm\n" +
            "TW96aWxsYSBBTU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2UxHzAdBgNVBAMT\n" +
            "FnJvb3QtY2EtcHJvZHVjdGlvbi1hbW+CAxAABjAMBgNVHRMBAf8EAjAAMA4GA1Ud\n" +
            "DwEB/wQEAwIHgDAWBgNVHSUBAf8EDDAKBggrBgEFBQcDAzBFBgNVHR8EPjA8MDqg\n" +
            "OKA2hjRodHRwczovL2NvbnRlbnQtc2lnbmF0dXJlLmNkbi5tb3ppbGxhLm5ldC9j\n" +
            "YS9jcmwucGVtMEMGCWCGSAGG+EIBBAQ2FjRodHRwczovL2NvbnRlbnQtc2lnbmF0\n" +
            "dXJlLmNkbi5tb3ppbGxhLm5ldC9jYS9jcmwucGVtME8GCCsGAQUFBwEBBEMwQTA/\n" +
            "BggrBgEFBQcwAoYzaHR0cHM6Ly9jb250ZW50LXNpZ25hdHVyZS5jZG4ubW96aWxs\n" +
            "YS5uZXQvY2EvY2EucGVtMDMGA1UdEQQsMCqCKGZlbm5lYy1kbGMuY29udGVudC1z\n" +
            "aWduYXR1cmUubW96aWxsYS5vcmcwCgYIKoZIzj0EAwMDZwAwZAIwXVe1A+r/yVwR\n" +
            "DtecWq2DOOIIbq6jPzz/L6GpAw1KHnVMVBnOyrsFNmPyGQb1H60bAjB0dxyh14JB\n" +
            "FCdPO01+y8I7nGRaWqjkmp/GLrdoginSppQYZInYTZagcfy/nIr5fKk=\n" +
            "-----END CERTIFICATE-----\n" +
            "-----BEGIN CERTIFICATE-----\n" +
            "MIIGYTCCBEmgAwIBAgIBATANBgkqhkiG9w0BAQwFADB9MQswCQYDVQQGEwJVUzEc\n" +
            "MBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEvMC0GA1UECxMmTW96aWxsYSBB\n" +
            "TU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2UxHzAdBgNVBAMTFnJvb3QtY2Et\n" +
            "cHJvZHVjdGlvbi1hbW8wHhcNMTUwMzE3MjI1MzU3WhcNMjUwMzE0MjI1MzU3WjB9\n" +
            "MQswCQYDVQQGEwJVUzEcMBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEvMC0G\n" +
            "A1UECxMmTW96aWxsYSBBTU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2UxHzAd\n" +
            "BgNVBAMTFnJvb3QtY2EtcHJvZHVjdGlvbi1hbW8wggIgMA0GCSqGSIb3DQEBAQUA\n" +
            "A4ICDQAwggIIAoICAQC0u2HXXbrwy36+MPeKf5jgoASMfMNz7mJWBecJgvlTf4hH\n" +
            "JbLzMPsIUauzI9GEpLfHdZ6wzSyFOb4AM+D1mxAWhuZJ3MDAJOf3B1Rs6QorHrl8\n" +
            "qqlNtPGqepnpNJcLo7JsSqqE3NUm72MgqIHRgTRsqUs+7LIPGe7262U+N/T0LPYV\n" +
            "Le4rZ2RDHoaZhYY7a9+49mHOI/g2YFB+9yZjE+XdplT2kBgA4P8db7i7I0tIi4b0\n" +
            "B0N6y9MhL+CRZJyxdFe2wBykJX14LsheKsM1azHjZO56SKNrW8VAJTLkpRxCmsiT\n" +
            "r08fnPyDKmaeZ0BtsugicdipcZpXriIGmsZbI12q5yuwjSELdkDV6Uajo2n+2ws5\n" +
            "uXrP342X71WiWhC/dF5dz1LKtjBdmUkxaQMOP/uhtXEKBrZo1ounDRQx1j7+SkQ4\n" +
            "BEwjB3SEtr7XDWGOcOIkoJZWPACfBLC3PJCBWjTAyBlud0C5n3Cy9regAAnOIqI1\n" +
            "t16GU2laRh7elJ7gPRNgQgwLXeZcFxw6wvyiEcmCjOEQ6PM8UQjthOsKlszMhlKw\n" +
            "vjyOGDoztkqSBy/v+Asx7OW2Q7rlVfKarL0mREZdSMfoy3zTgtMVCM0vhNl6zcvf\n" +
            "5HNNopoEdg5yuXo2chZ1p1J+q86b0G5yJRMeT2+iOVY2EQ37tHrqUURncCy4uwIB\n" +
            "A6OB7TCB6jAMBgNVHRMEBTADAQH/MA4GA1UdDwEB/wQEAwIBBjAWBgNVHSUBAf8E\n" +
            "DDAKBggrBgEFBQcDAzCBkgYDVR0jBIGKMIGHoYGBpH8wfTELMAkGA1UEBhMCVVMx\n" +
            "HDAaBgNVBAoTE01vemlsbGEgQ29ycG9yYXRpb24xLzAtBgNVBAsTJk1vemlsbGEg\n" +
            "QU1PIFByb2R1Y3Rpb24gU2lnbmluZyBTZXJ2aWNlMR8wHQYDVQQDExZyb290LWNh\n" +
            "LXByb2R1Y3Rpb24tYW1vggEBMB0GA1UdDgQWBBSzvOpYdKvhbngqsqucIx6oYyyX\n" +
            "tzANBgkqhkiG9w0BAQwFAAOCAgEAaNSRYAaECAePQFyfk12kl8UPLh8hBNidP2H6\n" +
            "KT6O0vCVBjxmMrwr8Aqz6NL+TgdPmGRPDDLPDpDJTdWzdj7khAjxqWYhutACTew5\n" +
            "eWEaAzyErbKQl+duKvtThhV2p6F6YHJ2vutu4KIciOMKB8dslIqIQr90IX2Usljq\n" +
            "8Ttdyf+GhUmazqLtoB0GOuESEqT4unX6X7vSGu1oLV20t7t5eCnMMYD67ZBn0YIU\n" +
            "/cm/+pan66hHrja+NeDGF8wabJxdqKItCS3p3GN1zUGuJKrLykxqbOp/21byAGog\n" +
            "Z1amhz6NHUcfE6jki7sM7LHjPostU5ZWs3PEfVVgha9fZUhOrIDsyXEpCWVa3481\n" +
            "LlAq3GiUMKZ5DVRh9/Nvm4NwrTfB3QkQQJCwfXvO9pwnPKtISYkZUqhEqvXk5nBg\n" +
            "QCkDSLDjXTx39naBBGIVIqBtKKuVTla9enngdq692xX/CgO6QJVrwpqdGjebj5P8\n" +
            "5fNZPABzTezG3Uls5Vp+4iIWVAEDkK23cUj3c/HhE+Oo7kxfUeu5Y1ZV3qr61+6t\n" +
            "ZARKjbu1TuYQHf0fs+GwID8zeLc2zJL7UzcHFwwQ6Nda9OJN4uPAuC/BKaIpxCLL\n" +
            "26b24/tRam4SJjqpiq20lynhUrmTtt6hbG3E1Hpy3bmkt2DYnuMFwEx2gfXNcnbT\n" +
            "wNuvFqc=\n" +
            "-----END CERTIFICATE-----\n" +
            "-----BEGIN CERTIFICATE-----\n" +
            "MIIFfjCCA2agAwIBAgIDEAAGMA0GCSqGSIb3DQEBDAUAMH0xCzAJBgNVBAYTAlVT\n" +
            "MRwwGgYDVQQKExNNb3ppbGxhIENvcnBvcmF0aW9uMS8wLQYDVQQLEyZNb3ppbGxh\n" +
            "IEFNTyBQcm9kdWN0aW9uIFNpZ25pbmcgU2VydmljZTEfMB0GA1UEAxMWcm9vdC1j\n" +
            "YS1wcm9kdWN0aW9uLWFtbzAeFw0xNzA1MDQwMDEyMzlaFw0xOTA1MDQwMDEyMzla\n" +
            "MIGmMQswCQYDVQQGEwJVUzEcMBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEv\n" +
            "MC0GA1UECxMmTW96aWxsYSBBTU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2Ux\n" +
            "JTAjBgNVBAMTHENvbnRlbnQgU2lnbmluZyBJbnRlcm1lZGlhdGUxITAfBgkqhkiG\n" +
            "9w0BCQEWEmZveHNlY0Btb3ppbGxhLmNvbTB2MBAGByqGSM49AgEGBSuBBAAiA2IA\n" +
            "BMCmt4C33KfMzsyKokc9SXmMSxozksQglhoGAA1KjlgqEOzcmKEkxtvnGWOA9FLo\n" +
            "A6U7Wmy+7sqmvmjLboAPQc4G0CEudn5Nfk36uEqeyiyKwKSAT+pZsqS4/maXIC7s\n" +
            "DqOCAYkwggGFMAwGA1UdEwQFMAMBAf8wDgYDVR0PAQH/BAQDAgEGMBYGA1UdJQEB\n" +
            "/wQMMAoGCCsGAQUFBwMDMB0GA1UdDgQWBBSIdXKZW/BQ8mC4PbEJlh6PeXme7jCB\n" +
            "qAYDVR0jBIGgMIGdgBSzvOpYdKvhbngqsqucIx6oYyyXt6GBgaR/MH0xCzAJBgNV\n" +
            "BAYTAlVTMRwwGgYDVQQKExNNb3ppbGxhIENvcnBvcmF0aW9uMS8wLQYDVQQLEyZN\n" +
            "b3ppbGxhIEFNTyBQcm9kdWN0aW9uIFNpZ25pbmcgU2VydmljZTEfMB0GA1UEAxMW\n" +
            "cm9vdC1jYS1wcm9kdWN0aW9uLWFtb4IBATAzBglghkgBhvhCAQQEJhYkaHR0cDov\n" +
            "L2FkZG9ucy5hbGxpem9tLm9yZy9jYS9jcmwucGVtME4GA1UdHgRHMEWgQzAggh4u\n" +
            "Y29udGVudC1zaWduYXR1cmUubW96aWxsYS5vcmcwH4IdY29udGVudC1zaWduYXR1\n" +
            "cmUubW96aWxsYS5vcmcwDQYJKoZIhvcNAQEMBQADggIBAKWhLjJB8XmW3VfLvyLF\n" +
            "OOUNeNs7Aju+EZl1PMVXf+917LB//FcJKUQLcEo86I6nC3umUNl+kaq4d3yPDpMV\n" +
            "4DKLHgGmegRsvAyNFQfd64TTxzyfoyfNWH8uy5vvxPmLvWb+jXCoMNF5FgFWEVon\n" +
            "5GDEK8hoHN/DMVe0jveeJhUSuiUpJhMzEf6Vbo0oNgfaRAZKO+VOY617nkTOPnVF\n" +
            "LSEcUPIdE8pcd+QP1t/Ysx+mAfkxAbt+5K298s2bIRLTyNUj1eBtTcCbBbFyWsly\n" +
            "rSMkJihFAWU2MVKqvJ74YI3uNhFzqJ/AAUAPoet14q+ViYU+8a1lqEWj7y8foF3r\n" +
            "m0ZiQpuHULiYCO4y4NR7g5ijj6KsbruLv3e9NyUAIRBHOZEKOA7EiFmWJgqH1aZv\n" +
            "/eS7aQ9HMtPKrlbEwUjV0P3K2U2ljs0rNvO8KO9NKQmocXaRpLm+s8PYBGxby92j\n" +
            "5eelLq55028BSzhJJc6G+cRT9Hlxf1cg2qtqcVJa8i8wc2upCaGycZIlBSX4gj/4\n" +
            "k9faY4qGuGnuEdzAyvIXWMSkb8jiNHQfZrebSr00vShkUEKOLmfFHbkwIaWNK0+2\n" +
            "2c3RL4tDnM5u0kvdgWf0B742JskkxqqmEeZVofsOZJLOhXxO9NO/S0hM16/vf/tl\n" +
            "Tnsnhv0nxUR0B9wxN7XmWmq4\n" +
            "-----END CERTIFICATE-----\n"
        testSignature(metadataBody, certChainBody, true)
    }

    @Test(expected = ExperimentDownloadException::class)
    fun validSignatureExpiredCertificate() {
        val url = server.url("/").url().toString()
        val metadataBody = """
            {
              "data": {
                "signature": {
                  "x5u": "$url",
                  "signature": "kRhyWZdLyjligYHSFhzhbyzUXBoUwoTPvyt9V0e-E7LKGgUYF2MVfqpA2zfIEDdqrImcMABVGHLUx9Nk614zciRBQ-gyaKA5SL2pPdZvoQXk_LLsPhEBgG4VDnxG4SBL"
                }
              }
            }
        """.trimIndent()
        val certChainBody = "-----BEGIN CERTIFICATE-----\n" +
            "MIIEpTCCBCygAwIBAgIEAQAAKTAKBggqhkjOPQQDAzCBpjELMAkGA1UEBhMCVVMx\n" +
            "HDAaBgNVBAoTE01vemlsbGEgQ29ycG9yYXRpb24xLzAtBgNVBAsTJk1vemlsbGEg\n" +
            "QU1PIFByb2R1Y3Rpb24gU2lnbmluZyBTZXJ2aWNlMSUwIwYDVQQDExxDb250ZW50\n" +
            "IFNpZ25pbmcgSW50ZXJtZWRpYXRlMSEwHwYJKoZIhvcNAQkBFhJmb3hzZWNAbW96\n" +
            "aWxsYS5jb20wIhgPMjAxODAyMTgwMDAwMDBaGA8yMDE4MDYxODAwMDAwMFowgbEx\n" +
            "CzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpDYWxpZm9ybmlhMRwwGgYDVQQKExNNb3pp\n" +
            "bGxhIENvcnBvcmF0aW9uMRcwFQYDVQQLEw5DbG91ZCBTZXJ2aWNlczExMC8GA1UE\n" +
            "AxMoZmVubmVjLWRsYy5jb250ZW50LXNpZ25hdHVyZS5tb3ppbGxhLm9yZzEjMCEG\n" +
            "CSqGSIb3DQEJARYUc2VjdXJpdHlAbW96aWxsYS5vcmcwdjAQBgcqhkjOPQIBBgUr\n" +
            "gQQAIgNiAATpKfWqAyDsh2ISzBycb8Y7JqLygByKI9vI2WZ2VWaGTYfmB1tQ8PFj\n" +
            "vZrtDqZeO8Dhs2KiMrvs/uoziM2zselYQhd0mz5z3dMui6BD6SPKB83K7xn2r2mW\n" +
            "plAZxuwnPKujggIYMIICFDAdBgNVHQ4EFgQUTp9+0KSp+vkzbEcXAENBCIyI9eow\n" +
            "gaoGA1UdIwSBojCBn4AUiHVymVvwUPJguD2xCZYej3l5nu6hgYGkfzB9MQswCQYD\n" +
            "VQQGEwJVUzEcMBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEvMC0GA1UECxMm\n" +
            "TW96aWxsYSBBTU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2UxHzAdBgNVBAMT\n" +
            "FnJvb3QtY2EtcHJvZHVjdGlvbi1hbW+CAxAABjAMBgNVHRMBAf8EAjAAMA4GA1Ud\n" +
            "DwEB/wQEAwIHgDAWBgNVHSUBAf8EDDAKBggrBgEFBQcDAzBFBgNVHR8EPjA8MDqg\n" +
            "OKA2hjRodHRwczovL2NvbnRlbnQtc2lnbmF0dXJlLmNkbi5tb3ppbGxhLm5ldC9j\n" +
            "YS9jcmwucGVtMEMGCWCGSAGG+EIBBAQ2FjRodHRwczovL2NvbnRlbnQtc2lnbmF0\n" +
            "dXJlLmNkbi5tb3ppbGxhLm5ldC9jYS9jcmwucGVtME8GCCsGAQUFBwEBBEMwQTA/\n" +
            "BggrBgEFBQcwAoYzaHR0cHM6Ly9jb250ZW50LXNpZ25hdHVyZS5jZG4ubW96aWxs\n" +
            "YS5uZXQvY2EvY2EucGVtMDMGA1UdEQQsMCqCKGZlbm5lYy1kbGMuY29udGVudC1z\n" +
            "aWduYXR1cmUubW96aWxsYS5vcmcwCgYIKoZIzj0EAwMDZwAwZAIwXVe1A+r/yVwR\n" +
            "DtecWq2DOOIIbq6jPzz/L6GpAw1KHnVMVBnOyrsFNmPyGQb1H60bAjB0dxyh14JB\n" +
            "FCdPO01+y8I7nGRaWqjkmp/GLrdoginSppQYZInYTZagcfy/nIr5fKk=\n" +
            "-----END CERTIFICATE-----\n" +
            "-----BEGIN CERTIFICATE-----\n" +
            "MIIFfjCCA2agAwIBAgIDEAAGMA0GCSqGSIb3DQEBDAUAMH0xCzAJBgNVBAYTAlVT\n" +
            "MRwwGgYDVQQKExNNb3ppbGxhIENvcnBvcmF0aW9uMS8wLQYDVQQLEyZNb3ppbGxh\n" +
            "IEFNTyBQcm9kdWN0aW9uIFNpZ25pbmcgU2VydmljZTEfMB0GA1UEAxMWcm9vdC1j\n" +
            "YS1wcm9kdWN0aW9uLWFtbzAeFw0xNzA1MDQwMDEyMzlaFw0xOTA1MDQwMDEyMzla\n" +
            "MIGmMQswCQYDVQQGEwJVUzEcMBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEv\n" +
            "MC0GA1UECxMmTW96aWxsYSBBTU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2Ux\n" +
            "JTAjBgNVBAMTHENvbnRlbnQgU2lnbmluZyBJbnRlcm1lZGlhdGUxITAfBgkqhkiG\n" +
            "9w0BCQEWEmZveHNlY0Btb3ppbGxhLmNvbTB2MBAGByqGSM49AgEGBSuBBAAiA2IA\n" +
            "BMCmt4C33KfMzsyKokc9SXmMSxozksQglhoGAA1KjlgqEOzcmKEkxtvnGWOA9FLo\n" +
            "A6U7Wmy+7sqmvmjLboAPQc4G0CEudn5Nfk36uEqeyiyKwKSAT+pZsqS4/maXIC7s\n" +
            "DqOCAYkwggGFMAwGA1UdEwQFMAMBAf8wDgYDVR0PAQH/BAQDAgEGMBYGA1UdJQEB\n" +
            "/wQMMAoGCCsGAQUFBwMDMB0GA1UdDgQWBBSIdXKZW/BQ8mC4PbEJlh6PeXme7jCB\n" +
            "qAYDVR0jBIGgMIGdgBSzvOpYdKvhbngqsqucIx6oYyyXt6GBgaR/MH0xCzAJBgNV\n" +
            "BAYTAlVTMRwwGgYDVQQKExNNb3ppbGxhIENvcnBvcmF0aW9uMS8wLQYDVQQLEyZN\n" +
            "b3ppbGxhIEFNTyBQcm9kdWN0aW9uIFNpZ25pbmcgU2VydmljZTEfMB0GA1UEAxMW\n" +
            "cm9vdC1jYS1wcm9kdWN0aW9uLWFtb4IBATAzBglghkgBhvhCAQQEJhYkaHR0cDov\n" +
            "L2FkZG9ucy5hbGxpem9tLm9yZy9jYS9jcmwucGVtME4GA1UdHgRHMEWgQzAggh4u\n" +
            "Y29udGVudC1zaWduYXR1cmUubW96aWxsYS5vcmcwH4IdY29udGVudC1zaWduYXR1\n" +
            "cmUubW96aWxsYS5vcmcwDQYJKoZIhvcNAQEMBQADggIBAKWhLjJB8XmW3VfLvyLF\n" +
            "OOUNeNs7Aju+EZl1PMVXf+917LB//FcJKUQLcEo86I6nC3umUNl+kaq4d3yPDpMV\n" +
            "4DKLHgGmegRsvAyNFQfd64TTxzyfoyfNWH8uy5vvxPmLvWb+jXCoMNF5FgFWEVon\n" +
            "5GDEK8hoHN/DMVe0jveeJhUSuiUpJhMzEf6Vbo0oNgfaRAZKO+VOY617nkTOPnVF\n" +
            "LSEcUPIdE8pcd+QP1t/Ysx+mAfkxAbt+5K298s2bIRLTyNUj1eBtTcCbBbFyWsly\n" +
            "rSMkJihFAWU2MVKqvJ74YI3uNhFzqJ/AAUAPoet14q+ViYU+8a1lqEWj7y8foF3r\n" +
            "m0ZiQpuHULiYCO4y4NR7g5ijj6KsbruLv3e9NyUAIRBHOZEKOA7EiFmWJgqH1aZv\n" +
            "/eS7aQ9HMtPKrlbEwUjV0P3K2U2ljs0rNvO8KO9NKQmocXaRpLm+s8PYBGxby92j\n" +
            "5eelLq55028BSzhJJc6G+cRT9Hlxf1cg2qtqcVJa8i8wc2upCaGycZIlBSX4gj/4\n" +
            "k9faY4qGuGnuEdzAyvIXWMSkb8jiNHQfZrebSr00vShkUEKOLmfFHbkwIaWNK0+2\n" +
            "2c3RL4tDnM5u0kvdgWf0B742JskkxqqmEeZVofsOZJLOhXxO9NO/S0hM16/vf/tl\n" +
            "Tnsnhv0nxUR0B9wxN7XmWmq4\n" +
            "-----END CERTIFICATE-----\n" +
            "-----BEGIN CERTIFICATE-----\n" +
            "MIIGYTCCBEmgAwIBAgIBATANBgkqhkiG9w0BAQwFADB9MQswCQYDVQQGEwJVUzEc\n" +
            "MBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEvMC0GA1UECxMmTW96aWxsYSBB\n" +
            "TU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2UxHzAdBgNVBAMTFnJvb3QtY2Et\n" +
            "cHJvZHVjdGlvbi1hbW8wHhcNMTUwMzE3MjI1MzU3WhcNMjUwMzE0MjI1MzU3WjB9\n" +
            "MQswCQYDVQQGEwJVUzEcMBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEvMC0G\n" +
            "A1UECxMmTW96aWxsYSBBTU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2UxHzAd\n" +
            "BgNVBAMTFnJvb3QtY2EtcHJvZHVjdGlvbi1hbW8wggIgMA0GCSqGSIb3DQEBAQUA\n" +
            "A4ICDQAwggIIAoICAQC0u2HXXbrwy36+MPeKf5jgoASMfMNz7mJWBecJgvlTf4hH\n" +
            "JbLzMPsIUauzI9GEpLfHdZ6wzSyFOb4AM+D1mxAWhuZJ3MDAJOf3B1Rs6QorHrl8\n" +
            "qqlNtPGqepnpNJcLo7JsSqqE3NUm72MgqIHRgTRsqUs+7LIPGe7262U+N/T0LPYV\n" +
            "Le4rZ2RDHoaZhYY7a9+49mHOI/g2YFB+9yZjE+XdplT2kBgA4P8db7i7I0tIi4b0\n" +
            "B0N6y9MhL+CRZJyxdFe2wBykJX14LsheKsM1azHjZO56SKNrW8VAJTLkpRxCmsiT\n" +
            "r08fnPyDKmaeZ0BtsugicdipcZpXriIGmsZbI12q5yuwjSELdkDV6Uajo2n+2ws5\n" +
            "uXrP342X71WiWhC/dF5dz1LKtjBdmUkxaQMOP/uhtXEKBrZo1ounDRQx1j7+SkQ4\n" +
            "BEwjB3SEtr7XDWGOcOIkoJZWPACfBLC3PJCBWjTAyBlud0C5n3Cy9regAAnOIqI1\n" +
            "t16GU2laRh7elJ7gPRNgQgwLXeZcFxw6wvyiEcmCjOEQ6PM8UQjthOsKlszMhlKw\n" +
            "vjyOGDoztkqSBy/v+Asx7OW2Q7rlVfKarL0mREZdSMfoy3zTgtMVCM0vhNl6zcvf\n" +
            "5HNNopoEdg5yuXo2chZ1p1J+q86b0G5yJRMeT2+iOVY2EQ37tHrqUURncCy4uwIB\n" +
            "A6OB7TCB6jAMBgNVHRMEBTADAQH/MA4GA1UdDwEB/wQEAwIBBjAWBgNVHSUBAf8E\n" +
            "DDAKBggrBgEFBQcDAzCBkgYDVR0jBIGKMIGHoYGBpH8wfTELMAkGA1UEBhMCVVMx\n" +
            "HDAaBgNVBAoTE01vemlsbGEgQ29ycG9yYXRpb24xLzAtBgNVBAsTJk1vemlsbGEg\n" +
            "QU1PIFByb2R1Y3Rpb24gU2lnbmluZyBTZXJ2aWNlMR8wHQYDVQQDExZyb290LWNh\n" +
            "LXByb2R1Y3Rpb24tYW1vggEBMB0GA1UdDgQWBBSzvOpYdKvhbngqsqucIx6oYyyX\n" +
            "tzANBgkqhkiG9w0BAQwFAAOCAgEAaNSRYAaECAePQFyfk12kl8UPLh8hBNidP2H6\n" +
            "KT6O0vCVBjxmMrwr8Aqz6NL+TgdPmGRPDDLPDpDJTdWzdj7khAjxqWYhutACTew5\n" +
            "eWEaAzyErbKQl+duKvtThhV2p6F6YHJ2vutu4KIciOMKB8dslIqIQr90IX2Usljq\n" +
            "8Ttdyf+GhUmazqLtoB0GOuESEqT4unX6X7vSGu1oLV20t7t5eCnMMYD67ZBn0YIU\n" +
            "/cm/+pan66hHrja+NeDGF8wabJxdqKItCS3p3GN1zUGuJKrLykxqbOp/21byAGog\n" +
            "Z1amhz6NHUcfE6jki7sM7LHjPostU5ZWs3PEfVVgha9fZUhOrIDsyXEpCWVa3481\n" +
            "LlAq3GiUMKZ5DVRh9/Nvm4NwrTfB3QkQQJCwfXvO9pwnPKtISYkZUqhEqvXk5nBg\n" +
            "QCkDSLDjXTx39naBBGIVIqBtKKuVTla9enngdq692xX/CgO6QJVrwpqdGjebj5P8\n" +
            "5fNZPABzTezG3Uls5Vp+4iIWVAEDkK23cUj3c/HhE+Oo7kxfUeu5Y1ZV3qr61+6t\n" +
            "ZARKjbu1TuYQHf0fs+GwID8zeLc2zJL7UzcHFwwQ6Nda9OJN4uPAuC/BKaIpxCLL\n" +
            "26b24/tRam4SJjqpiq20lynhUrmTtt6hbG3E1Hpy3bmkt2DYnuMFwEx2gfXNcnbT\n" +
            "wNuvFqc=\n" +
            "-----END CERTIFICATE-----"

        val calendar = Calendar.getInstance()
        calendar.set(Calendar.YEAR, 2028)
        calendar.set(Calendar.MONTH, 6)
        calendar.set(Calendar.DAY_OF_MONTH, 13)
        testSignature(metadataBody, certChainBody, false, calendar.time)
    }

    @Test(expected = ExperimentDownloadException::class)
    fun validSignatureCertificateNotYetValid() {
        val url = server.url("/").url().toString()
        val metadataBody = """
            {
              "data": {
                "signature": {
                  "x5u": "$url",
                  "signature": "kRhyWZdLyjligYHSFhzhbyzUXBoUwoTPvyt9V0e-E7LKGgUYF2MVfqpA2zfIEDdqrImcMABVGHLUx9Nk614zciRBQ-gyaKA5SL2pPdZvoQXk_LLsPhEBgG4VDnxG4SBL"
                }
              }
            }
        """.trimIndent()
        val certChainBody = "-----BEGIN CERTIFICATE-----\n" +
            "MIIEpTCCBCygAwIBAgIEAQAAKTAKBggqhkjOPQQDAzCBpjELMAkGA1UEBhMCVVMx\n" +
            "HDAaBgNVBAoTE01vemlsbGEgQ29ycG9yYXRpb24xLzAtBgNVBAsTJk1vemlsbGEg\n" +
            "QU1PIFByb2R1Y3Rpb24gU2lnbmluZyBTZXJ2aWNlMSUwIwYDVQQDExxDb250ZW50\n" +
            "IFNpZ25pbmcgSW50ZXJtZWRpYXRlMSEwHwYJKoZIhvcNAQkBFhJmb3hzZWNAbW96\n" +
            "aWxsYS5jb20wIhgPMjAxODAyMTgwMDAwMDBaGA8yMDE4MDYxODAwMDAwMFowgbEx\n" +
            "CzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpDYWxpZm9ybmlhMRwwGgYDVQQKExNNb3pp\n" +
            "bGxhIENvcnBvcmF0aW9uMRcwFQYDVQQLEw5DbG91ZCBTZXJ2aWNlczExMC8GA1UE\n" +
            "AxMoZmVubmVjLWRsYy5jb250ZW50LXNpZ25hdHVyZS5tb3ppbGxhLm9yZzEjMCEG\n" +
            "CSqGSIb3DQEJARYUc2VjdXJpdHlAbW96aWxsYS5vcmcwdjAQBgcqhkjOPQIBBgUr\n" +
            "gQQAIgNiAATpKfWqAyDsh2ISzBycb8Y7JqLygByKI9vI2WZ2VWaGTYfmB1tQ8PFj\n" +
            "vZrtDqZeO8Dhs2KiMrvs/uoziM2zselYQhd0mz5z3dMui6BD6SPKB83K7xn2r2mW\n" +
            "plAZxuwnPKujggIYMIICFDAdBgNVHQ4EFgQUTp9+0KSp+vkzbEcXAENBCIyI9eow\n" +
            "gaoGA1UdIwSBojCBn4AUiHVymVvwUPJguD2xCZYej3l5nu6hgYGkfzB9MQswCQYD\n" +
            "VQQGEwJVUzEcMBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEvMC0GA1UECxMm\n" +
            "TW96aWxsYSBBTU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2UxHzAdBgNVBAMT\n" +
            "FnJvb3QtY2EtcHJvZHVjdGlvbi1hbW+CAxAABjAMBgNVHRMBAf8EAjAAMA4GA1Ud\n" +
            "DwEB/wQEAwIHgDAWBgNVHSUBAf8EDDAKBggrBgEFBQcDAzBFBgNVHR8EPjA8MDqg\n" +
            "OKA2hjRodHRwczovL2NvbnRlbnQtc2lnbmF0dXJlLmNkbi5tb3ppbGxhLm5ldC9j\n" +
            "YS9jcmwucGVtMEMGCWCGSAGG+EIBBAQ2FjRodHRwczovL2NvbnRlbnQtc2lnbmF0\n" +
            "dXJlLmNkbi5tb3ppbGxhLm5ldC9jYS9jcmwucGVtME8GCCsGAQUFBwEBBEMwQTA/\n" +
            "BggrBgEFBQcwAoYzaHR0cHM6Ly9jb250ZW50LXNpZ25hdHVyZS5jZG4ubW96aWxs\n" +
            "YS5uZXQvY2EvY2EucGVtMDMGA1UdEQQsMCqCKGZlbm5lYy1kbGMuY29udGVudC1z\n" +
            "aWduYXR1cmUubW96aWxsYS5vcmcwCgYIKoZIzj0EAwMDZwAwZAIwXVe1A+r/yVwR\n" +
            "DtecWq2DOOIIbq6jPzz/L6GpAw1KHnVMVBnOyrsFNmPyGQb1H60bAjB0dxyh14JB\n" +
            "FCdPO01+y8I7nGRaWqjkmp/GLrdoginSppQYZInYTZagcfy/nIr5fKk=\n" +
            "-----END CERTIFICATE-----\n" +
            "-----BEGIN CERTIFICATE-----\n" +
            "MIIFfjCCA2agAwIBAgIDEAAGMA0GCSqGSIb3DQEBDAUAMH0xCzAJBgNVBAYTAlVT\n" +
            "MRwwGgYDVQQKExNNb3ppbGxhIENvcnBvcmF0aW9uMS8wLQYDVQQLEyZNb3ppbGxh\n" +
            "IEFNTyBQcm9kdWN0aW9uIFNpZ25pbmcgU2VydmljZTEfMB0GA1UEAxMWcm9vdC1j\n" +
            "YS1wcm9kdWN0aW9uLWFtbzAeFw0xNzA1MDQwMDEyMzlaFw0xOTA1MDQwMDEyMzla\n" +
            "MIGmMQswCQYDVQQGEwJVUzEcMBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEv\n" +
            "MC0GA1UECxMmTW96aWxsYSBBTU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2Ux\n" +
            "JTAjBgNVBAMTHENvbnRlbnQgU2lnbmluZyBJbnRlcm1lZGlhdGUxITAfBgkqhkiG\n" +
            "9w0BCQEWEmZveHNlY0Btb3ppbGxhLmNvbTB2MBAGByqGSM49AgEGBSuBBAAiA2IA\n" +
            "BMCmt4C33KfMzsyKokc9SXmMSxozksQglhoGAA1KjlgqEOzcmKEkxtvnGWOA9FLo\n" +
            "A6U7Wmy+7sqmvmjLboAPQc4G0CEudn5Nfk36uEqeyiyKwKSAT+pZsqS4/maXIC7s\n" +
            "DqOCAYkwggGFMAwGA1UdEwQFMAMBAf8wDgYDVR0PAQH/BAQDAgEGMBYGA1UdJQEB\n" +
            "/wQMMAoGCCsGAQUFBwMDMB0GA1UdDgQWBBSIdXKZW/BQ8mC4PbEJlh6PeXme7jCB\n" +
            "qAYDVR0jBIGgMIGdgBSzvOpYdKvhbngqsqucIx6oYyyXt6GBgaR/MH0xCzAJBgNV\n" +
            "BAYTAlVTMRwwGgYDVQQKExNNb3ppbGxhIENvcnBvcmF0aW9uMS8wLQYDVQQLEyZN\n" +
            "b3ppbGxhIEFNTyBQcm9kdWN0aW9uIFNpZ25pbmcgU2VydmljZTEfMB0GA1UEAxMW\n" +
            "cm9vdC1jYS1wcm9kdWN0aW9uLWFtb4IBATAzBglghkgBhvhCAQQEJhYkaHR0cDov\n" +
            "L2FkZG9ucy5hbGxpem9tLm9yZy9jYS9jcmwucGVtME4GA1UdHgRHMEWgQzAggh4u\n" +
            "Y29udGVudC1zaWduYXR1cmUubW96aWxsYS5vcmcwH4IdY29udGVudC1zaWduYXR1\n" +
            "cmUubW96aWxsYS5vcmcwDQYJKoZIhvcNAQEMBQADggIBAKWhLjJB8XmW3VfLvyLF\n" +
            "OOUNeNs7Aju+EZl1PMVXf+917LB//FcJKUQLcEo86I6nC3umUNl+kaq4d3yPDpMV\n" +
            "4DKLHgGmegRsvAyNFQfd64TTxzyfoyfNWH8uy5vvxPmLvWb+jXCoMNF5FgFWEVon\n" +
            "5GDEK8hoHN/DMVe0jveeJhUSuiUpJhMzEf6Vbo0oNgfaRAZKO+VOY617nkTOPnVF\n" +
            "LSEcUPIdE8pcd+QP1t/Ysx+mAfkxAbt+5K298s2bIRLTyNUj1eBtTcCbBbFyWsly\n" +
            "rSMkJihFAWU2MVKqvJ74YI3uNhFzqJ/AAUAPoet14q+ViYU+8a1lqEWj7y8foF3r\n" +
            "m0ZiQpuHULiYCO4y4NR7g5ijj6KsbruLv3e9NyUAIRBHOZEKOA7EiFmWJgqH1aZv\n" +
            "/eS7aQ9HMtPKrlbEwUjV0P3K2U2ljs0rNvO8KO9NKQmocXaRpLm+s8PYBGxby92j\n" +
            "5eelLq55028BSzhJJc6G+cRT9Hlxf1cg2qtqcVJa8i8wc2upCaGycZIlBSX4gj/4\n" +
            "k9faY4qGuGnuEdzAyvIXWMSkb8jiNHQfZrebSr00vShkUEKOLmfFHbkwIaWNK0+2\n" +
            "2c3RL4tDnM5u0kvdgWf0B742JskkxqqmEeZVofsOZJLOhXxO9NO/S0hM16/vf/tl\n" +
            "Tnsnhv0nxUR0B9wxN7XmWmq4\n" +
            "-----END CERTIFICATE-----\n" +
            "-----BEGIN CERTIFICATE-----\n" +
            "MIIGYTCCBEmgAwIBAgIBATANBgkqhkiG9w0BAQwFADB9MQswCQYDVQQGEwJVUzEc\n" +
            "MBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEvMC0GA1UECxMmTW96aWxsYSBB\n" +
            "TU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2UxHzAdBgNVBAMTFnJvb3QtY2Et\n" +
            "cHJvZHVjdGlvbi1hbW8wHhcNMTUwMzE3MjI1MzU3WhcNMjUwMzE0MjI1MzU3WjB9\n" +
            "MQswCQYDVQQGEwJVUzEcMBoGA1UEChMTTW96aWxsYSBDb3Jwb3JhdGlvbjEvMC0G\n" +
            "A1UECxMmTW96aWxsYSBBTU8gUHJvZHVjdGlvbiBTaWduaW5nIFNlcnZpY2UxHzAd\n" +
            "BgNVBAMTFnJvb3QtY2EtcHJvZHVjdGlvbi1hbW8wggIgMA0GCSqGSIb3DQEBAQUA\n" +
            "A4ICDQAwggIIAoICAQC0u2HXXbrwy36+MPeKf5jgoASMfMNz7mJWBecJgvlTf4hH\n" +
            "JbLzMPsIUauzI9GEpLfHdZ6wzSyFOb4AM+D1mxAWhuZJ3MDAJOf3B1Rs6QorHrl8\n" +
            "qqlNtPGqepnpNJcLo7JsSqqE3NUm72MgqIHRgTRsqUs+7LIPGe7262U+N/T0LPYV\n" +
            "Le4rZ2RDHoaZhYY7a9+49mHOI/g2YFB+9yZjE+XdplT2kBgA4P8db7i7I0tIi4b0\n" +
            "B0N6y9MhL+CRZJyxdFe2wBykJX14LsheKsM1azHjZO56SKNrW8VAJTLkpRxCmsiT\n" +
            "r08fnPyDKmaeZ0BtsugicdipcZpXriIGmsZbI12q5yuwjSELdkDV6Uajo2n+2ws5\n" +
            "uXrP342X71WiWhC/dF5dz1LKtjBdmUkxaQMOP/uhtXEKBrZo1ounDRQx1j7+SkQ4\n" +
            "BEwjB3SEtr7XDWGOcOIkoJZWPACfBLC3PJCBWjTAyBlud0C5n3Cy9regAAnOIqI1\n" +
            "t16GU2laRh7elJ7gPRNgQgwLXeZcFxw6wvyiEcmCjOEQ6PM8UQjthOsKlszMhlKw\n" +
            "vjyOGDoztkqSBy/v+Asx7OW2Q7rlVfKarL0mREZdSMfoy3zTgtMVCM0vhNl6zcvf\n" +
            "5HNNopoEdg5yuXo2chZ1p1J+q86b0G5yJRMeT2+iOVY2EQ37tHrqUURncCy4uwIB\n" +
            "A6OB7TCB6jAMBgNVHRMEBTADAQH/MA4GA1UdDwEB/wQEAwIBBjAWBgNVHSUBAf8E\n" +
            "DDAKBggrBgEFBQcDAzCBkgYDVR0jBIGKMIGHoYGBpH8wfTELMAkGA1UEBhMCVVMx\n" +
            "HDAaBgNVBAoTE01vemlsbGEgQ29ycG9yYXRpb24xLzAtBgNVBAsTJk1vemlsbGEg\n" +
            "QU1PIFByb2R1Y3Rpb24gU2lnbmluZyBTZXJ2aWNlMR8wHQYDVQQDExZyb290LWNh\n" +
            "LXByb2R1Y3Rpb24tYW1vggEBMB0GA1UdDgQWBBSzvOpYdKvhbngqsqucIx6oYyyX\n" +
            "tzANBgkqhkiG9w0BAQwFAAOCAgEAaNSRYAaECAePQFyfk12kl8UPLh8hBNidP2H6\n" +
            "KT6O0vCVBjxmMrwr8Aqz6NL+TgdPmGRPDDLPDpDJTdWzdj7khAjxqWYhutACTew5\n" +
            "eWEaAzyErbKQl+duKvtThhV2p6F6YHJ2vutu4KIciOMKB8dslIqIQr90IX2Usljq\n" +
            "8Ttdyf+GhUmazqLtoB0GOuESEqT4unX6X7vSGu1oLV20t7t5eCnMMYD67ZBn0YIU\n" +
            "/cm/+pan66hHrja+NeDGF8wabJxdqKItCS3p3GN1zUGuJKrLykxqbOp/21byAGog\n" +
            "Z1amhz6NHUcfE6jki7sM7LHjPostU5ZWs3PEfVVgha9fZUhOrIDsyXEpCWVa3481\n" +
            "LlAq3GiUMKZ5DVRh9/Nvm4NwrTfB3QkQQJCwfXvO9pwnPKtISYkZUqhEqvXk5nBg\n" +
            "QCkDSLDjXTx39naBBGIVIqBtKKuVTla9enngdq692xX/CgO6QJVrwpqdGjebj5P8\n" +
            "5fNZPABzTezG3Uls5Vp+4iIWVAEDkK23cUj3c/HhE+Oo7kxfUeu5Y1ZV3qr61+6t\n" +
            "ZARKjbu1TuYQHf0fs+GwID8zeLc2zJL7UzcHFwwQ6Nda9OJN4uPAuC/BKaIpxCLL\n" +
            "26b24/tRam4SJjqpiq20lynhUrmTtt6hbG3E1Hpy3bmkt2DYnuMFwEx2gfXNcnbT\n" +
            "wNuvFqc=\n" +
            "-----END CERTIFICATE-----"

        val calendar = Calendar.getInstance()
        calendar.set(Calendar.YEAR, 2010)
        calendar.set(Calendar.MONTH, 6)
        calendar.set(Calendar.DAY_OF_MONTH, 13)
        testSignature(metadataBody, certChainBody, false, calendar.time)
    }

    private fun testSignature(metadataBody: String, certChainBody: String, expected: Boolean, currentDate: Date = defaultDate()) {
        val experimentsJsonText = """
            {
              "data": [
                {
                  "id": "leanplum-start",
                  "description": "Enable Leanplum SDK - Bug 1351571 \nExpand English Users to more region - Bug 1411066\nEnable  50% eng|zho|deu globally for Leanplum. see https://bugzilla.mozilla.org/show_bug.cgi?id=1411066#c8",
                  "last_modified": 1523549895713,
                  "schema": 1523549592861,
                  "match": {
                    "lang": "eng|zho|deu|fra|ita|ind|por|spa|pol|rus",
                    "appId": "^org.mozilla.firefox_beta${'$'}|^org.mozilla.firefox${'$'}",
                    "regions": []
                  },
                  "buckets": {
                    "start": "0",
                    "count": "100"
                  },
                  "branches": [
                    {
                      "name": "control",
                      "ratio": 5
                    },
                    {
                      "name": "red-pill",
                      "ratio": 21
                    }
                  ]
                },
                {
                  "id": "5e23b482-8800-47be-b6dc-1a3bb6e455d4",
                  "name": "cusstom-tabs",
                  "id": "custom-tabs",
                  "description": "Allows apps to open tabs in a customized UI.",
                  "last_modified": 1510211043874,
                  "match": {
                    "regions": []
                  },
                  "schema": 1510207707840,
                  "buckets": {
                    "start": "0",
                    "count": "100"
                  },
                  "branches": [
                    {
                      "name": "some-branch",
                      "ratio": 2
                    }
                  ]
                },
                {
                  "id": "activity-stream-opt-out",
                  "description": "Enable Activity stream by default for users in the \"opt out\" group.",
                  "last_modified": 1500969355986,
                  "schema": 1498764179980,
                  "match": {
                    "appId": "^org.mozilla.fennec.*${'$'}",
                    "regions": []
                  },
                  "buckets": {
                    "start": "0",
                    "count": "100"
                  },
                  "branches": [
                    {
                      "name": "some-branch",
                      "ratio": 2
                    }
                  ]
                },
                {
                  "id": "full-bookmark-management",
                  "description": "Bug 1232439 - Show full-page edit bookmark dialog",
                  "last_modified": 1498690258010,
                  "schema": 1480618438089,
                  "match": {
                    "appId": "^org.mozilla.fennec.*${'$'}"
                  },
                  "buckets": {
                    "start": "0",
                    "count": "100"
                  },
                  "branches": [
                    {
                      "name": "some-branch",
                      "ratio": 2
                    }
                  ]
                },
                {
                  "id": "top-addons-menu",
                  "description": "Show addon menu item in top-level.",
                  "last_modified": 1498599522440,
                  "match": {},
                  "schema": 1480618438089,
                  "buckets": {
                    "start": "0",
                    "count": "100"
                  },
                  "branches": [
                    {
                      "name": "some-branch",
                      "ratio": 2
                    }
                  ]
                },
                {
                  "id": "offline-cache",
                  "description": "",
                  "last_modified": 1497643056372,
                  "schema": 1480618438089,
                  "match": {
                    "appId": "^org.mozilla.fennec|org.mozilla.firefox_beta"
                  },
                  "buckets": {
                    "start": "0",
                    "count": "100"
                  },
                  "branches": [
                    {
                      "name": "some-branch",
                      "ratio": 2
                    }
                  ]
                },
                {
                  "id": "process-background-telemetry",
                  "description": "Gate flag for controlling if background telemetry processing (sync ping) is enabled or not.",
                  "last_modified": 1496971360625,
                  "schema": 1480618438089,
                  "match": {},
                  "buckets": {
                    "start": "0",
                    "count": "100"
                  },
                  "branches": [
                    {
                      "name": "some-branch",
                      "ratio": 2
                    }
                  ]
                },
                {
                  "id": "activity-stream-setting",
                  "description": "Show a setting in \"experimental features\" for enabling/disabling activity stream.",
                  "last_modified": 1496331790186,
                  "schema": 1480618438089,
                  "match": {
                    "appId": "^org.mozilla.fennec.*${'$'}"
                  },
                  "buckets": {
                    "start": "0",
                    "count": "100"
                  },
                  "branches": [
                    {
                      "name": "some-branch",
                      "ratio": 2
                    }
                  ]
                },
                {
                  "id": "activity-stream",
                  "description": "Enable/Disable Activity Stream",
                  "last_modified": 1496331773809,
                  "schema": 1480618438089,
                  "match": {
                    "appId": "^org.mozilla.fennec.*${'$'}"
                  },
                  "buckets": {
                    "start": "0",
                    "count": "100"
                  },
                  "branches": [
                    {
                      "name": "some-branch",
                      "ratio": 2
                    }
                  ]
                },
                {
                  "id": "download-content-catalog-sync",
                  "description": "",
                  "last_modified": 1485853244635,
                  "schema": 1480618438089,
                  "match": {
                    "appId": ""
                  },
                  "buckets": {
                    "start": "0",
                    "count": "100"
                  },
                  "branches": [
                    {
                      "name": "some-branch",
                      "ratio": 2
                    }
                  ]
                },
                {
                  "id": "promote-add-to-homescreen",
                  "description": "",
                  "last_modified": 1482264639326,
                  "schema": 1480618438089,
                  "match": {
                    "appId": "^org.mozilla.fennec.*${'$'}|^org.mozilla.firefox_beta${'$'}"
                  },
                  "buckets": {
                    "start": "50",
                    "count": "50"
                  },
                  "branches": [
                    {
                      "name": "some-branch",
                      "ratio": 2
                    }
                  ]
                },
                {
                  "id": "triple-readerview-bookmark-prompt",
                  "description": "",
                  "last_modified": 1482262302021,
                  "schema": 1480618438089,
                  "match": {
                    "appId": "^org.mozilla.fennec.*${'$'}|^org.mozilla.firefox_beta${'$'}"
                  },
                  "buckets": {
                    "start": "0",
                    "count": "100"
                  },
                  "branches": [
                    {
                      "name": "some-branch",
                      "ratio": 2
                    }
                  ]
                },
                {
                  "id": "compact-tabs",
                  "description": "Arrange tabs in two columns in portrait mode (tabs tray)",
                  "last_modified": 1482242613284,
                  "schema": 1480618438089,
                  "match": {},
                  "buckets": {
                    "start": "0",
                    "count": "50"
                  },
                  "branches": [
                    {
                      "name": "some-branch",
                      "ratio": 2
                    }
                  ]
                },
                {
                  "id": "hls-video-playback",
                  "description": "",
                  "last_modified": 1477907551487,
                  "schema": 1467794476773,
                  "match": {},
                  "buckets": {
                    "start": "0",
                    "count": "100"
                  },
                  "branches": [
                    {
                      "name": "some-branch",
                      "ratio": 2
                    }
                  ]
                },
                {
                  "id": "bookmark-history-menu",
                  "description": "",
                  "last_modified": 1467794477013,
                  "match": {},
                  "buckets": {
                    "start": "0",
                    "count": "100"
                  },
                  "branches": [
                    {
                      "name": "some-branch",
                      "ratio": 2
                    }
                  ]
                },
                {
                  "id": "content-notifications-12hrs",
                  "description": "",
                  "last_modified": 1467794476938,
                  "match": {},
                  "buckets": {
                    "start": "0",
                    "count": "0"
                  },
                  "branches": [
                    {
                      "name": "some-branch",
                      "ratio": 2
                    }
                  ]
                },
                {
                  "id": "whatsnew-notification",
                  "description": "",
                  "last_modified": 1467794476893,
                  "match": {},
                  "buckets": {
                    "start": "0",
                    "count": "0"
                  },
                  "branches": [
                    {
                      "name": "some-branch",
                      "ratio": 2
                    }
                  ]
                },
                {
                  "id": "content-notifications-8am",
                  "description": "",
                  "last_modified": 1467794476875,
                  "match": {},
                  "buckets": {
                    "start": "0",
                    "count": "0"
                  },
                  "branches": [
                    {
                      "name": "some-branch",
                      "ratio": 2
                    }
                  ]
                },
                {
                  "id": "content-notifications-5pm",
                  "description": "",
                  "last_modified": 1467794476838,
                  "match": {},
                  "buckets": {
                    "start": "0",
                    "count": "0"
                  },
                  "branches": [
                    {
                      "name": "some-branch",
                      "ratio": 2
                    }
                  ]
                }
              ],
              "last_modified": "1523549895713"
            }
        """.trimIndent()

        val url = server.url("/").url().toString()
        server.setDispatcher(object : Dispatcher() {
            override fun dispatch(request: RecordedRequest): MockResponse {
                if (request.path.contains("buckets/testbucket/collections/testcollection")) {
                    return MockResponse().setBody(metadataBody)
                }
                return MockResponse().setBody(certChainBody)
            }
        })
        val experimentsJSON = JSONObject(experimentsJsonText)
        val experimentsJSONArray = experimentsJSON.getJSONArray("data")
        val experiments = mutableListOf<Experiment>()
        val parser = JSONExperimentParser()
        for (i in 0 until experimentsJSONArray.length()) {
            experiments.add(parser.fromJson(experimentsJSONArray[i] as JSONObject))
        }
        val client = HttpURLConnectionClient()
        val verifier = SignatureVerifier(client, KintoClient(client, url, "testbucket", "testcollection"), currentDate)
        assertEquals(expected, verifier.validSignature(experiments, experimentsJSON.getLong("last_modified")))
        server.shutdown()
    }
}
