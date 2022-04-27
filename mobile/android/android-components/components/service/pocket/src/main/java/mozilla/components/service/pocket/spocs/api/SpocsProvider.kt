/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs.api

import mozilla.components.service.pocket.stories.api.PocketResponse

/**
 * All possible operations related to SPocs - Sponsored Pocket stories.
 */
internal interface SpocsProvider {
    /**
     * Download new sponsored stories.
     *
     * @return [PocketResponse.Success] containing a list of sponsored stories or
     * [PocketResponse.Failure] if the request didn't complete successfully.
     */
    suspend fun getSponsoredStories(): PocketResponse<List<ApiSpoc>>

    /**
     * Delete all data associated with [profileId].
     *
     * @return [PocketResponse.Success] if the request completed successfully, [PocketResponse.Failure] otherwise.
     */
    suspend fun deleteProfile(): PocketResponse<Boolean>
}
