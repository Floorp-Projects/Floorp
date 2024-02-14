# Multisession Architecture

To support multiple simultaneous browsing sessions the architecture of Focus has been refactored to strictly separate sessions (and associated data) from the UI that displays this data.

Sessions are managed by a global `SessionManager` instance. UI components display data hold by `Session` objects.

The new multisession architecture makes use of Android's [Architecture Components](https://developer.android.com/topic/libraries/architecture/index.html). The list of sessions in the `SessionManager` and the data inside a `Session` is returned as [LiveData](https://developer.android.com/topic/libraries/architecture/livedata.html) objects. UI components can observe those LiveData objects for changes and update the UI state if needed.

![](http://i.imgur.com/CUb5ZLW.png)

* _BrowserFragment_ is the fragment that displays the browser chrome and web content. A _BrowserFragment_ is always connected to a _Session_ (1:1).

* _IWebView_ is a generic interface for the view that renders web content. At runtime it's implemented by either [WebView](https://developer.android.com/reference/android/webkit/WebView.html) or GeckoView. State changes will be reported to a class that implements _IWebView.Callback_.

* With the multisession architecture _IWebView.Callback_ is implemented by _SessionCallbackProxy_. The purpose of this class is to update the _Session_ object for this browsing session. Currently _SessionCallbackProxy_ will still delegate some callback methods to the BrowserFragment. This is expected to be removed in a follow-up refactoring eventually.

* _BrowserFragment_ displays data hold by the _Session_ object. Data that changes periodically (e.g. URL, progress, ..) are represented as [LiveData](https://developer.android.com/topic/libraries/architecture/livedata.html) objects and can be observed so that the UI can be updated whenever the state changes.

* Sessions are switched by displaying a new _BrowserFragment_ for a different _Session_ object.
