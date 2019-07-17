/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.content.blocking

/**
 * Represents a blocked content tracker.
 * @property url The URL of the tracker.
 * @property categories A list of categories that this [Tracker] belongs.
 */
class Tracker(val url: String, val categories: List<Category> = emptyList()) {
    enum class Category {
        Ad, Analytic, Social, Cryptomining, Fingerprinting, Content
    }
}
