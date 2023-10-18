/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics.fonts

import org.json.JSONException
import org.json.JSONObject
import java.io.DataInputStream
import java.io.FileInputStream
import java.io.IOException
import java.io.InputStream
import java.security.MessageDigest
import java.security.NoSuchAlgorithmException
import kotlin.math.min

/**
 * FontMetric represents the information about a Font File
 */
data class FontMetric(
    val path: String = "",
    val hash: String = "",
) {
    var family: String = ""
    var subFamily: String = ""
    var uniqueSubFamily: String = ""
    var fullName: String = ""
    var fontVersion: String = ""
    var revision: Int = -1
    var created: Long = -1L
    var modified: Long = -1L

    /**
     * Return a JSONObject of this Font's details
     */
    fun toJson(): JSONObject {
        val jsonObject = JSONObject()
        try {
            // Use abbreviations to make the json smaller
            jsonObject.put("F", family.replace("\u0000", ""))
            jsonObject.put("SF", subFamily.replace("\u0000", ""))
            jsonObject.put("USF", uniqueSubFamily.replace("\u0000", ""))
            jsonObject.put("FN", fullName.replace("\u0000", ""))
            jsonObject.put("V", fontVersion.replace("\u0000", ""))
            jsonObject.put("R", revision)
            jsonObject.put("C", created)
            jsonObject.put("M", modified)
            jsonObject.put("H", hash)
            jsonObject.put("P", path.replace("\u0000", ""))
        } catch (_: JSONException) {
        }
        return jsonObject
    }
}

/**
 * Parse a font, given via an InputStream, to extract the Font information
 * including Family, SubFamily, Revision, etc
 */
object FontParser {
    /**
     * Parse a font file and return a FontMetric object describing it
     */
    fun parse(path: String): FontMetric {
        return parse(path, FileInputStream(path))
    }

    /**
     * Parse a font file and return a FontMetric object describing it
     */
    fun parse(path: String, inputStream: InputStream): FontMetric {
        val hash = calculateFileHash(inputStream)
        val fontDetails = FontMetric(path, hash)

        inputStream.reset()
        readFontFile(inputStream, fontDetails)

        return fontDetails
    }

    @Suppress("MagicNumber")
    private fun readFontFile(inputStream: InputStream, fontDetails: FontMetric) {
        val file = DataInputStream(inputStream)
        val numFonts: Int
        val magicNumber = file.readInt()
        var bytesReadSoFar = 4

        if (magicNumber == 0x74746366) {
            // The Font File has a TTC Header
            val majorVersion = file.readUnsignedShort()
            file.skipBytes(2) // Minor Version
            numFonts = file.readInt()
            bytesReadSoFar += 8

            file.skipBytes(4 * numFonts) // OffsetTable
            bytesReadSoFar += 4 * numFonts
            if (majorVersion == 2) {
                file.skipBytes(12)
                bytesReadSoFar += 12
            }

            file.skipBytes(4) // Magic Number for the Font
            bytesReadSoFar += 4
        }
        val numTables: Int = file.readUnsignedShort()
        bytesReadSoFar += 2
        file.skipBytes(6) // Rest of header
        bytesReadSoFar += 6

        // Find the head table
        var headOffset = 0
        var nameOffset = 0
        var nameLength = 0
        for (i in 0 until numTables) {
            val tableName =
                CharArray(4) {
                    file.readUnsignedByte().toChar()
                }
            file.skipBytes(4) // checksum
            val offset = file.readInt() // technically it's unsigned but we should be okay
            val length = file.readInt() // technically it's unsigned but we should be okay

            bytesReadSoFar += 16

            if (String(tableName) == "head") {
                headOffset = offset
            } else if (String(tableName) == "name") {
                nameOffset = offset
                nameLength = length
            }
        }

        if (headOffset == 0 || nameOffset == 0) {
            throw IOException("Could not find head or name table")
        }

        if (headOffset < nameOffset) {
            file.skipBytes(headOffset - bytesReadSoFar)
            bytesReadSoFar = headOffset
            bytesReadSoFar += readHeadTable(file, fontDetails)
            file.skipBytes(nameOffset - bytesReadSoFar)
            readNameTable(file, nameLength, fontDetails)
        } else {
            file.skipBytes(nameOffset - bytesReadSoFar)
            bytesReadSoFar = nameOffset
            bytesReadSoFar += readNameTable(file, nameLength, fontDetails)
            file.skipBytes(headOffset - bytesReadSoFar)
            readHeadTable(file, fontDetails)
        }
        file.close()
    }

    @Suppress("MagicNumber")
    private fun readHeadTable(file: DataInputStream, fontDetails: FontMetric): Int {
        // Find the details in the head table
        file.skipBytes(4) // Fixed version
        fontDetails.revision = file.readInt()
        file.skipBytes(12) // checksum, magic, flags, units
        fontDetails.created = file.readLong()
        fontDetails.modified = file.readLong()
        return 36
    }

    @Suppress("MagicNumber")
    private fun readNameTable(
        file: DataInputStream,
        tableLength: Int,
        fontDetails: FontMetric,
    ): Int {
        file.skipBytes(2) // format
        val numNames = file.readUnsignedShort()
        val stringOffset = file.readUnsignedShort()
        var bytesReadSoFar = 6
        val nameTable = arrayListOf<Triple<Int, Int, Int>>()

        for (i in 0 until numNames) {
            file.skipBytes(6) // platform id, encoding id, langid
            val nameID = file.readUnsignedShort()
            val length = file.readUnsignedShort()
            val offset = file.readUnsignedShort()
            nameTable.add(Triple(nameID, length, offset))
            bytesReadSoFar += 12
        }

        val stringTableSize = min(tableLength - bytesReadSoFar, tableLength - stringOffset)
        val stringTable = ByteArray(stringTableSize)

        if (stringTable.size != file.read(stringTable)) {
            throw IOException("Did not read entire string table")
        }

        bytesReadSoFar += stringTable.size

        // Now we're at the beginning of the string table
        for (i in nameTable) {
            when (i.first) {
                1 -> fontDetails.family = getString(stringTable, i.third, i.second)
                2 -> fontDetails.subFamily = getString(stringTable, i.third, i.second)
                3 -> fontDetails.uniqueSubFamily = getString(stringTable, i.third, i.second)
                4 -> fontDetails.fullName = getString(stringTable, i.third, i.second)
                5 -> fontDetails.fontVersion = getString(stringTable, i.third, i.second)
            }
        }
        return bytesReadSoFar
    }

    private fun getString(
        stringTable: ByteArray,
        offset: Int,
        length: Int,
    ): String {
        return String(stringTable.copyOfRange(offset, offset + length))
    }

    /**
     * Calculate the SHA-256 hash of the file passed
     */
    fun calculateFileHash(path: String): String {
        return calculateFileHash(FileInputStream(path))
    }

    /**
     * Calculate the SHA-256 hash of the InputStream passed
     */
    @Suppress("MagicNumber")
    private fun calculateFileHash(inputStream: InputStream): String {
        try {
            val md = MessageDigest.getInstance("SHA-256")
            val buffer = ByteArray(8192)
            var bytesRead: Int
            while (inputStream.read(buffer).also { bytesRead = it } != -1) {
                md.update(buffer, 0, bytesRead)
            }
            val digest = md.digest()
            // Convert the byte array to a hexadecimal string
            val hashBuilder = StringBuilder()
            for (b in digest) {
                hashBuilder.append(String.format("%02X", b))
            }
            return hashBuilder.toString()
        } catch (_: NoSuchAlgorithmException) {
            return "sha-256-not-found"
        }
    }
}
