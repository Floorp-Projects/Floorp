[android-components](../../index.md) / [mozilla.components.feature.privatemode.notification](../index.md) / [AbstractPrivateNotificationService](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`AbstractPrivateNotificationService()`

Manages notifications for private tabs.

Private tab notifications solve two problems:

1. They allow users to interact with the browser from outside of the app
    (example: by closing all private tabs).
2. The notification will keep the process alive, allowing the browser to
    keep private tabs in memory.

As long as a private tab is open this service will keep its notification alive.

