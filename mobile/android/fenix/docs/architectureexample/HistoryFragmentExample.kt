/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is example code for the 'Simplified Example' section of
// /docs/architecture-overview.md
class HistoryFragment : Fragment() {

    private val store by lazy {
        StoreProvider.get(this) {
            HistoryStore(
                initialState = HistoryState.initial,
                middleware = listOf(
                    HistoryNavigationMiddleware(findNavController())
                    HistoryStorageMiddleware(HistoryStorage()),
                    HistoryTelemetryMiddleware(),
                )
            )
        }
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        return ComposeView(requireContext()).apply {
            setContent {
                HistoryScreen(store)
            }
        }
    }
}

@Composable
private fun HistoryScreen(store: HistoryStore) {
    val state = store.observeAsState(initialValue = HistoryState.initial) { state -> state }
    val listState = rememberLazyListState()
    LazyColumn(listState) {
        if (state.selectedItems.isNotEmpty()) {
            HistoryMultiSelectHeader(
                onDeleteSelectedClick = {
                    store.dispatch(HistoryAction.DeleteItems(state.selectedItems))
                }
            )
        } else {
            HistoryHeader(
                onDeleteAllClick = { store.dispatch(HistoryAction.DeleteItems(state.items)) }
            )
        }
        items(items = state.displayItems, key = { item -> item.id } ) { item ->
            val isSelected = state.selectedItems.find { selectedItem ->
                selectdItem == item
            }
            HistoryItem(
                item = item,
                isSelected = isSelected,
                onClick = { store.dispatch(HistoryAction.OpenItem(item)) },
                onLongClick = { store.dispatch(HistoryAction.ToggleItemSelection(item)) },
                onDeleteClick = { store.dispatch(HistoryAction.DeleteItems(listOf(item))) },
            )
        }
    }
}
