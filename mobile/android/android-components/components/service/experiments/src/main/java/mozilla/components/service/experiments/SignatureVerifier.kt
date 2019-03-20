/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import android.util.Base64
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import org.json.JSONArray
import org.json.JSONObject
import java.io.ByteArrayInputStream
import java.math.BigInteger
import java.nio.charset.StandardCharsets
import java.security.InvalidKeyException
import java.security.NoSuchAlgorithmException
import java.security.NoSuchProviderException
import java.security.PublicKey
import java.security.Signature
import java.security.SignatureException
import java.security.cert.CertificateException
import java.security.cert.CertificateFactory
import java.security.cert.X509Certificate
import java.util.Date
import java.util.concurrent.TimeUnit

/**
 * This class is used to validate the signature of the downloaded list of experiments
 */
internal class SignatureVerifier(
    private val client: Client,
    private val kintoClient: KintoClient,
    private val currentDate: Date = Date()
) {
    /**
     * Checks the signature of an experiment list against the signature found on the
     * metadata JSON object, using the certificates found on the x5u field
     *
     * @param experiments experiment list
     * @param lastModified last modified time of the experiment list
     *
     * @return true if the list of experiments validates against the signature, false otherwise
     */
    internal fun validSignature(experiments: List<Experiment>, lastModified: Long?): Boolean {
        val sortedExperiments = experiments.sortedBy { it.id }
        val resultJson = JSONArray()
        val parser = JSONExperimentParser()
        for (experiment in sortedExperiments) {
            resultJson.put(parser.toJson(experiment))
        }
        val metadata: String? = kintoClient.getMetadata()
        metadata?.let {
            val metadataJson = JSONObject(metadata).getJSONObject(DATA_KEY)
            val signatureJson = metadataJson.getJSONObject(SIGNATURE_KEY)
            val signature = signatureJson.getString(SIGNATURE_KEY)
            val publicKey = getX5U(signatureJson.getString(X5U_KEY))
            val resultJsonString = resultJson.toString().replace("\\/", "/")
            val data = "$SIGNATURE_PREFIX{\"data\":$resultJsonString,\"last_modified\":\"$lastModified\"}"
            return validSignature(data, signature, publicKey)
        }
        return true
    }

    /**
     * Retrieves the public key from the end-entity certificate from the certificate chain
     * located on the given URL from the X5U field
     *
     * @param url url of the certificate chain
     *
     * @throws ExperimentDownloadException if the certificate chain format is invalid
     *
     * @return public key of the end-entity certificate
     */
    private fun getX5U(url: String): PublicKey {
        val certs = ArrayList<X509Certificate>()
        val cf = CertificateFactory.getInstance("X.509")
        val response = client.fetch(Request(url))
        response.body.useBufferedReader {
            val firstLine = it.readLine()
            if (firstLine != "-----BEGIN CERTIFICATE-----") {
                throw ExperimentDownloadException("")
            }
            var certPem = firstLine
            certPem += '\n'

            it.readLines().forEach { line ->
                certPem += line
                certPem += '\n'
                if (line == "-----END CERTIFICATE-----") {
                    val cert = cf.generateCertificate(ByteArrayInputStream(certPem.toByteArray()))
                    certs.add(cert as X509Certificate)
                    certPem = ""
                }
            }
            if (certs.count() < MIN_CERTIFICATES) {
                throw ExperimentDownloadException("The chain must contain at least 2 certificates")
            }
            verifyCertChain(certs)
        }
        return certs[0].publicKey
    }

    /**
     * Verifies the validity of the certificates on the chain
     *
     * @param certificates certificates
     *
     * @throws ExperimentDownloadException if any certificate is not valid
     */
    private fun verifyCertChain(certificates: List<X509Certificate>) {
        for (i in 0 until certificates.count()) {
            val cert = certificates[i]
            if (!isCertValid(cert)) {
                throw ExperimentDownloadException("Certificate expired or not yet valid")
            }
            if ((i + 1) == certificates.count()) {
                verifyRoot(cert)
            } else {
                verifyCertSignedByParent(cert, i, certificates)
            }
        }
    }

    /**
     * Verifies that the certificate is signed by its parent on the chain
     *
     * @param certificate certificate
     * @param index index of the certificate on the chain
     * @param certificates certificate chain
     *
     * @throws ExperimentDownloadException if the certificate chain is invalid
     *
     * @return true if the certificate is signed by its parent on the chain, false otherwise
     */
    private fun verifyCertSignedByParent(
        certificate: X509Certificate,
        index: Int,
        certificates: List<X509Certificate>
    ) {
        try {
            certificate.verify(certificates[index + 1].publicKey)
        } catch (e: CertificateException) {
            invalidCertChain(e)
        } catch (e: NoSuchAlgorithmException) {
            invalidCertChain(e)
        } catch (e: InvalidKeyException) {
            invalidCertChain(e)
        } catch (e: NoSuchProviderException) {
            invalidCertChain(e)
        } catch (e: SignatureException) {
            invalidCertChain(e)
        }
    }

    private fun invalidCertChain(e: Exception) {
        throw ExperimentDownloadException(e)
    }

    /**
     * Checks the certificate validity. It checks against a window of 30 days
     *
     * @param certificate certificate to check
     *
     * @return true if the certificate is still valid (with a 30-day window), false otherwise
     */
    private fun isCertValid(certificate: X509Certificate): Boolean {
        val notBefore = certificate.notBefore
        val notAfter = certificate.notAfter
        return currentDate.time >= (notBefore.time - TimeUnit.DAYS.toMillis(CERT_VALIDITY_ROOM_DAYS)) &&
            (notAfter.time + TimeUnit.DAYS.toMillis(CERT_VALIDITY_ROOM_DAYS)) >= currentDate.time
    }

    /**
     * Verifies the root certificate of the chain, checking that the issuer matches the subject
     *
     * @param certificate root certificate
     *
     * @throws ExperimentDownloadException if the root certificate fails the verification
     */
    private fun verifyRoot(certificate: X509Certificate) {
        val subject = certificate.subjectDN.name
        val issuer = certificate.issuerDN.name
        if (subject != issuer) {
            throw ExperimentDownloadException("subject does not match issuer")
        }
    }

    /**
     * Validates the signature of a signed data, given a signature and a public key using ECDSA
     * with curve P-384 and SHA-384

     * @param signedData signed data
     * @param signature signature
     * @param publicKey public key
     *
     * @return true if the signed data validates against the signature
     */
    private fun validSignature(signedData: String, signature: String, publicKey: PublicKey): Boolean {
        val dsa = Signature.getInstance("SHA384withECDSA")
        dsa.initVerify(publicKey)
        dsa.update(signedData.toByteArray(StandardCharsets.UTF_8))
        val signatureBytes = Base64.decode(signature.replace("-", "+").replace("_", "/"), 0)
        return dsa.verify(signatureToASN1(signatureBytes))
    }

    /**
     * Converts signature bytes into the ASN1 DER format. The provided bytes contain r and s values
     * concatenated, and the implementation converts these two values into the DER format

     * @param signatureBytes signatures bytes, r and s values concatenated
     *
     * @throws ExperimentDownloadException if the signature is not valid
     *
     * @return DER-encoded signature bytes
     */
    private fun signatureToASN1(signatureBytes: ByteArray): ByteArray {
        if (signatureBytes.count() == 0 || signatureBytes.count() % 2 != 0) {
            throw ExperimentDownloadException("Invalid signature")
        }
        var rBytes = ByteArray(signatureBytes.count() / 2)
        for (i in 0 until signatureBytes.count() / 2) {
            rBytes += signatureBytes[i]
        }
        var sBytes = ByteArray(signatureBytes.count() / 2)
        for (i in signatureBytes.count() / 2 until signatureBytes.count()) {
            sBytes += signatureBytes[i]
        }
        val r = BigInteger(rBytes)
        val s = BigInteger(sBytes)
        rBytes = r.toByteArray()
        sBytes = s.toByteArray()
        val res = ByteArray(NUMBER_OF_DER_ADDITIONAL_BYTES + rBytes.size + sBytes.size)
        res[START_POS] = FIRST_DER_NUMBER
        res[B1_POS] = (REMAINING_DER_ADDITIONAL_BYTES + rBytes.size + sBytes.size).toByte()
        res[SECOND_NUMBER_POS] = SECOND_DER_NUMBER
        res[B2_POS] = rBytes.size.toByte()
        System.arraycopy(rBytes, START_POS, res, R_POS, rBytes.size)
        res[R_POS + rBytes.size] = SECOND_DER_NUMBER
        res[B3_POS + rBytes.size] = sBytes.size.toByte()
        System.arraycopy(sBytes, START_POS, res, S_POS + rBytes.size, sBytes.size)
        return res
    }

    companion object {
        private const val DATA_KEY = "data"
        private const val SIGNATURE_KEY = "signature"
        private const val X5U_KEY = "x5u"
        private const val CERT_VALIDITY_ROOM_DAYS = 30L
        private const val MIN_CERTIFICATES = 2
        private const val SIGNATURE_PREFIX = "Content-Signature:\u0000"
        private const val NUMBER_OF_DER_ADDITIONAL_BYTES = 6
        private const val FIRST_DER_NUMBER: Byte = 0x30
        private const val REMAINING_DER_ADDITIONAL_BYTES = 4
        private const val SECOND_DER_NUMBER: Byte = 0x02
        private const val START_POS = 0
        private const val B1_POS = 1
        private const val SECOND_NUMBER_POS = 2
        private const val B2_POS = 3
        private const val R_POS = 4
        private const val B3_POS = 5
        private const val S_POS = 6
    }
}
