/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.edit

import android.content.DialogInterface
import android.graphics.PorterDuff
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
import androidx.lifecycle.ViewModelProviders
import androidx.navigation.Navigation
import com.jakewharton.rxbinding3.widget.textChanges
import com.uber.autodispose.AutoDispose
import com.uber.autodispose.android.lifecycle.AndroidLifecycleScopeProvider
import io.reactivex.Observable
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.functions.BiFunction
import io.reactivex.schedulers.Schedulers
import kotlinx.android.synthetic.main.fragment_edit_bookmark.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.appservices.places.UrlParseFailed
import mozilla.components.concept.storage.BookmarkInfo
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.getColorFromAttr
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.library.bookmarks.BookmarksSharedViewModel
import java.util.concurrent.TimeUnit
import kotlin.coroutines.CoroutineContext

class EditBookmarkFragment : Fragment(), CoroutineScope {

    private lateinit var sharedViewModel: BookmarksSharedViewModel
    private lateinit var job: Job
    private lateinit var guidToEdit: String
    private var bookmarkNode: BookmarkNode? = null
    private var bookmarkParent: BookmarkNode? = null

    override val coroutineContext: CoroutineContext
        get() = Main + job

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        job = Job()
        setHasOptionsMenu(true)
        sharedViewModel = activity?.run {
            ViewModelProviders.of(this).get(BookmarksSharedViewModel::class.java)
        }!!
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        return inflater.inflate(R.layout.fragment_edit_bookmark, container, false)
    }

    override fun onResume() {
        super.onResume()
        (activity as? AppCompatActivity)?.title = getString(R.string.edit_bookmark_fragment_title)
        (activity as? AppCompatActivity)?.supportActionBar?.show()

        guidToEdit = EditBookmarkFragmentArgs.fromBundle(arguments!!).guidToEdit
        launch(IO) {
            bookmarkNode = requireComponents.core.bookmarksStorage.getTree(guidToEdit)
            bookmarkParent = sharedViewModel.selectedFolder
                ?: bookmarkNode?.parentGuid?.let { requireComponents.core.bookmarksStorage.getTree(it) }

            launch(Main) {
                when (bookmarkNode?.type) {
                    BookmarkNodeType.FOLDER -> {
                        bookmark_url_edit.visibility = View.GONE
                        bookmark_url_label.visibility = View.GONE
                    }
                    BookmarkNodeType.ITEM -> {
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
                        Navigation.findNavController(requireActivity(), R.id.container).navigate(
                            EditBookmarkFragmentDirections
                                .actionBookmarkEditFragmentToBookmarkSelectFolderFragment(null)
                        )
                    }
                }
            }
        }

        updateBookmarkFromObservableInput()
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

    override fun onDestroy() {
        super.onDestroy()
        job.cancel()
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.bookmarks_edit, menu)
        menu.findItem(R.id.delete_bookmark_button).icon.colorFilter =
            PorterDuffColorFilter(R.attr.primaryText.getColorFromAttr(context!!), PorterDuff.Mode.SRC_IN)
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
                    launch(IO) {
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
        launch(IO) {
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
