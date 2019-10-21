/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// This is example code for the 'Simplified Example' section of
// /docs/architecture-overview.md
class ThemeController(
    private val ContactsStore
) {
    fun themeSelected(newTheme: Theme) {
        store.dispatch(ContactsAction.ThemeChanged(newTheme = newTheme))
    }
}
