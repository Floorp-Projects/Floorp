*******
libpref
*******
libpref is a generic key/value store that is used to implement *prefs*, a term
that encompasses a variety of things.

- Feature enable/disable flags (e.g. ``dom.IntersectionObserver.enabled``,
  ``xpinstall.signatures.required``).
- User preferences (e.g. things set from ``about:preferences``)
- Internal application parameters (e.g.
  ``javascript.options.mem.nursery.max_kb``).
- Testing and debugging flags (e.g. ``network.disable.ipc.security``).
- Things that might need locking in an enterprise installation.
- Application data (e.g.
  ``browser.onboarding.tour.onboarding-tour-addons.completed``,
  ``services.sync.clients.lastSync``, ``network.predictor.cleaned-up``).
- A cheap and dirty form of IPC(!) (some devtools prefs).

Some of these (particularly the last two) are not an ideal use of libpref.

The C++ API is in the ``Preferences`` class. The XPIDL API is in the
``nsIPrefService`` and ``nsIPrefBranch`` interfaces.

High-level design
=================

Keys
----
Keys (a.k.a. *pref names*) are 8-bit strings, and ASCII in practice. The
convention is to use a dotted segmented form, e.g. ``foo.bar.baz``, but the
segments have no built-in meaning.

Naming is inconsistent, e.g. segments have various forms: ``foo_bar``,
``foo-bar``, ``fooBar``, etc. Pref names for feature flags are likewise
inconsistent: ``foo.enabled``, ``foo.enable``, ``foo.disable``, ``fooEnabled``,
``enable-foo``, ``foo.enabled.bar``, etc.

The grouping of prefs into families, via pref name segments, is ad hoc. Some of
these families are closely related, e.g. there are many font prefs that are
present for every language script.

Some prefs only make sense when considered in combination with other prefs.

Many pref names are known at compile time, but some are computed at runtime.

Basic values
------------
The basic types of pref values are bools, 32-bit ints, and 8-bit C strings.

Strings are used to encode many types of data: identifiers, alphanumeric IDs,
UUIDs, SHA1 hashes, CSS color hex values, large integers that don't fit into
32-bit ints (e.g. timestamps), directory names, URLs, comma-separated lists,
space-separated lists, JSON blobs, etc. There is a 1 MiB length limit on string
values; longer strings are rejected outright.

**Problem:** The C string encoding is unclear; some API functions deal with
unrestricted 8-bit strings (i.e. Latin1), but some require UTF-8.

There is some API support for faking floats, by converting them from/to strings when getting/setting.

**Problem:** confusion between ints and floats can lead to bugs.

Each pref consists of a default value and/or a user value. Default values can
be initialized from file at startup, and can be added and modified at runtime
via the API. User values can be initialized from file at startup, and can be
added, modified and removed at runtime via the API and ``about:config``.

If both values are present the user value takes precedence for most operations,
though there are operations that specifically work on the default value.

If a user value is set to the same value as the default value, the user value
is removed, unless the pref is marked as *sticky* at startup.

**Problem:** it would be better to have a clear notion of "reset to default",
at least for prefs that have a default value.

Prefs can be locked. This prevents them from being given a user value, or
hides the existing user value if there is one.

Complex values
--------------
There is API support for some complex values.

``nsIFile`` objects are handled by storing the filename as a string, similar to
how floats are faked by storing them as strings.

``nsIPrefLocalizedString`` objects are ones for which the default value
specifies a properties file that contains an entry whose name matches the
prefname. When gotten, the value from that entry is put into the user value.
When set, the given value just overwrites the user value, like a string pref.

**Problem:** this is weird and unlike all the other pref types.

``nsIRelativeFilePref`` objects are only used in comm-central.

Pref Branches
-------------
XPIDL-based access to prefs is via ``nsIPrefBranch``/``nsPrefBranch``, which
lets you specify a branch of the pref tree (e.g. ``font.``) and pref names work
relative to that point.

This API can be used from C++, but for C++ code there is also direct access
through the ``Preferences`` class, which uses absolute pref names.

Threads
-------
For the most part, all the basic API functions only work on the main thread.
However, there are two exceptions to this.

The narrow exception is that the Servo traversal thread is allowed to get pref
values. This only occurs when the main thread is paused, which makes it safe.
(Note: `bug 1474789 <https://bugzilla.mozilla.org/show_bug.cgi?id=1474789>`_
indicates that this may not be true.)

The broad exception is that static prefs can have a cached copy of a pref value
that can be accessed from other threads. See below.

Notifications
-------------
There is a notification API for being told when a pref's value changes. C++
code can register a callback function and JS code can register an observer (via
``nsIObserver``, which requires XPCOM). In both cases, the registered entity
will be notified when the value of the named pref value changes, or when the
value of any pref matching a given prefix changes. E.g. all font pref changes
can be observed by adding a ``font.`` prefix-matching observer.

See also the section on static prefs below.

Static prefs
------------
There is a special kind of pref called a static pref. Static prefs are defined
in ``StaticPrefList.yaml``.

If a static pref is defined in both ``StaticPrefList.yaml`` and a pref data
file, the latter definition will take precedence. A pref shouldn't appear in
both ``StaticPrefList.yaml`` and ``all.js``, but it may make sense for a pref
to appear in both ``StaticPrefList.yaml`` and an app-specific pref data file
such as ``firefox.js``.

Each static pref has a *mirror* kind.

* ``always``: A C++ *mirror variable* is associated with the pref. The variable
  is always kept in sync with the pref value. This kind is common.
* ``once``: A C++ mirror variable is associated with the pref. The variable is
  synced once with the pref's value at startup, and then does not change. This
  kind is less common, and mostly used for graphics prefs.
* ``never``: No C++ mirror variable is associated with the pref. This is much
  like a normal pref.

An ``always`` or ``once`` static pref can only be used for prefs with
bool/int/float values, not strings or complex values.

Each mirror variable is read-only, accessible via a getter function.

Mirror variables have two benefits. First, they allow C++ and Rust code to get
the pref value directly from the variable instead of requiring a slow hash
table lookup, which is important for prefs that are consulted frequently.
Second, they allow C++ and Rust code to get the pref value off the main thread.
The mirror variable must have an atomic type if it is read off the main thread,
and assertions ensure this.

Note that mirror variables could be implemented via vanilla callbacks without
API support, except for one detail: libpref gives their callbacks higher
priority than normal callbacks, ensuring that any static pref will be
up-to-date if read by a normal callback.

**Problem:** It is not clear what should happen to a static pref's mirror
variable if the pref is deleted? Currently there is a missing
``NotifyCallbacks()`` call so the mirror variable keeps its value from before
the deletion. The cleanest solution is probably to disallow static prefs from
being deleted.

Loading and Saving
------------------
Default pref values are initialized from various pref data files. Notable ones
include:

- ``modules/libpref/init/all.js``, used by all products;
- ``browser/app/profile/firefox.js``, used by Firefox desktop;
- ``mobile/android/app/mobile.js``, used by Firefox mobile;
- ``mail/app/profile/all-thunderbird.js``, used by Thunderbird (in comm-central);
- ``suite/browser/browser-prefs.js``, used by SeaMonkey (in comm-central).

In release builds these are all put into ``omni.ja``.

User pref values are initialized from ``prefs.js`` and (if present)
``user.js``, in the user's profile. This only happens once, in the parent
process. Note that ``prefs.js`` is managed by Firefox, and regularly
overwritten. ``user.js`` is created and managed by the user, and Firefox only
reads it.

These files are not JavaScript; the ``.js`` suffix is present for historical
reasons. They are read by a custom parser within libpref.

**Problem:** geckodriver has a separate prefs parser in the mozprofile crate.

**Problem:** there is no versioning of these files, for either the syntax or
the data. This makes changing the file format difficult.

There are API functions to save modified prefs, either synchronously or
asynchronously (via an off-main-thread runnable), either to the default file
(``prefs.js``) or to a named file. When saving to the default file, no action
will take place if no prefs have been modified.

Also, whenever a pref is modified, we wait 500ms and then automatically do an
off-main-thread save to ``prefs.js``. This provides an approximation of
`durability <https://en.wikipedia.org/wiki/ACID#Durability>`_, but it is still
possible for something to go wrong (e.g. a parent process crash) and end up
with recently changed prefs not being saved. (If such a thing happens, it
compromises `atomicity <https://en.wikipedia.org/wiki/ACID#Atomicity>`_, i.e. a
sequence of multiple related pref changes might only get partially written.)

Only prefs whose values have changed from the default are saved to ``prefs.js.``

**Problem:** Each time prefs are saved, the entire file is overwritten -- 10s
or even 100s of KiBs -- even if only a single value has changed. This happens
at least every 5 minutes, due to sync. Furthermore, various prefs are changed
during and shortly after startup, which can result in 10s of MiBs of disk
activity.

about:support
-------------
about:support contains an "Important Modified Preferences" table. It contains
all prefs that (a) have had their value changed from the default, and (b) whose
prefix match a whitelist in ``Troubleshoot.jsm``. The whitelist matching is to
avoid exposing pref values that might be privacy-sensitive.

**Problem:** The whitelist of prefixes is specified separately from the prefs
themselves. Having an attribute on a pref definition would be better.

Sync
----
On desktop, a pref is synced onto a device via Sync if there is an
accompanying ``services.sync.prefs.sync.``-prefixed pref. I.e. the pref
``foo.bar`` is synced if the pref ``services.sync.prefs.sync.foo.bar`` exists
and is true.

Previously, one could push prefs onto a device even if a local
``services.sync.prefs.sync.``-prefixed pref was not present; however this
behavior changed in `bug 1538015 <https://bugzilla.mozilla.org/show_bug.cgi?id=1538015>`_
to require the local prefixed pref to be present. The old (insecure) behavior
can be re-enabled by setting a single pref ``services.sync.prefs.dangerously_allow_arbitrary``
to true on the target browser - subsequently any pref can be pushed there by
creating a *remote* ``services.sync.prefs.sync.``-prefixed pref.

In practice, only a small subset of prefs (about 70) have a ``services.sync.prefs.sync.``-prefixed
pref by default.

**Problem:** This is gross. An attribute on the pref definition would be
better, but it might be hard to change that at this point.

The number of synced prefs is small because prefs are synced across versions;
any pref whose meaning might change shouldn't be synced. Also, we don't sync
prefs that may differ across different devices (such as a desktop machine
vs. a notebook).

Prefs are not synced on mobile.

Rust
----
Static prefs mirror variables can be accessed from Rust code via the
``static_prefs::pref!`` macro. Other prefs currently cannot be accessed. Parts
of libpref's C++ API could be made accessible to Rust code fairly
straightforwardly via C bindings, either hand-made or generated.

Cost of a pref
--------------
The cost of a single pref is low, but the cost of several thousand prefs is
reasonably high, and includes the following.

- Parsing and initializing at startup.
- IPC costs at startup and on pref value changes.
- Disk writing costs of pref value changes, especially during startup.
- Memory usage for storing the prefs, callbacks and observers, and C++ mirror
  variables.
- Complexity: most pref combinations are untested. Some can be set to a bogus
  value by a curious user, which can have `serious effects
  <https://rejzor.wordpress.com/2015/06/14/improve-firefox-html5-video-playback-performance/>`_
  (read the comments). Prefs can also have bugs. Real-life examples include
  mistyped prefnames, ``all.js`` entries with incorrect types (e.g. confusing
  int vs. float), both of which mean changing the pref value via about:config
  or the API would have no effect (see `bug 1414150
  <https://bugzilla.mozilla.org/show_bug.cgi?id=1414150>`_ for examples of
  both).
- Sync cost, for synced prefs.

Guidelines
----------
We have far too many prefs. This is at least partly because we have had, for a
long time, a culture of "when in doubt, add a pref". Also, we don't have any
system — either technical or cultural — for removing unnecessary prefs. See
`bug 90440 <https://bugzilla.mozilla.org/show_bug.cgi?id=90440>`_ for a pref
that was unused for 17 years.

In short, prefs are Firefox's equivalent of the Windows Registry: a dumping
ground for anything and everything. We should have guidelines for when to add a
pref.

Here are some good reasons to add a pref.

- *A user may genuinely want to change it.* E.g. it controls a feature that is
  adjustable in about:preferences.
- *To enable/disable new features.* Once a feature is mature, consider removing
  the pref. A pref expiry mechanism would help with this.
- *For certain testing/debugging flags.* Ideally, these would not be visible in
  about:config.

Here are some less good reasons to add a pref.

- *I'm not confident about this numeric parameter (cache size, timeout, etc.)*
  Get confident! In practice, few if any users will change it. Adding a pref
  doesn't absolve you of the responsibility of finding a good default. Then
  make it a code constant.
- *I need to experiment with different parameters during development.* This is
  reasonable, but consider removing the pref before landing or once the feature
  has matured. An expiry mechanism would help with this.
- *I sometimes fiddle with this value for debugging or testing.* 
  Is it worth exposing it to the whole world to save yourself a recompile every
  once in a while? Consider making it a code constant. 
- *Different values are needed on different platforms.* This can be done in
  other ways, e.g. ``#ifdef`` in C++ code.

These guidelines do not consider application data prefs (i.e. ones that
typically don't have a default value). They are quite different from the other
kinds. They arguably shouldn't prefs at all, and should be stored via some
other mechanism.

Low-level details
=================
The key idea is that the prefs database consists of two pieces. The first is an
initial snapshot of pref values that is created when the first child process is
created. This snapshot is stored in immutable, shared memory, and shared by all
processes.

Pref value changes that occur after this point are stored in a second hash
table. Each process has its own copy of this hash table. When pref values
change in the parent process, it performs IPC to inform child processes about
the changes, so they can update their copy.

The motivation for this design is memory usage. It's not tenable for every
child process to have a full copy of the prefs database.

Not all child processes need access to prefs. Those that do include web content
processes, the GPU process, and the RDD process.

Parent process startup
----------------------
The parent process initially has only a hash table.

Early in startup, the parent process loads all of the static prefs and default
prefs (mainly from ``omni.ja``) into that hash table. The parent process also
registers C++ mirror variables for static prefs, initializes them, and
registers callbacks so they will be updated appropriately for all subsequent
updates.

Slightly later in startup, the parent process loads all user prefs files,
mainly from the profile directory.

When the first getter for a ``once`` static pref is called, all the ``once``
static prefs have their mirror variables set and special frozen prefs are put
into the hash table. These frozen prefs are copies of the ``once`` prefs that
are given ``$$$`` prefixes and suffixes on their names. They are also marked
specially so they are ignored for all cases except when starting a new child
process. They exist so that all child processes can be given the same ``once``
values as the parent process.

Child process startup (parent side)
-----------------------------------
When the first child process is created, the parent process serializes its hash
table into a shared, immutable snapshot. This snapshot is stored in a shared
memory region managed by a ``SharedPrefMap`` instance. The parent process then
clears the hash table. The hash table is subsequently used only to store
changed pref values.

When any child process is created, the parent process serializes all pref
values present in the hash table (i.e. those that have changed since the
snapshot was made) and stores them in a second, short-lived shared memory
region. This represents the set of changes the child process needs to apply on
top of the snapshot, and allows it to build a hash table which should exactly
match the parent's.

The parent process passes two file descriptors to the child process, one for
each region of memory. The snapshot is the same for all child processes.

Child process startup (child side)
----------------------------------
Early in child process startup, the prefs service maps in and deserializes both
shared memory regions sent from the parent process, but defers further
initialization until requested by XPCOM initialization. Once that happens,
mirror variables are initialized for static prefs, but no default values are
set in the hash table, and no prefs files are loaded.

Once the mirror variables have been initialized, we dispatch pref change
callbacks for any prefs in the shared snapshot which have user values or are
locked. This causes the mirror variables to be updated.

After that, the changed pref values received from the parent process (via
``changedPrefsFd``) are added to the prefs database. Their values override the
values in the snapshot, and pref change callbacks are dispatched for them as
appropriate. ``once`` mirror variable are initialized from the special frozen
pref values.

Pref lookups
------------
Each prefs database has both a hash table and a shared memory snapshot. A given
pref may have an entry in either or both of these. If a pref exists in both,
the hash table entry takes precedence.

For pref lookups, the hash table is checked first, followed by the shared
snapshot. The entry in the hash table may have the type ``None``, in which case
the pref is treated as if it did not exist. The entry in the static snapshot
never has the type ``None``.

For pref enumeration, both maps are enumerated, starting with the hash table.
While iterating over the hash table, any entry with the type ``None`` is
skipped. While iterating over the shared snapshot, any entry which also exists
in the hash table is skipped. The combined result of the two iterations
represents the full contents of the prefs database.

Pref changes
------------
Pref changes can only be initiated in the parent process. All API methods that
modify prefs fail noisily (with ``NS_ERROR``) if run outside the parent
process.

Pref changes that happen before the initial snapshot have been made are simple,
and take place in the hash table. There is no shared snapshot to update, and no
child processes to synchronize with.

Once a snapshot has been created, any changes need to happen in the hash table.

If an entry for a changed pref already exists in the hash table, that entry can
be updated directly. Likewise for prefs that do not exist in either the hash
table or the shared snapshot: a new hash table entry can be created.

More care is needed when a changed pref exists in the snapshot but not in the
hash table. In that case, we create a hash table entry with the same values as
the snapshot entry, and then update it... but *only* if the changes will have
an effect. If a caller attempts to set a pref to its existing value, we do not
want to waste memory creating an unnecessary hash table entry.

Content processes must be told about any visible pref value changes. (A change
to a default value that is hidden by a user value is unimportant.) When this
happens, ``ContentParent`` detects the change (via an observer).  It checks the
pref name against a small blacklist of prefixes that child processes should not
care about (this is an optimization to reduce IPC rather than a
capabilities/security consideration), and for string prefs it also checks the
value(s) don't exceed 4 KiB. If the checks pass, it sends an IPC message
(``PreferenceUpdate``) to the child process, and the child process updates
the pref (default and user value) accordingly.

**Problem:** The blacklist of prefixes is specified separately from the prefs
themselves. Having an attribute on a pref definition would be better.

**Problem:** The 4 KiB limit can lead to inconsistencies between the parent
process and child processes. E.g. see
`bug 1303051 <https://bugzilla.mozilla.org/show_bug.cgi?id=1303051#c28>`_.

Pref deletions
--------------
Pref deletion is more complicated. If a pref to be deleted exists only in the
hash table of the parent process, its entry can simply be removed. If it exists
in the shared snapshot, however, its hash table entry needs to be kept (or
created), and its type changed to ``None``. The presence of this entry masks
the snapshot entry, causing it to be ignored by pref enumerators.
