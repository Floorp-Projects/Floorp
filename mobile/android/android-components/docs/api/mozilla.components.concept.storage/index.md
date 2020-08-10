[android-components](../index.md) / [mozilla.components.concept.storage](./index.md)

## Package mozilla.components.concept.storage

### Types

| Name | Summary |
|---|---|
| [BookmarkInfo](-bookmark-info/index.md) | `data class BookmarkInfo`<br>Class for making alterations to any bookmark node |
| [BookmarkNode](-bookmark-node/index.md) | `data class BookmarkNode`<br>Class for holding metadata about any bookmark node |
| [BookmarkNodeType](-bookmark-node-type/index.md) | `enum class BookmarkNodeType`<br>The types of bookmark nodes |
| [BookmarksStorage](-bookmarks-storage/index.md) | `interface BookmarksStorage : `[`Storage`](-storage/index.md)<br>An interface which defines read/write operations for bookmarks data. |
| [HistoryAutocompleteResult](-history-autocomplete-result/index.md) | `data class HistoryAutocompleteResult`<br>Describes an autocompletion result against history storage. |
| [HistoryStorage](-history-storage/index.md) | `interface HistoryStorage : `[`Storage`](-storage/index.md)<br>An interface which defines read/write methods for history data. |
| [Login](-login/index.md) | `data class Login`<br>Represents a login that can be used by autofill APIs. |
| [LoginStorageDelegate](-login-storage-delegate/index.md) | `interface LoginStorageDelegate`<br>Used to handle [Login](-login/index.md) storage so that the underlying engine doesn't have to. An instance of this should be attached to the Gecko runtime in order to be used. |
| [LoginValidationDelegate](-login-validation-delegate/index.md) | `interface LoginValidationDelegate`<br>Provides a method for checking whether or not a given login can be stored. |
| [LoginsStorage](-logins-storage/index.md) | `interface LoginsStorage : `[`AutoCloseable`](http://docs.oracle.com/javase/7/docs/api/java/lang/AutoCloseable.html)<br>An interface describing a storage layer for logins/passwords. |
| [PageObservation](-page-observation/index.md) | `data class PageObservation` |
| [PageVisit](-page-visit/index.md) | `data class PageVisit`<br>Information to record about a visit. |
| [RedirectSource](-redirect-source/index.md) | `enum class RedirectSource`<br>A redirect source describes how a page redirected to another page. |
| [SearchResult](-search-result/index.md) | `data class SearchResult`<br>Encapsulates a set of properties which define a result of querying history storage. |
| [Storage](-storage/index.md) | `interface Storage`<br>An interface which provides generic operations for storing browser data like history and bookmarks. |
| [TopFrecentSiteInfo](-top-frecent-site-info/index.md) | `data class TopFrecentSiteInfo`<br>Information about a top frecent site. This represents a most frequently visited site. |
| [VisitInfo](-visit-info/index.md) | `data class VisitInfo`<br>Information about a history visit. |
| [VisitType](-visit-type/index.md) | `enum class VisitType`<br>Visit type constants as defined by Desktop Firefox. |
