package org.mozilla.fenix.components.toolbar

sealed class TabCounterMenuItem {
    object CloseTab : TabCounterMenuItem()
    class NewTab(val isPrivate: Boolean) : TabCounterMenuItem()
}
