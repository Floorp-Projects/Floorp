/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.helpers

import android.content.Context
import android.database.Cursor
import android.net.Uri
import android.provider.MediaStore
import android.util.Log
import androidx.core.net.toUri

object DeleteFilesHelper {
    fun deleteFileUsingDisplayName(context: Context, displayName: String): Boolean {
        val uri = getUriFromDisplayName(context, displayName)
        if (uri != null) {
            val resolver = context.contentResolver
            val selectionArgs = arrayOf(displayName)
            resolver.delete(
                uri,
                MediaStore.Files.FileColumns.DISPLAY_NAME + "=?",
                selectionArgs,
            )
            Log.d("TestLog", "Download file deleted")
            return true
        }
        Log.d("TestLog", "Download file could not be found")
        return false
    }

    private fun getUriFromDisplayName(context: Context, displayName: String): Uri? {
        val projection = arrayOf(MediaStore.Files.FileColumns._ID)
        val extUri: Uri = MediaStore.Files.getContentUri("external")
        val cursor: Cursor = context.contentResolver.query(
            extUri,
            projection,
            MediaStore.Files.FileColumns.DISPLAY_NAME + " LIKE ?",
            arrayOf(displayName),
            null,
        )!!
        cursor.moveToFirst()
        return if (cursor.count > 0) {
            val columnIndex: Int = cursor.getColumnIndex(projection[0])
            val fileId: Long = cursor.getLong(columnIndex)
            cursor.close()
            "$extUri/$fileId".toUri()
        } else {
            null
        }
    }
}
