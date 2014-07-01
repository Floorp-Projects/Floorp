/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search.autocomplete;


/**
 * Allows rows to pass a search event to the parent fragment.
 */
public interface AcceptsSearchQuery {
    void onSearch(String s);
}
