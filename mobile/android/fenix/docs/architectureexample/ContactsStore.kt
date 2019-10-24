/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is example code for the 'Simplified Example' section of
// /docs/architecture-overview.md
class ContactsStore(
    private val initialState: ContactsState
) : Store<ContactsState, Reducer<ContactState, ContactsAction>>(initialState, ::reducer)

sealed class ContactsAction {
    data class ContactRenamed(val contactId: Int, val newName: String) : ContactsAction
    data class ThemeChanged(val newTheme: Theme) : ContactsAction
}

data class ContactsState(
    val contacts: List<Contact>,
    val theme: Theme
)

data class Contact(
    val name: String,
    val id: Int,
    val imageUrl: Uri
)

enum class Theme {
    ORANGE, DARK
}

fun reducer(oldState: ContactsState, action: ContactsAction): ContactsState = when (action) {
    is ContactsAction.ThemeChanged -> oldState.copy(theme = action.newTheme)
    is ContactsAction.ContactRenamed -> {
        val newContacts = oldState.contacts.map { contact ->
            // If this is the contact we want to change...
            if (contact.id == action.contactId) {
                // Update its name, but keep other values the same
                contact.copy(name = newName)
            } else {
                // Otherwise return the original contact
                return@map contact
            }
        }

        return oldState.copy(contacts = newContacts)
    }
}
