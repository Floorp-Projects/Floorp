/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// This is example code for the 'Simplified Example' section of
// /docs/architecture-overview.md
class ContactsFragment : Fragment() {

    lateinit var contactsStore: ContactsStore
    lateinit var contactsView: ContactsView

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val view = inflater.inflate(R.layout.fragment_contacts, container, false)

        // Create the various components and hook them up to each other
        val initialState = ContactsState(
            contacts = emptyList(),
            theme = Theme.ORANGE
        )

        contactsStore = ContactsStore(initialState = initialState)

        val contactsController = ContactsController(
            store = store,
            navController = findNavController()
        )

        val themeController = ThemeController(
            store = store
        )

        val interactor = ContactsInteractor(
            contactsController = contactsController,
            themeController = themeController
        )

        contactsView = ContactsView(view.contains_container, interactor)
    }

    override onViewCreated(view: View, savedInstanceState: Bundle?) {
        // Whenever State is updated, pass it to the View
        consumeFrom(contactsStore) { state ->
            contactsView.update(state)
        }
    }
}
