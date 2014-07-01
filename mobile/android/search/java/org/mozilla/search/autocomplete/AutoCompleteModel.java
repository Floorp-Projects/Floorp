/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search.autocomplete;

/**
 * The SuggestionModel is the data model behind the autocomplete rows. Right now it
 * only has a text field. In the future, this could be extended to include other
 * types of rows. For example, a row that has a URL and the name of a website.
 */
class AutoCompleteModel {

    // The text that should immediately jump out to the user;
    // for example, the name of a restaurant or the title
    // of a website.
    private final String mainText;

    public AutoCompleteModel(String mainText) {
        this.mainText = mainText;
    }

    public String getMainText() {
        return mainText;
    }

    public String toString() {
        return mainText;
    }

}
