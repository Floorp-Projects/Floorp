GeckoView Extension Managing API
================================

Agi Sferro <agi@sferro.dev>

November 19th, 2019

Introduction
------------

This document describes the API for installing, uninstalling and updating
Extensions with GeckoView.

Installing an extension provides the extension the ability to run at startup
time, especially useful for e.g. extensions that intercept network requests,
like an ad-blocker or a proxy extension. It also provides additional security
from third-party extensions like signature checking and prompting the user for
permissions.

For this version of the API we will assume that the extension store is backed
by ``addons.mozilla.org``, and so are the signatures. Running a third-party
extension store is something we might consider in the future but explicitly not
in scope for this document.

API
---

The embedder will be able to install, uninstall, enable, disable and update
extensions using the similarly-named APIs.

Installing
^^^^^^^^^^

Gecko will download the extension pointed by the URI provided in install, parse
the manifest and signature and provide an ``onInstallPrompt`` callback with the
list of permissions requested by the extension and some information about the
extension.

The embedder will be able to install bundled first-party extensions using
``installBuiltIn``. This method will only accept URIs that start with
``resource://`` and will give additional privileges like being able to use app
messaging and not needing a signature.

Each permission will have a machine readable name that the embedder will use to
produce user-facing internationalized strings. E.g. “bookmarks” gives access to
bookmarks, “sessions” gives access to recently closed sessions. The full list
of permissions that are currently shown to the UI in Firefox Desktop is
available at: ``chrome/browser/browser.properties``

``WebExtension.MetaData`` properties expected to be set to absolute moz-extension urls
(e.g. ``baseUrl`` and ``optionsPageUrl``) are not available yet right after installing
a new extension. Once the extension has been fully started, the delegate method
``WebExtensionController.AddonManagerDelegate.onReady`` will be providing to the
embedder app a new instance of the `MetaData` object where ``baseUrl`` is expected
to be set to a ``"moz-extension://..."`` url (and ``optionsPageUrl`` as well if an
options page was declared in the extension manifest.json file).

Updating
^^^^^^^^

To update an extension, the embedder will be able to call update which will
check if any update is available (using the update_url provided by the
extension, or addons.mozilla.org if no update_url has been provided). The
embedder will receive a GeckoResult that will provide the updated extension
object. This result can also be used to know when the update process is
complete, e.g. the embedder could use it to display a persistent notification
to the user to avoid having the app be killed while updates are in process.

If the updated extension needs additional permissions, ``GeckoView`` will call
``onUpdatePrompt``.

Until this callback is resolved (i.e. the embedder’s returned ``GeckoResult``
is completed), the old addon will be running, only when the prompt is resolved
and the update is applied the new version of the addon starts running and the
``GeckoResult`` returned from update is resolved.

This callback will provide both the current ``WebExtension`` object and the
updated WebExtension object so that the embedder can show appropriate
information to the user, e.g. the app might decide to remember whether the user
denied the request for a certain version and only prompt the user once the
version string changes.

As a side effect of updating, Gecko will check its internal blocklist and might
disable extensions that are incompatible with the current version of Gecko or
deemed unsafe. The resulting ``WebExtension`` object will reflect that by
having isEnabled set to false. The embedder will be able to inspect the reason
why the extension was disabled using ``metaData.blockedReason``.

Gecko will not update any extension or blocklist state without the embedder’s
input.

Enabling and Disabling
^^^^^^^^^^^^^^^^^^^^^^

Embedders will be able to enable and disabling extension using the homonymous
APIs. Calling enable on an extension might not actually enable it if the
extension has been added to the Gecko blocklist. Embedders can check the value
of ``metaData.blockedReason`` to display to the user whether the extension can
actually be enabled or not. The returned WebExtension object will reflect the
updated enablement state in isEnabled.

Listing
^^^^^^^

The embedder is expected to keep a collection of all available extensions using
the result of install and update. To retrieve the extensions that are already
installed the embedder will be able to use ``listInstalled`` which will
asynchronously retrieve the full list of extensions. We recommend calling
``listInstalled`` every time the user is presented with the extension manager
UI to ensure all information is up to date.

Outline
^^^^^^^

.. code:: java

  public class WebExtensionController {
    // Start the process of installing an extension,
    // the embedder will either get the installed extension
    // or an error
    GeckoResult<WebExtension> install(String uri);

    // Install a built-in WebExtension with privileged
    // permissions, uri must be resource://
    // Privileged WebExtensions have access to experiments
    // (i.e. they can run chrome code), don’t need signatures
    // and have access to native messaging to the app
    GeckoResult<WebExtension> installBuiltIn(String uri)

    GeckoResult<Void> uninstall(WebExtension extension);

    GeckoResult<WebExtension> enable(WebExtension extension);

    GeckoResult<WebExtension> disable(WebExtension extension);

    GeckoResult<List<WebExtension>> listInstalled();

    // Checks for updates. This method returns a GeckoResult that is
    // resolved either with the updated WebExtension object or null
    // if the extension does not have pending updates.
    GeckoResult<WebExtension> update(WebExtension extension);

    public interface PromptDelegate {
        GeckoResult<AllowOrDeny> onInstallPrompt(WebExtension extension);

        GeckoResult<AllowOrDeny> onUpdatePrompt(
            WebExtension currentlyInstalled,
            WebExtension updatedExtension,
            List<String> newPermissions);

        // Called when the extension calls browser.permission.request
        GeckoResult<AllowOrDeny> onOptionalPrompt(
            WebExtension extension,
            List<String> optionalPermissions);
    }

    void setPromptDelegate(PromptDelegate promptDelegate);
  }

As part of this document, we will add a ``MetaData`` field to WebExtension
which will contain all the information known about the extension. Note: we will
rename ``ActionIcon`` to Icon to represent its generic use as the
``WebExtension`` icon class.

.. code:: java

  public class WebExtension {
    // Renamed from ActionIcon
    static class Icon {}

    final MetaData metadata;
    final boolean isBuiltIn;

    final boolean isEnabled;

    public static class SignedStateFlags {
      final static int UNKNOWN;
      final static int PRELIMINARY;
      final static int SIGNED;
      final static int SYSTEM;
      final static int PRIVILEGED;
    }

    // See nsIBlocklistService.idl
    public static class BlockedReason {
      final static int NOT_BLOCKED;
      final static int SOFTBLOCKED;
      final static int BLOCKED;
      final static int OUTDATED;
      final static int VULNERABLE_UPDATE_AVAILABLE;
      final static int VULNERABLE_NO_UPDATE;
    }

    public class MetaData {
      final Icon icon;
      final String[] permissions;
      final String[] origins;
      final String name;
      final String description;
      final String version;
      final String creatorName;
      final String creatorUrl;
      final String homepageUrl;
      final String baseUrl;
      final String optionsPageUrl;
      final boolean openOptionsPageInTab;
      final boolean isRecommended;
      final @BlockedReason int blockedReason;
      final @SignedState int signedState;
      // more if needed
    }
  }

Implementation Details
^^^^^^^^^^^^^^^^^^^^^^

We will use ``AddonManager`` as a backend for ``WebExtensionController`` and
delegate the prompt to the app using ``PromptDelegate``. We will also merge
``WebExtensionController`` and ``WebExtensionEventDispatcher`` for ease of
implementation.

Existing APIs
^^^^^^^^^^^^^

Some APIs today return a ``WebExtension`` object that might have not been
fetched yet by ``listInstalled``. In these cases, GeckoView will return a stub
``WebExtension`` object in which the metadata field will be null to avoid
waiting for a addon list call. To ensure that the metadata field is populated,
the embedder will need to call ``listInstalled`` at least once during the app
startup.

Deprecation Path
^^^^^^^^^^^^^^^^

The existing ``registerWebExtension`` and ``unregisterWebExtension`` APIs will
be deprecated by ``installBuiltIn`` and ``uninstall``. We will remove the above
APIs 6 releases after the implementation of ``installBuiltIn`` lands and mark
it as deprecated in the API.
