/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is example code for the 'Simplified Example' section of
// /docs/architecture-overview.md
class ContactsInteractor(
    private val contactsController: ContactsController,
    private val themeController: ThemeController
) {

    fun onThemeSelected(theme: Theme) {
        themeController.themeSelected(theme)
    }

    fun onContactRenamed(contactId: Int, newName: String) {
        contactsController.contactRenamed(contactId, newName)
    }

    fun onChatSelected(contactId: Int) {
        contactsController.chatSelected(contactId)
    }
}
