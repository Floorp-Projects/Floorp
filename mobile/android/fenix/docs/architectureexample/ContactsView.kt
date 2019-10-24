/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is example code for the 'Simplified Example' section of
// /docs/architecture-overview.md
class ContactsView(
    private val container: ViewGroup,
    private val interactor: ContactsInteractor
) {

    val view: View = LayoutInflater.from(container.context)
        .inflate(R.layout.contact_list, container, true)

    private val contactAdapter: ContactAdapter

    init {
        // Setup view constraints and anything else that will not change as data updates
        view.select_theme_orange.setOnClickListener {
            interactor.onThemeSelected(Theme.ORANGE)
        }
        view.select_theme_dark.setOnClickListner {
            interactor.onThemeSelected(Theme.DARK)
        }
        // The RecyclerView.Adapter is passed the interactor, and will call it from its own listeners
        contactAdapter = ContactAdapter(view.contactRoot, interactor)
        view.contact_recycler.apply {
            adapter = contactAdapter
        }
    }

    fun update(state: ContactsState) {
        view.toolbar.setColor(ContextCompat.getColor(this, R.color.state.toolbarColor))
        contactAdapter.update(state)
    }
}
