
Environment
===========

The environment consists of data that is expected to be characteristic for performance and other behavior and not expected to change too often.

Changes to most of these data points are detected (where possible and sensible) and will lead to a session split in the :doc:`main-ping`.
The environment data may also be submitted by other ping types.

*Note:* This is not submitted with all ping types due to privacy concerns. This and other data is inspected under the `data collection policy <https://wiki.mozilla.org/Firefox/Data_Collection>`_.

Some parts of the environment must be fetched asynchronously at startup. We don't want other Telemetry components to block on waiting for the environment, so some items may be missing from it until the async fetching finished.
This currently affects the following sections:

- profile
- add-ons
- services


Structure:

.. code-block:: js

    {
      build: {
        applicationId: <string>, // nsIXULAppInfo.ID
        applicationName: <string>, // "Firefox"
        architecture: <string>, // e.g. "x86", build architecture for the active build
        buildId: <string>, // e.g. "20141126041045"
        version: <string>, // e.g. "35.0"
        vendor: <string>, // e.g. "Mozilla"
        displayVersion: <string>, // e.g. "35.0b1"
        platformVersion: <string>, // e.g. "35.0"
        xpcomAbi: <string>, // e.g. "x86-msvc"
        updaterAvailable: <bool>, // Whether the app was built with app update available (MOZ_UPDATER)
      },
      settings: {
        addonCompatibilityCheckEnabled: <bool>, // Whether application compatibility is respected for add-ons
        blocklistEnabled: <bool>, // true on failure
        isDefaultBrowser: <bool>, // whether Firefox is the default browser. On Windows, this is operationalized as whether Firefox is the default HTTP protocol handler and the default HTML file handler.
        defaultSearchEngine: <string>, // e.g. "yahoo"
        defaultSearchEngineData: {, // data about the current default engine
          name: <string>, // engine name, e.g. "Yahoo"; or "NONE" if no default
          loadPath: <string>, // where the engine line is located; missing if no default
          origin: <string>, // 'default', 'verified', 'unverified', or 'invalid'; based on the presence and validity of the engine's loadPath verification hash.
          submissionURL: <string> // set for default engines or well known search domains
        },
        defaultPrivateSearchEngine: <string>, // e.g. "duckduckgo"
        defaultPrivateSearchEngine: {,
          // data about the current default engine for private browsing mode. Same as defaultSearchEngineData.
        },
        launcherProcessState: <integer>, // optional, values correspond to values of mozilla::LauncherRegistryInfo::EnabledState enum
        e10sEnabled: <bool>, // whether e10s is on, i.e. browser tabs open by default in a different process
        e10sMultiProcesses: <integer>, // Maximum number of processes that will be launched for regular web content
        fissionEnabled: <bool>, // whether fission is enabled this session, and subframes can load in a different process
        telemetryEnabled: <bool>, // false on failure
        locale: <string>, // e.g. "it", null on failure
        intl: {
          requestedLocales: [ <string>, ... ], // The locales that are being requested.
          availableLocales: [ <string>, ... ], // The locales that are available for use.
          appLocales: [ <string>, ... ], // The negotiated locales that are being used.
          systemLocales: [ <string>, ... ], // The locales for the OS.
          regionalPrefsLocales: [ <string>, ... ], // The regional preferences for the OS.
          acceptLanguages: [ <string>, ... ], // The languages for the Accept-Languages header.
        },
        update: {
          channel: <string>, // e.g. "release", null on failure
          enabled: <bool>, // true on failure
          autoDownload: <bool>, // true on failure
          background: <bool>, // Indicates whether updates may be installed when Firefox is not running.
        },
        userPrefs: {
          // Only prefs which are changed are listed in this block
          "pref.name.value": value // some prefs send the value
          "pref.name.url": "<user-set>" // For some privacy-sensitive prefs
            // only the fact that the value has been changed is recorded
        },
        attribution: { // optional, only present if the installation has attribution data
          // all of these values are optional.
          source: <string>, // referring partner domain, when install happens via a known partner
          medium: <string>, // category of the source, such as "organic" for a search engine
          campaign: <string>, // identifier of the particular campaign that led to the download of the product
          content: <string>, // identifier to indicate the particular link within a campaign
          variation: <string>, // name/id of the variation cohort used in the enrolled funnel experiment
          experiment: <string>, // name/id of the enrolled funnel experiment
          ua: <string>, // identifier derived from the user agent downloading the installer, e.g., chrome, Google Chrome 123
          dltoken: <string>, // Unique token created at Firefox download time. ex: c18f86a3-f228-4d98-91bb-f90135c0aa9c
        },
        sandbox: {
          effectiveContentProcessLevel: <integer>,
          contentWin32kLockdownState: <integer>,
        }
      },
      // Optional, missing if fetching the information failed or had not yet completed.
      services: {
        // True if the user has a firefox account
        accountEnabled: <bool>,
        // True if the user has sync enabled.
        syncEnabled: <bool>
      },
      profile: {
        creationDate: <integer>, // integer days since UNIX epoch, e.g. 16446
        resetDate: <integer>, // integer days since UNIX epoch, e.g. 16446 - optional
        firstUseDate: <integer>, // integer days since UNIX epoch, e.g. 16446 - optional
      },
      partner: { // This section may not be immediately available on startup
        distributionId: <string>, // pref "distribution.id", null on failure
        distributionVersion: <string>, // pref "distribution.version", null on failure
        partnerId: <string>, // pref mozilla.partner.id, null on failure
        distributor: <string>, // pref app.distributor, null on failure
        distributorChannel: <string>, // pref app.distributor.channel, null on failure
        partnerNames: [
          // list from prefs app.partner.<name>=<name>
        ],
      },
      system: {
        memoryMB: <number>,
        virtualMaxMB: <number>, // windows-only
        isWow64: <bool>, // windows-only
        isWowARM64: <bool>, // windows-only
        hasWinPackageId: <bool>, // windows-only
        winPackageFamilyName: <string>, // windows-only
        cpu: {
            count: <number>,  // desktop only, e.g. 8, or null on failure - logical cpus
            cores: <number>, // desktop only, e.g., 4, or null on failure - physical cores
            vendor: <string>, // desktop only, e.g. "GenuineIntel", or null on failure
            family: <number>, // desktop only, null on failure
            model: <number, // desktop only, null on failure
            stepping: <number>, // desktop only, null on failure
            l2cacheKB: <number>, // L2 cache size in KB, only on windows & mac
            l3cacheKB: <number>, // desktop only, L3 cache size in KB
            speedMHz: <number>, // desktop only, cpu clock speed in MHz
            extensions: [
              <string>,
              ...
              // as applicable:
              // "hasMMX", "hasSSE", "hasSSE2", "hasSSE3", "hasSSSE3",
              // "hasSSE4A", "hasSSE4_1", "hasSSE4_2", "hasAVX", "hasAVX2",
              // "hasAES", "hasEDSP", "hasARMv6", "hasARMv7", "hasNEON"
            ],
        },
        device: { // This section is only available on mobile devices.
          model: <string>, // the "device" from FHR, null on failure
          manufacturer: <string>, // null on failure
          hardware: <string>, // null on failure
          isTablet: <bool>, // null on failure
        },
        os: {
            name: <string>, // "Windows_NT" or null on failure
            version: <string>, // e.g. "6.1", null on failure
            kernelVersion: <string>, // android only or null on failure
            servicePackMajor: <number>, // windows only or null on failure
            servicePackMinor: <number>, // windows only or null on failure
            windowsBuildNumber: <number>, // windows only or null on failure
            windowsUBR: <number>, // windows 10 only or null on failure
            installYear: <number>, // windows only or null on failure
            locale: <string>, // "en" or null on failure
            hasPrefetch: <bool>, // windows only, or null on failure
            hasSuperfetch: <bool>, // windows only, or nul on failure
        },
        hdd: {
          profile: { // hdd where the profile folder is located
              model: <string>, // windows only or null on failure
              revision: <string>, // windows only or null on failure
              type: <string>, // "SSD" or "HDD" windows only or null on failure
          },
          binary:  { // hdd where the application binary is located
              model: <string>, // windows only or null on failure
              revision: <string>, // windows only or null on failure
              type: <string>, // "SSD" or "HDD" windows only or null on failure
          },
          system:  { // hdd where the system files are located
              model: <string>, // windows only or null on failure
              revision: <string>, // windows only or null on failure
              type: <string>, // "SSD" or "HDD" windows only or null on failure
          },
        },
        gfx: {
            D2DEnabled: <bool>, // null on failure
            DWriteEnabled: <bool>, // null on failure
            ContentBackend: <string> // One of "Cairo", "Skia", or "Direct2D 1.1"
            Headless: <bool>, // null on failure
            //DWriteVersion: <string>, // temporarily removed, pending bug 1154500
            adapters: [
              {
                description: <string>, // e.g. "Intel(R) HD Graphics 4600", null on failure
                vendorID: <string>, // null on failure
                deviceID: <string>, // null on failure
                subsysID: <string>, // null on failure
                RAM: <number>, // in MB, null on failure
                driver: <string>, // null on failure
                driverVendor: <string>, // null on failure
                driverVersion: <string>, // null on failure
                driverDate: <string>, // null on failure
                GPUActive: <bool>, // currently always true for the first adapter
              },
              ...
            ],
            // Note: currently only added on Desktop. On Linux, only a single
            // monitor is returned for the primary screen.
            monitors: [
              {
                screenWidth: <number>,  // screen width in pixels
                screenHeight: <number>, // screen height in pixels
                refreshRate: <number>,  // refresh rate in hertz (present on Windows only).
                                        //  (values <= 1 indicate an unknown value)
                pseudoDisplay: <bool>,  // networked screen (present on Windows only)
                scale: <number>,        // backing scale factor (present on Mac only)
              },
              ...
            ],
            features: {
              compositor: <string>,     // Layers backend for compositing (e.g. "d3d11", "none", "opengl", "webrender")

              // Each the following features can have one of the following statuses:
              //   "unused"      - This feature has not been requested.
              //   "unavailable" - Safe Mode or OS restriction prevents use.
              //   "blocked"     - Blocked due to an internal condition such as safe mode.
              //   "blacklisted" - Blocked due to a blacklist restriction.
              //   "denied"      - Blocked due to allowlist restrictions.
              //   "disabled"    - User explicitly disabled this default feature.
              //   "failed"      - This feature was attempted but failed to initialize.
              //   "available"   - User has this feature available.
              // The status can also include a ":" followed by a reason
              // e.g. "FEATURE_FAILURE_WEBRENDER_VIDEO_CRASH_INTEL_23.20.16.4973"
              d3d11: { // This feature is Windows-only.
                status: <string>,
                warp: <bool>,           // Software rendering (WARP) mode was chosen.
                textureSharing: <bool>  // Whether or not texture sharing works.
                version: <number>,      // The D3D11 device feature level.
                blacklisted: <bool>,    // Whether D3D11 is blacklisted; use to see whether WARP
                                        // was blacklist induced or driver-failure induced.
              },
              d2d: { // This feature is Windows-only.
                status: <string>,
                version: <string>,      // Either "1.0" or "1.1".
              },
              gpuProcess: { // Out-of-process compositing ("GPU process") feature
                status: <string>, // "Available" means currently in use
              },
              hwCompositing: { // hardware acceleration. i.e. whether we try using the GPU
                status: <string>
              },
              wrCompositor: { // native OS compositor (CA, DComp, etc.)
                status: <string>
              }
              wrSoftware: { // Software backend for WebRender, only computed when 'compositor' is 'webrender'
                status: <string>
              }
              openglCompositing: { // OpenGL compositing.
                status: <string>
              }
            },
          },
        appleModelId: <string>, // Mac only or null on failure
        sec: { // This feature is Windows 8+ only
          antivirus: [ <string>, ... ],    // null if unavailable on platform: Product name(s) of registered antivirus programs
          antispyware: [ <string>, ... ],  // null if unavailable on platform: Product name(s) of registered antispyware programs
          firewall: [ <string>, ... ],     // null if unavailable on platform: Product name(s) of registered firewall programs
        },
      },
      addons: {
        activeAddons: { // the currently enabled add-ons
          <addon id>: {
            blocklisted: <bool>,
            description: <string>, // null if not available
            name: <string>,
            userDisabled: <bool>,
            appDisabled: <bool>,
            version: <string>,
            scope: <integer>,
            type: <string>, // "extension", "service", ...
            foreignInstall: <bool>,
            hasBinaryComponents: <bool>,
            installDay: <number>, // days since UNIX epoch, 0 on failure
            updateDay: <number>, // days since UNIX epoch, 0 on failure
            signedState: <integer>, // whether the add-on is signed by AMO, only present for extensions
            isSystem: <bool>, // true if this is a System Add-on
            isWebExtension: <bool>, // true if this is a WebExtension
            multiprocessCompatible: <bool>, // true if this add-on does *not* require e10s shims
          },
          ...
        },
        theme: { // the active theme
          id: <string>,
          blocklisted: <bool>,
          description: <string>,
          name: <string>,
          userDisabled: <bool>,
          appDisabled: <bool>,
          version: <string>,
          scope: <integer>,
          foreignInstall: <bool>,
          hasBinaryComponents: <bool>
          installDay: <number>, // days since UNIX epoch, 0 on failure
          updateDay: <number>, // days since UNIX epoch, 0 on failure
        },
        activeGMPlugins: {
            <gmp id>: {
                version: <string>,
                userDisabled: <bool>,
                applyBackgroundUpdates: <integer>,
            },
            ...
        },
      },
      experiments: {
        "<experiment id>": { branch: "<branch>", type: "<type>", enrollmentId: "<id>" },
        // ...
      }
    }

build
-----

buildId
~~~~~~~
Firefox builds downloaded from mozilla.org use a 14-digit buildId. Builds included in other distributions may have a different format (e.g. only 10 digits).

Settings
--------

defaultSearchEngine
~~~~~~~~~~~~~~~~~~~
Note: Deprecated, use defaultSearchEngineData instead.

Contains the string identifier or name of the default search engine provider. This will not be present in environment data collected before the Search Service initialization.

The special value ``NONE`` could occur if there is no default search engine.

The special value ``UNDEFINED`` could occur if a default search engine exists but its identifier could not be determined.

This field's contents are ``Services.search.defaultEngine.identifier`` (if defined) or ``"other-"`` + ``Services.search.defaultEngine.name`` if not. In other words, search engines without an ``.identifier`` are prefixed with ``other-``.

defaultSearchEngineData
~~~~~~~~~~~~~~~~~~~~~~~
Contains data identifying the engine currently set as the default.

The object contains:

- a ``name`` property with the name of the engine, or ``NONE`` if no
  engine is currently set as the default.

- a ``loadPath`` property: an anonymized path of the engine xml file, e.g.
  jar:[app]/omni.ja!browser/engine.xml
  (where 'browser' is the name of the chrome package, not a folder)
  [profile]/searchplugins/engine.xml
  [distribution]/searchplugins/common/engine.xml
  [other]/engine.xml
  [other]/addEngineWithDetails
  [other]/addEngineWithDetails:extensionID
  [http/https]example.com/engine-name.xml
  [http/https]example.com/engine-name.xml:extensionID

- an ``origin`` property: the value will be ``default`` for engines that are built-in or from distribution partners, ``verified`` for user-installed engines with valid verification hashes, ``unverified`` for non-default engines without verification hash, and ``invalid`` for engines with broken verification hashes.

- a ``submissionURL`` property with the HTTP url we would use to search.
  For privacy, we don't record this for user-installed engines.

``loadPath`` and ``submissionURL`` are not present if ``name`` is ``NONE``.

defaultPrivateSearchEngineData
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This contains the data identifying the engine current set as the default for
private browsing mode. This may be the same engine as set for normal browsing
mode.

This object contains the same information as ``defaultSearchEngineData``. It
is only reported if the ``browser.search.separatePrivateDefault`` preference is
set to ``true``.

userPrefs
~~~~~~~~~

This object contains user preferences.

Each key in the object is the name of a preference. A key's value depends on the policy with which the preference was collected. There are three such policies, "value", "state", and "default value". For preferences collected under the "value" policy, the value will be the preference's value. For preferences collected under the "state" policy, the value will be an opaque marker signifying only that the preference has a user value. The "state" policy is therefore used when user privacy is a concern. For preferences collected under the "default value" policy, the value will be the preference's default value, if the preference exists. If the preference does not exist, there is no key or value.

The following is a partial list of `collected preferences <https://searchfox.org/mozilla-central/search?q=const+DEFAULT_ENVIRONMENT_PREFS&path=>`_.

- ``browser.search.suggest.enabled``: The "master switch" for search suggestions everywhere in Firefox (search bar, urlbar, etc.). Defaults to true.

- ``browser.urlbar.quicksuggest.onboardingDialogChoice``: The user's choice in the Firefox Suggest onboarding dialog. If the dialog was shown multiple times, this records the user's most recent choice. Values are the following. Empty string: The user has not made a choice (e.g., because the dialog hasn't been shown). ``accept``: The user accepted the dialog and opted in. ``settings``: The user clicked in the "Customize" button (the user remains opted out in this case). ``learn_more``: The user clicked "Learn more" (the user remains opted out). ``not_now_link``: The user clicked "Not now" (the user remains opted out). ``dismissed_escape_key``: The user dismissed the dialog by pressing the Escape key (the user remains opted out). ``dismissed_other``: The dialog was dismissed in some other unknown way, for example when the dialog is replaced with another higher priority dialog like the one shown when quitting the app (the user remains opted out).

- ``browser.urlbar.quicksuggest.dataCollection.enabled``: Whether the user has opted in to data collection for Firefox Suggest. This pref is set to true when the user opts in to the Firefox Suggest onboarding dialog modal. The user can also toggle the pref using a toggle switch in the Firefox Suggest preferences UI.

- ``browser.urlbar.suggest.quicksuggest.nonsponsored``: True if non-sponsored Firefox Suggest suggestions are enabled in the urlbar.

- ``browser.urlbar.suggest.quicksuggest.sponsored``: True if sponsored Firefox Suggest suggestions are enabled in the urlbar.

- ``browser.urlbar.suggest.searches``: True if search suggestions are enabled in the urlbar. Defaults to false.

- ``browser.zoom.full`` (deprecated): True if zoom is enabled for both text and images, that is if "Zoom Text Only" is not enabled. Defaults to true. This preference was collected in Firefox 50 to 52 (`Bug 979323 <https://bugzilla.mozilla.org/show_bug.cgi?id=979323>`_).

- ``security.tls.version.enable-deprecated``: True if deprecated versions of TLS (1.0 and 1.1) have been enabled by the user. Defaults to false.

- ``privacy.firstparty.isolate``: True if the user has changed the (unsupported, hidden) First Party Isolation preference. Defaults to false.

- ``privacy.resistFingerprinting``: True if the user has changed the (unsupported, hidden) Resist Fingerprinting preference. Defaults to false.

- ``toolkit.telemetry.pioneerId``: The state of the Pioneer ID. If set, then user is enrolled in Pioneer. Note that this does *not* collect the value.

- ``app.normandy.test-prefs.bool``: Test pref that will help troubleshoot uneven unenrollment in experiments. Defaults to false.

- ``app.normandy.test-prefs.integer``: Test pref that will help troubleshoot uneven unenrollment in experiments. Defaults to 0.

- ``app.normandy.test-prefs.string``: Test pref that will help troubleshoot uneven unenrollment in experiments. Defaults to "".

attribution
~~~~~~~~~~~

This object contains the attribution data for the product installation.

Attribution data is used to link installations of Firefox with the source that the user arrived at the Firefox download page from. It would indicate, for instance, when a user executed a web search for Firefox and arrived at the download page from there, directly navigated to the site, clicked on a link from a particular social media campaign, etc.

The attribution data is included in some versions of the default Firefox installer for Windows (the "stub" installer) and stored as part of the installation. All platforms other than Windows and also Windows installations that did not use the stub installer do not have this data and will not include the ``attribution`` object.

sandbox
~~~~~~~

This object contains data about the state of Firefox's sandbox.

Specific keys are:

- ``effectiveContentProcessLevel``: The meanings of the values are OS dependent. Details of the meanings can be found in the `Firefox prefs file <https://hg.mozilla.org/mozilla-central/file/tip/browser/app/profile/firefox.js>`_. The value here is the effective value, not the raw value, some platforms enforce a minimum sandbox level. If there is an error calculating this, it will be ``null``.
- ``contentWin32kLockdownState``: The status of Win32k Lockdown for Content process. 1 = "Lockdown enabled", 2 = "Lockdown disabled -- Missing WebRender", 3 = "Lockdown disabled -- Unsupported OS", 4 = "Lockdown disabled -- User pref not set". If there is an error calculating this, it will be ``null``.

profile
-------

creationDate
~~~~~~~~~~~~

The assumed creation date of this client's profile.
It's read from a file-stored timestamp from the client's profile directory.

.. note::

    If the timestamp file does not exist all files in the profile directory are scanned.
    The oldest creation or modification date of the scanned files is then taken to be the profile creation date.
    This has been shown to sometimes be inaccurate (`bug 1449739 <https://bugzilla.mozilla.org/show_bug.cgi?id=1449739>`_).

resetDate
~~~~~~~~~~~~

The time of the last reset time for the profile. If the profile has never been
reset this field will not be present.
It's read from a file-stored timestamp from the client's profile directory.

firstUseDate
~~~~~~~~~~~~

The time of the first use of profile. If this is an old profile where we can't
determine this this field will not be present.
It's read from a file-stored timestamp from the client's profile directory.

partner
-------

If the user is using a partner repack, this contains information identifying the repack being used, otherwise "partnerNames" will be an empty array and other entries will be null. The information may be missing when the profile just becomes available. In Firefox for desktop, the information along with other customizations defined in distribution.ini are processed later in the startup phase, and will be fully applied when "distribution-customization-complete" notification is sent.

Distributions are most reliably identified by the ``distributionId`` field. Partner information can be found in the `partner repacks <https://github.com/mozilla-partners>`_ (`the old one <https://hg.mozilla.org/build/partner-repacks/>`_ is deprecated): it contains one private repository per partner.
Important values for ``distributionId`` include:

- "MozillaOnline" for the Mozilla China repack.
- "canonical", for the `Ubuntu Firefox repack <http://bazaar.launchpad.net/~mozillateam/firefox/firefox.trusty/view/head:/debian/distribution.ini>`_.
- "yandex", for the Firefox Build by Yandex.

system
------

os
~~

This object contains operating system information.

- ``name``: the name of the OS.
- ``version``: a string representing the OS version.
- ``kernelVersion``: an Android only string representing the kernel version.
- ``servicePackMajor``: the Windows only major version number for the installed service pack.
- ``servicePackMinor``: the Windows only minor version number for the installed service pack.
- ``windowsBuildNumber``: the Windows build number.
- ``windowsUBR``: the Windows UBR number, only available for Windows >= 10. This value is incremented by Windows cumulative updates patches.
- ``installYear``: the Windows only integer representing the year the OS was installed.
- ``locale``: the string representing the OS locale.
- ``hasPrefetch``: the Windows-only boolean representing whether or not the OS-based prefetch application start-up optimization is set to use the default settings.
- ``hasSuperfetch``: the Windows-only boolean representing whether or not the OS-based superfetch application start-up optimization service is running and using the default settings.

addons
------

activeAddons
~~~~~~~~~~~~

Starting from Firefox 44, the length of the following string fields: ``name``, ``description`` and ``version`` is limited to 100 characters. The same limitation applies to the same fields in ``theme``.

Some of the fields in the record for each add-on are not available during startup.  The fields that will always be present are ``id``, ``version``, ``type``, ``updateDate``, ``scope``, ``isSystem``, ``isWebExtension``, and ``multiprocessCompatible``.  All the other fields documented above become present shortly after the ``sessionstore-windows-restored`` observer topic is notified.

activeGMPPlugins
~~~~~~~~~~~~~~~~

Up-to-date information is not available immediately during startup. The field will be populated with dummy information until the blocklist is loaded. At the latest, this will happen just after the ``sessionstore-windows-restored`` observer topic is notified.

experiments
-----------
For each experiment we collect the

- ``id`` (Like ``hotfix-reset-xpi-verification-timestamp-1548973``, max length 100 characters)
- ``branch`` (Like ``control``, max length 100 characters)
- ``type`` (Optional. Like ``normandy-exp``, max length 20 characters)
- ``enrollmentId`` (Optional. Like ``5bae2134-e121-46c2-aa00-232f3f5855c5``, max length 40 characters)

In the event any of these fields are truncated, a warning is printed to the console

Note that this list includes other types of deliveries, including Normandy rollouts and Nimbus feature defaults.

Version History
---------------

- Firefox 88:

  - Removed ``addons.activePlugins`` as part of removing NPAPI plugin support. (`bug 1682030 <https://bugzilla.mozilla.org/show_bug.cgi?id=1682030>`_)

- Firefox 70:

  - Added ``experiments.<experiment id>.enrollmentId``. (`bug 1555172 <https://bugzilla.mozilla.org/show_bug.cgi?id=1555172>`_)

- Firefox 67:

  - Removed ``persona``. The ``addons.activeAddons`` list should be used instead. (`bug 1525511 <https://bugzilla.mozilla.org/show_bug.cgi?id=1525511>`_)

- Firefox 61:

  - Removed empty ``addons.activeExperiment`` (`bug 1452935 <https://bugzilla.mozilla.org/show_bug.cgi?id=1452935>`_).
