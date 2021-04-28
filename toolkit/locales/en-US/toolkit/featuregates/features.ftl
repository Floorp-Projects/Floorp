# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# The title of the experiment should be kept in English as it may be referenced
# by various online articles and is technical in nature.
experimental-features-css-masonry2 =
    .label = CSS: Masonry Layout
experimental-features-css-masonry-description = Enables support for the experimental CSS Masonry Layout feature. See the <a data-l10n-name="explainer">explainer</a> for a high level description of the feature. To provide feedback, please comment in <a data-l10n-name="w3c-issue">this GitHub issue</a> or <a data-l10n-name="bug">this bug</a>.

# The title of the experiment should be kept in English as it may be referenced
# by various online articles and is technical in nature.
experimental-features-web-gpu2 =
    .label = Web API: WebGPU
experimental-features-web-gpu-description2 = This new API provides low-level support for performing computation and graphics rendering using the <a data-l10n-name="wikipedia">Graphics Processing Unit (GPU)</a> of the user’s device or computer. The <a data-l10n-name="spec">specification</a> is still a work-in-progress. See <a data-l10n-name="bugzilla">bug 1602129</a> for more details.

# The title of the experiment should be kept in English as it may be referenced
# by various online articles and is technical in nature.
experimental-features-media-avif =
    .label = Media: AVIF
experimental-features-media-avif-description = With this feature enabled, { -brand-short-name } supports the AV1 Image File (AVIF) format. This is a still image file format that leverages the capabilities of the AV1 video compression algorithms to reduce image size. See <a data-l10n-name="bugzilla">bug 1443863</a> for more details.

# The title of the experiment should be kept in English as it may be referenced
# by various online articles and is technical in nature.
experimental-features-web-api-inputmode =
    .label = Web API: inputmode
# "inputmode" and "contenteditable" are technical terms and shouldn't be translated.
experimental-features-web-api-inputmode-description = Our implementation of the <a data-l10n-name="mdn-inputmode">inputmode</a> global attribute has been updated as per <a data-l10n-name="whatwg">the WHATWG specification</a>, but we still need to make other changes too, like making it available on contenteditable content. See <a data-l10n-name="bugzilla">bug 1205133</a> for more details.

# The title of the experiment should be kept in English as it may be referenced
# by various online articles and is technical in nature.
experimental-features-css-constructable-stylesheets =
    .label = CSS: Constructable Stylesheets
experimental-features-css-constructable-stylesheets-description = The addition of a constructor to the <a data-l10n-name="mdn-cssstylesheet">CSSStyleSheet</a> interface as well as a variety of related changes makes it possible to directly create new stylesheets without having to add the sheet to the HTML. This makes it much easier to create reusable stylesheets for use with <a data-l10n-name="mdn-shadowdom">Shadow DOM</a>. See <a data-l10n-name="bugzilla">bug 1520690</a> for more details.

experimental-features-devtools-color-scheme-simulation =
    .label = Developer Tools: Color Scheme Simulation
experimental-features-devtools-color-scheme-simulation-description = Adds an option to simulate different color schemes allowing you to test <a data-l10n-name="mdn-preferscolorscheme">@prefers-color-scheme</a> media queries. Using this media query lets your stylesheet respond to whether the user prefers a light or dark user interface. This feature lets you test your code without having to change settings in your browser (or operating system, if the browser follows a system-wide color scheme setting). See <a data-l10n-name="bugzilla1">bug 1550804</a> and <a data-l10n-name="bugzilla2">bug 1137699</a> for more details.

experimental-features-devtools-execution-context-selector =
    .label = Developer Tools: Execution Context Selector
experimental-features-devtools-execution-context-selector-description = This feature displays a button on the console’s command line that lets you change the context in which the expression you enter will be executed. See <a data-l10n-name="bugzilla1">bug 1605154</a> and <a data-l10n-name="bugzilla2">bug 1605153</a> for more details.

experimental-features-devtools-compatibility-panel =
    .label = Developer Tools: Compatibility Panel
experimental-features-devtools-compatibility-panel-description = A side panel for the Page Inspector that shows you information detailing your app’s cross-browser compatibility status. See <a data-l10n-name="bugzilla">bug 1584464</a> for more details.

# Do not translate 'SameSite', 'Lax' and 'None'.
experimental-features-cookie-samesite-lax-by-default2 =
    .label = Cookies: SameSite=Lax by default
experimental-features-cookie-samesite-lax-by-default2-description = Treat cookies as “SameSite=Lax” by default if no “SameSite” attribute is specified. Developers must opt-in to the current status quo of unrestricted use by explicitly asserting “SameSite=None”.

# Do not translate 'SameSite', 'Lax' and 'None'.
experimental-features-cookie-samesite-none-requires-secure2 =
    .label = Cookies: SameSite=None requires secure attribute
experimental-features-cookie-samesite-none-requires-secure2-description = Cookies with “SameSite=None” attribute require the secure attribute. This feature requires “Cookies: SameSite=Lax by default”.

# about:home should be kept in English, as it refers to the the URI for
# the internal default home page.
experimental-features-abouthome-startup-cache =
    .label = about:home startup cache
experimental-features-abouthome-startup-cache-description = A cache for the initial about:home document that is loaded by default at startup. The purpose of the cache is to improve startup performance.

# The title of the experiment should be kept in English as it may be referenced
# by various online articles and is technical in nature.
experimental-features-cookie-samesite-schemeful =
    .label = Cookies: Schemeful SameSite
experimental-features-cookie-samesite-schemeful-description = Treat cookies from the same domain, but with different schemes (e.g. http://example.com and https://example.com) as cross-site instead of same-site. Improves security, but potentially introduces breakage.

# "Service Worker" is an API name and is usually not translated.
experimental-features-devtools-serviceworker-debugger-support =
    .label = Developer Tools: Service Worker debugging
# "Service Worker" is an API name and is usually not translated.
experimental-features-devtools-serviceworker-debugger-support-description = Enables experimental support for Service Workers in the Debugger panel. This feature may slow the Developer Tools down and increase memory consumption.

# WebRTC global mute toggle controls
experimental-features-webrtc-global-mute-toggles =
    .label = WebRTC Global Mute Toggles
experimental-features-webrtc-global-mute-toggles-description = Add controls to the WebRTC global sharing indicator that allow users to globally mute their microphone and camera feeds.

# Win32k Lockdown
experimental-features-win32k-lockdown =
    .label = Win32k Lockdown
experimental-features-win32k-lockdown-description = Disable use of Win32k APIs in browser tabs. Provides an increase in security but may currently be unstable or glitchy. (Windows only)

# JS JIT Warp project
experimental-features-js-warp =
    .label = JavaScript JIT: Warp
experimental-features-js-warp-description = Enable Warp, a project to improve JavaScript performance and memory usage.

# Fission is the name of the feature and should not be translated.
experimental-features-fission =
    .label = Fission (Site Isolation)
experimental-features-fission-description = Fission (site isolation) is an experimental feature in { -brand-short-name } to provide an additional layer of defense against security bugs. By isolating each site into a separate process, Fission makes it harder for malicious websites to get access to information from other pages you are visiting. This is a major architectural change in { -brand-short-name } and we appreciate you testing and reporting any issues you might encounter. For more details, see <a data-l10n-name="wiki">the wiki</a>.

# Support for having multiple Picture-in-Picture windows open simultaneously
experimental-features-multi-pip =
    .label = Multiple Picture-in-Picture Support
experimental-features-multi-pip-description = Experimental support for allowing multiple Picture-in-Picture windows to be open at the same time.

experimental-features-http3 =
    .label = HTTP/3 protocol
experimental-features-http3-description = Experimental support for the HTTP/3 protocol.

# Search during IME
experimental-features-ime-search =
    .label = Address Bar: show results during IME composition
experimental-features-ime-search-description = An IME (Input Method Editor) is a tool that allows you to enter complex symbols, such as those used in East Asian or Indic written languages, using a standard keyboard. Enabling this experiment will keep the address bar panel open, showing search results and suggestions, while using IME to input text. Note that the IME might display a panel that covers the address bar results, therefore this preference is only suggested for IME not using this type of panel.
