/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.digitalassetlinks

/**
 * Describes the nature of a statement, and consists of a kind and a detail.
 * @property kindAndDetail Kind and detail, separated by a slash character.
 */
enum class Relation(val kindAndDetail: String) {

    /**
     * Grants the target permission to retrieve sign-in credentials stored for the source.
     * For App -> Web transitions, requests the app to use the declared origin to be used as origin
     * for the client app in the web APIs context.
     */
    USE_AS_ORIGIN("delegate_permission/common.use_as_origin"),

    /**
     * Requests the ability to handle all URLs from a given origin.
     */
    HANDLE_ALL_URLS("delegate_permission/common.handle_all_urls"),

    /**
     * Grants the target permission to retrieve sign-in credentials stored for the source.
     */
    GET_LOGIN_CREDS("delegate_permission/common.get_login_creds"),
}
