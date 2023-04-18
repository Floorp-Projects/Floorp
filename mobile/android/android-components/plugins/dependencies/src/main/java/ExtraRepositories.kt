/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Extra Maven repositories to use
object ExtraRepositories {
    val mozillaStaging = (ApplicationServicesConfig.channel == ApplicationServicesChannel.STAGING
        || ApplicationServicesConfig.channel == ApplicationServicesChannel.NIGHTLY_STAGING)
}


