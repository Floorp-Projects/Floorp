/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import android.content.Context
import android.view.LayoutInflater
import android.widget.EditText
import androidx.appcompat.app.AlertDialog
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.support.ktx.android.view.showKeyboard
import org.mozilla.fenix.R
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.ext.getDefaultCollectionNumber

/**
 * A lambda that is invoked when a confirmation button in a [CollectionsDialog] is clicked.
 *
 * The [TabCollection] ID of the selected collection is passed to the delegate when confirmed.
 * If the selected collection was newly created then [Boolean] is set to true.
 */
typealias OnPositiveButtonClick = (id: Long?, isNewCollection: Boolean) -> Unit

/**
 * A lambda that is invoked when a cancel button in a [CollectionsDialog] is clicked.
 */
typealias OnNegativeButtonClick = () -> Unit

/**
 * A data class for creating a dialog to prompt adding/creating a collection. See also [show].
 *
 * @property onPositiveButtonClick Invoked when a user clicks on a confirmation button in the dialog.
 * @property onNegativeButtonClick Invoked when a user clicks on a cancel button in the dialog.
 */
data class CollectionsDialog(
    val storage: TabCollectionStorage,
    val sessionList: List<TabSessionState>,
    val onPositiveButtonClick: OnPositiveButtonClick,
    val onNegativeButtonClick: OnNegativeButtonClick,
)

/**
 * Create and display a [CollectionsDialog] using [AlertDialog].
 */
fun CollectionsDialog.show(
    context: Context,
) {
    if (storage.cachedTabCollections.isEmpty()) {
        showAddNewDialog(context, storage)
        return
    }

    val collections = storage.cachedTabCollections.map { it.title }
    val layout = LayoutInflater.from(context).inflate(R.layout.add_new_collection_dialog, null)
    val list = layout.findViewById<RecyclerView>(R.id.recycler_view)

    val builder = AlertDialog.Builder(context).setTitle(R.string.tab_tray_select_collection)
        .setView(layout)
        .setPositiveButton(R.string.create_collection_positive) { dialog, _ ->
            val selectedCollection =
                (list.adapter as CollectionsListAdapter).getSelectedCollection()
            val collection = storage.cachedTabCollections[selectedCollection]

            MainScope().launch {
                val id = storage.addTabsToCollection(collection, sessionList)
                onPositiveButtonClick.invoke(id, false)
            }

            dialog.dismiss()
        }.setNegativeButton(R.string.create_collection_negative) { dialog, _ ->
            onNegativeButtonClick.invoke()

            dialog.cancel()
        }

    val dialog = builder.create()
    val collectionNames =
        arrayOf(context.getString(R.string.tab_tray_add_new_collection)) + collections
    val collectionsListAdapter = CollectionsListAdapter(collectionNames) {
        dialog.dismiss()
        showAddNewDialog(context, storage)
    }

    list.apply {
        layoutManager = LinearLayoutManager(context)
        adapter = collectionsListAdapter
    }
    dialog.show()
}

internal fun CollectionsDialog.showAddNewDialog(
    context: Context,
    collectionsStorage: TabCollectionStorage,
) {
    val layout = LayoutInflater.from(context).inflate(R.layout.name_collection_dialog, null)
    val collectionNameEditText: EditText = layout.findViewById(R.id.collection_name)

    collectionNameEditText.setText(
        context.getString(
            R.string.create_collection_default_name,
            collectionsStorage.cachedTabCollections.getDefaultCollectionNumber(),
        ),
    )

    AlertDialog.Builder(context)
        .setTitle(R.string.tab_tray_add_new_collection)
        .setView(layout).setPositiveButton(R.string.create_collection_positive) { dialog, _ ->

            MainScope().launch {
                val id = storage.createCollection(
                    collectionNameEditText.text.toString(),
                    sessionList,
                )
                onPositiveButtonClick.invoke(id, true)
            }

            dialog.dismiss()
        }
        .setNegativeButton(R.string.create_collection_negative) { dialog, _ ->
            onNegativeButtonClick.invoke()
            dialog.cancel()
        }
        .create()
        .show()

    collectionNameEditText.setSelection(0, collectionNameEditText.text.length)
    collectionNameEditText.showKeyboard()
}
