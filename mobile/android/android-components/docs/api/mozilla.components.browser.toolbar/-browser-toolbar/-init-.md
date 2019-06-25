[android-components](../../index.md) / [mozilla.components.browser.toolbar](../index.md) / [BrowserToolbar](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`BrowserToolbar(context: <ERROR CLASS>, attrs: <ERROR CLASS>? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`

A customizable toolbar for browsers.

The toolbar can switch between two modes: display and edit. The display mode displays the current
URL and controls for navigation. In edit mode the current URL can be edited. Those two modes are
implemented by the DisplayToolbar and EditToolbar classes.

```
             +----------------+
             | BrowserToolbar |
             +--------+-------+
                      +
              +-------+-------+
              |               |
    +---------v------+ +-------v--------+
    | DisplayToolbar | |   EditToolbar  |
    +----------------+ +----------------+
```

