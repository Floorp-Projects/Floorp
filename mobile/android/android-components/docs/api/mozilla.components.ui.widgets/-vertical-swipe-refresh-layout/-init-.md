[android-components](../../index.md) / [mozilla.components.ui.widgets](../index.md) / [VerticalSwipeRefreshLayout](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`VerticalSwipeRefreshLayout(context: <ERROR CLASS>, attrs: <ERROR CLASS>? = null)`

[SwipeRefreshLayout](#) that filters only vertical scrolls for triggering pull to refresh.

Following situations will not trigger pull to refresh:

* a scroll happening more on the horizontal axis
* a scale in/out gesture
* a quick scale gesture

To control responding to scrolls and showing the pull to refresh throbber or not
use the [View.isEnabled](#) property.

