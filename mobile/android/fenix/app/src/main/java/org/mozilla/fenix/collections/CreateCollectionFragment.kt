package org.mozilla.fenix.collections

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.ViewModelProviders
import kotlinx.android.synthetic.main.fragment_create_collection.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable
import org.mozilla.fenix.mvi.getManagedEmitter
import org.mozilla.fenix.utils.ItsNotBrokenSnack

class CreateCollectionFragment : DialogFragment() {

    private lateinit var collectionCreationComponent: CollectionCreationComponent

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(DialogFragment.STYLE_NO_TITLE, R.style.CreateCollectionDialogStyle)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val view = inflater.inflate(R.layout.fragment_create_collection, container, false)

        collectionCreationComponent = CollectionCreationComponent(
            view.create_collection_wrapper,
            ActionBusFactory.get(this)
        )

        return view
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val tabs = activity?.run {
            ViewModelProviders.of(this).get(CreateCollectionViewModel::class.java)
        }!!.tabs

        getManagedEmitter<CollectionCreationChange>().onNext(CollectionCreationChange.TabListChange(tabs))

        getAutoDisposeObservable<CollectionCreationAction>().subscribe {
            when (it) {
                is CollectionCreationAction.Close -> dismiss()
                is CollectionCreationAction.SaveTabsToCollection -> {
                    dismiss()
                    ItsNotBrokenSnack(requireContext())
                        .showSnackbar("1843")
                }
                is CollectionCreationAction.AddTabToSelection -> getManagedEmitter<CollectionCreationChange>()
                    .onNext(CollectionCreationChange.TabAdded(it.tab))
                is CollectionCreationAction.RemoveTabFromSelection -> getManagedEmitter<CollectionCreationChange>()
                    .onNext(CollectionCreationChange.TabRemoved(it.tab))
                is CollectionCreationAction.SelectAllTapped -> getManagedEmitter<CollectionCreationChange>()
                    .onNext(CollectionCreationChange.AddAllTabs)
            }
        }
    }

    companion object {
        const val createCollectionTag = "createCollection"
    }
}
