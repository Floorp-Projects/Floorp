
Environment
===========

The environment consists of data that is expected to be characteristic for performance and other behavior and not expected to change too often.

Changes to most of these data points are detected (where possible and sensible) and will lead to a session split in the :doc:`main-ping`.
The environment data may also be submitted by other ping types.

*Note:* This is not submitted with all ping types due to privacy concerns. This and other data is inspected under the `data collection policy <https://wiki.mozilla.org/Firefox/Data_Collection>`_.

Some parts of the environment must be fetched asynchronously at startup. We don't want other Telemetry components to block on waiting for the environment, so some items may be missing from it until the async fetching finished.
This currently affects the following sections:

- profile
- addons


Structure::

    {
      build: {
        applicationId: <string>, // nsIXULAppInfo.ID
        applicationName: <string>, // "Firefox"
        architecture: <string>, // e.g. "x86", build architecture for the active build
        architecturesInBinary: <string>, // e.g. "i386-x86_64", from nsIMacUtils.architecturesInBinary, only present for mac universal builds
        buildId: <string>, // e.g. "20141126041045"
        version: <string>, // e.g. "35.0"
        vendor: <string>, // e.g. "Mozilla"
        platformVersion: <string>, // e.g. "35.0"
        xpcomAbi: <string>, // e.g. "x86-msvc"
        hotfixVersion: <string>, // e.g. "20141211.01"
      },
      settings: {
        blocklistEnabled: <bool>, // true on failure
        isDefaultBrowser: <bool>, // null on failure, not available on Android
        e10sEnabled: <bool>, // false on failure
        telemetryEnabled: <bool>, // false on failure
        locale: <string>, // e.g. "it", null on failure
        update: {
          channel: <string>, // e.g. "release", null on failure
          enabled: <bool>, // true on failure
          autoDownload: <bool>, // true on failure
        },
        userPrefs: {
          // Only prefs which are changed from the default value are listed
          // in this block
          "pref.name.value": value // some prefs send the value
          "pref.name.url": "<user-set>" // For some privacy-sensitive prefs
            // only the fact that the value has been changed is recorded
        },
      },
      profile: { // This section is not available on Android.
        creationDate: <integer>, // integer days since UNIX epoch, e.g. 16446
        resetDate: <integer>, // integer days since UNIX epoch, e.g. 16446 - optional
      },
      partner: {
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
        isWow64: <bool>, // windows-only
        cpu: {
            count: <number>,  // e.g. 8, or null on failure
            vendor: <string>, // e.g. "GenuineIntel", or null on failure
            family: <string>, // null on failure
            model: <string>, // null on failure
            stepping: <string>, // null on failure
            extensions: [
              <string>,
              ...
              // as applicable:
              // "MMX", "SSE", "SSE2", "SSE3", "SSSE3", "SSE4A", "SSE4_1",
              // "SSE4_2", "EDSP", "ARMv6", "ARMv7", "NEON"
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
            kernelVersion: <string>, // android/b2g only or null on failure
            servicePackMajor: <number>, // windows only or null on failure
            servicePackMinor: <number>, // windows only or null on failure
            locale: <string>, // "en" or null on failure
        },
        hdd: {
          profile: { // hdd where the profile folder is located
              model: <string>, // windows only or null on failure
              revision: <string>, // windows only or null on failure
          },
          binary:  { // hdd where the application binary is located
              model: <string>, // windows only or null on failure
              revision: <string>, // windows only or null on failure
          },
          system:  { // hdd where the system files are located
              model: <string>, // windows only or null on failure
              revision: <string>, // windows only or null on failure
          },
        },
        gfx: {
            D2DEnabled: <bool>, // null on failure
            DWriteEnabled: <bool>, // null on failure
            //DWriteVersion: <string>, // temporarily removed, pending bug 1154500
            adapters: [
              {
                description: <string>, // e.g. "Intel(R) HD Graphics 4600", null on failure
                vendorID: <string>, // null on failure
                deviceID: <string>, // null on failure
                subsysID: <string>, // null on failure
                RAM: <number>, // in MB, null on failure
                driver: <string>, // null on failure
                driverVersion: <string>, // null on failure
                driverDate: <string>, // null on failure
                GPUActive: <bool>, // currently always true for the first adapter
              },
              ...
            ],
          },
      },
      addons: {
        activeAddons: { // the currently enabled addons
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
            hasBinaryComponents: <bool>
            installDay: <number>, // days since UNIX epoch, 0 on failure
            updateDay: <number>, // days since UNIX epoch, 0 on failure
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
        activePlugins: [
          {
            name: <string>,
            version: <string>,
            description: <string>,
            blocklisted: <bool>,
            disabled: <bool>,
            clicktoplay: <bool>,
            mimeTypes: [<string>, ...],
            updateDay: <number>, // days since UNIX epoch, 0 on failure
          },
          ...
        ],
        activeGMPlugins: {
            <gmp id>: {
                version: <string>,
                userDisabled: <bool>,
                applyBackgroundUpdates: <bool>,
            },
            ...
        ],
        activeExperiment: { // section is empty if there's no active experiment
            id: <string>, // id
            branch: <string>, // branch name
        },
        persona: <string>, // id of the current persona, null on GONK
      },
    }
