/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is example code for the 'Simplified Example' section of
// /docs/architecture-overview.md
class ContactsController(
    private val store: ContactsStore,
    private val navController: NavController
) {

    fun contactRenamed(contactId: Int, newName: String) {
        store.dispatch(ContactsAction.ContactRenamed(contactId = contactId, newName = newName))
    }

    fun chatSelected(contactId: Int) {
        // This is how we pass arguments between fragments using Google's navigation library.
        // See https://developer.android.com/guide/navigation/navigation-getting-started
        val directions = ContactsFragment.actionContactsFragmentToChatFragment(
            contactId = contactId
        )
        navController.nav(R.id.contactFragment, directions)
    }
}
