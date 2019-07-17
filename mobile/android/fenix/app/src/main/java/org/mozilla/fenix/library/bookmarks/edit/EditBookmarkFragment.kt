/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.edit

import android.content.DialogInterface
import android.graphics.PorterDuff.Mode.SRC_IN
import android.graphics.PorterDuffColorFilter
import android.os.Bundle
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.lifecycleScope
import androidx.navigation.Navigation
import com.jakewharton.rxbinding3.widget.textChanges
import com.uber.autodispose.AutoDispose
import com.uber.autodispose.android.lifecycle.AndroidLifecycleScopeProvider
import io.reactivex.Observable
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.functions.BiFunction
import io.reactivex.schedulers.Schedulers
import kotlinx.android.synthetic.main.fragment_edit_bookmark.*
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.launch
import mozilla.appservices.places.UrlParseFailed
import mozilla.components.concept.storage.BookmarkInfo
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.support.ktx.android.view.hideKeyboard
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.getColorFromAttr
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.setRootTitles
import org.mozilla.fenix.ext.withRootTitle
import org.mozilla.fenix.library.bookmarks.BookmarksSharedViewModel
import java.util.concurrent.TimeUnit

class EditBookmarkFragment : Fragment() {

    private lateinit var guidToEdit: String
    private val sharedViewModel: BookmarksSharedViewModel by activityViewModels()
    private var bookmarkNode: BookmarkNode? = null
    private var bookmarkParent: BookmarkNode? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        return inflater.inflate(R.layout.fragment_edit_bookmark, container, false)
    }

    override fun onResume() {
        super.onResume()
        context?.let {
            setRootTitles(it, showMobileRoot = true)
        }

        val activity = activity as? AppCompatActivity
        activity?.supportActionBar?.show()

        guidToEdit = EditBookmarkFragmentArgs.fromBundle(arguments!!).guidToEdit
        lifecycleScope.launch(IO) {
            bookmarkNode = requireComponents.core.bookmarksStorage.getTree(guidToEdit)
            bookmarkParent = sharedViewModel.selectedFolder
                ?: bookmarkNode?.parentGuid?.let {
                    requireComponents.core.bookmarksStorage.getTree(it)
                }.withRootTitle()

            launch(Main) {
                when (bookmarkNode?.type) {
                    BookmarkNodeType.FOLDER -> {
                        activity?.title = getString(R.string.edit_bookmark_folder_fragment_title)
                        bookmark_url_edit.visibility = View.GONE
                        bookmark_url_label.visibility = View.GONE
                    }
                    BookmarkNodeType.ITEM -> {
                        activity?.title = getString(R.string.edit_bookmark_fragment_title)
                    }
                    else -> throw IllegalArgumentException()
                }

                if (bookmarkNode != null) {
                    bookmark_name_edit.setText(bookmarkNode!!.title)
                    bookmark_url_edit.setText(bookmarkNode!!.url)

                    if (sharedViewModel.selectedFolder != null && bookmarkNode?.title != null) {
                        val bookmarkPair = Pair(bookmarkNode?.title, bookmarkNode?.url)
                        updateBookmarkNode(bookmarkPair)
                    }
                }
            }

            bookmarkParent?.let { node ->
                launch(Main) {
                    bookmark_folder_selector.text = node.title
                    bookmark_folder_selector.setOnClickListener {
                        sharedViewModel.selectedFolder = null
                        nav(
                            R.id.bookmarkEditFragment,
                            EditBookmarkFragmentDirections
                                .actionBookmarkEditFragmentToBookmarkSelectFolderFragment(null)
                        )
                    }
                }
            }
        }

        updateBookmarkFromObservableInput()
    }

    override fun onPause() {
        super.onPause()
        bookmark_name_edit.hideKeyboard()
        bookmark_url_edit.hideKeyboard()
    }

    private fun updateBookmarkFromObservableInput() {
        Observable.combineLatest(
            bookmark_name_edit.textChanges().skipInitialValue(),
            bookmark_url_edit.textChanges().skipInitialValue(),
            BiFunction { name: CharSequence, url: CharSequence ->
                Pair(name.toString(), url.toString())
            })
            .filter { it.first.isNotBlank() }
            .debounce(debouncePeriodInMs, TimeUnit.MILLISECONDS)
            .subscribeOn(Schedulers.io())
            .observeOn(AndroidSchedulers.mainThread())
            .`as`(AutoDispose.autoDisposable(AndroidLifecycleScopeProvider.from(this@EditBookmarkFragment)))
            .subscribe {
                updateBookmarkNode(it)
            }
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.bookmarks_edit, menu)
        menu.findItem(R.id.delete_bookmark_button).icon.colorFilter =
            PorterDuffColorFilter(R.attr.primaryText.getColorFromAttr(context!!), SRC_IN)
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.delete_bookmark_button -> {
                displayDeleteBookmarkDialog()
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    private fun displayDeleteBookmarkDialog() {
        activity?.let { activity ->
            AlertDialog.Builder(activity).apply {
                setMessage(R.string.bookmark_deletion_confirmation)
                setNegativeButton(android.R.string.cancel) { dialog: DialogInterface, _ ->
                    dialog.cancel()
                }
                setPositiveButton(R.string.tab_collection_dialog_positive) { dialog: DialogInterface, _ ->
                    lifecycleScope.launch(IO) {
                        requireComponents.core.bookmarksStorage.deleteNode(guidToEdit)
                        requireComponents.analytics.metrics.track(Event.RemoveBookmark)
                        launch(Main) {
                            Navigation.findNavController(requireActivity(), R.id.container).popBackStack()
                        }
                    }
                    dialog.dismiss()
                }
                create()
            }.show()
        }
    }

    private fun updateBookmarkNode(pair: Pair<String?, String?>) {
        lifecycleScope.launch(IO) {
            try {
                requireComponents.let {
                    if (pair != Pair(bookmarkNode?.title, bookmarkNode?.url)) {
                        it.analytics.metrics.track(Event.EditedBookmark)
                    }
                    if (sharedViewModel.selectedFolder != null) {
                        it.analytics.metrics.track(Event.MovedBookmark)
                    }
                    it.core.bookmarksStorage.updateNode(
                        guidToEdit,
                        BookmarkInfo(
                            sharedViewModel.selectedFolder?.guid ?: bookmarkNode!!.parentGuid,
                            bookmarkNode?.position,
                            pair.first,
                            if (bookmarkNode?.type == BookmarkNodeType.ITEM) pair.second else null
                        )
                    )
                }
            } catch (e: UrlParseFailed) {
                launch(Main) {
                    bookmark_url_edit.error = getString(R.string.bookmark_invalid_url_error)
                }
            }
        }
    }

    companion object {
        private const val debouncePeriodInMs = 500L
    }
}
