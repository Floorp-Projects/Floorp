/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.datastore

import androidx.datastore.core.Serializer
import java.io.InputStream
import java.io.OutputStream

/**
 * Serializer for [SelectedPocketStoriesCategories] defined in selected_pocket_stories_categories.proto.
 */
@Suppress("BlockingMethodInNonBlockingContext")
object SelectedPocketStoriesCategorySerializer : Serializer<SelectedPocketStoriesCategories> {
    override val defaultValue: SelectedPocketStoriesCategories = SelectedPocketStoriesCategories.getDefaultInstance()

    override suspend fun readFrom(input: InputStream): SelectedPocketStoriesCategories {
        return SelectedPocketStoriesCategories.parseFrom(input)
    }

    override suspend fun writeTo(t: SelectedPocketStoriesCategories, output: OutputStream) {
        t.writeTo(output)
    }
}
