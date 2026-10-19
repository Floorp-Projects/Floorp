# Chrome extension document file systems

`chrome-extension-filesystem.patch` implements the legacy callback-based
`requestFileSystem`/`webkitRequestFileSystem`, directory/file entries, native
`File`, quota queries, and asynchronous `FileWriter` with actual profile-backed
files. It is restricted to authenticated converted extension documents. Ordinary
Firefox extensions and websites do not receive the globals.

Both filesystem kinds persist in the extension's profile storage. Uninstall
removes them after queued I/O settles. Private extension documents use a
separate native private partition, deleted after the last private context exits
and during profile shutdown. Extension updates preserve regular files.

`filesystem:moz-extension://<owner>/<kind>/<path>` supports owner-only reads,
images, and actual downloads. The parent checks both loading and triggering
principals. Executable document/script/style/object loads are rejected. Native
CSP is checked before creating the channel; workers without a document are
rejected. No storage directory or file path is accepted from extension code.

The channel uses a unique reserved URL under the owner's `moz-extension:` origin
to preserve native CORS and canvas access. Consequently, `Response.url` exposes
that reserved URL. It cannot be used to retrieve a storage entry directly and
responses bypass caches. Entry URLs retain their `filesystem:` form.

Limits are 50 MiB per kind, or 2 GiB with `unlimitedStorage`, and 10,000 entries
including directories. Loaded quota metadata must be a nonnegative safe integer.
Operations use portable file names, reject root escapes and symbolic links,
validate buffers before allocation, and stream reads through bounded pipes.

Each document runs at most four `FileWriter` operations at once. Further
operations wait in a queue capped at 128 entries and 1 GiB of immutable Blob
snapshots. Chunks are materialized after an operation starts; the IPC bridge
keeps its separate limit of four buffers of at most 16 MiB each. Queue overflow
reports `QuotaExceededError`. Aborting a waiting writer releases its
reservation; closing the owner document cancels its queued work.

Native browser verification lives in `FileSystemNative.test.mts` and
`FileSystemPrivateNative.test.mts`: real writes/seek/truncate, native `File`,
PNG decode and pixels, more than 16 MiB fetch with exact SHA-256, cancellation,
owner-primed cache negatives, foreign addon/site/download/native-channel
denials, explicit CSP denials, byte/entry quotas, queued-I/O uninstall, and
private lifetime. `CaptureCorpusNative.test.mts` drives the immutable GoFullPage
Store package.
