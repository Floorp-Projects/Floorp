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
experimental-features-web-api-link-preload =
    .label = Web API: <link rel="preload">
# Do not translate "rel", "preload" or "link" here, as they are all HTML spec
# values that do not get translated.
experimental-features-web-api-link-preload-description = The <a data-l10n-name="rel">rel</a> attribute with value <code>"preload"</code> on a <a data-l10n-name="link">&lt;link&gt;</a> element is intended to help provide performance gains by letting you download resources earlier in the page lifecycle, ensuring that they’re available earlier and are less likely to block page rendering. Read <a data-l10n-name="readmore">“Preloading content with <code>rel="preload"</code>”</a> or see <a data-l10n-name="bugzilla">bug 1583604</a> for more details.

# The title of the experiment should be kept in English as it may be referenced
# by various online articles and is technical in nature.
experimental-features-css-focus-visible =
    .label = CSS: Pseudo-class: :focus-visible
experimental-features-css-focus-visible-description = Allows focus styles to be applied to elements like buttons and form controls, only when they are focused using the keyboard (e.g. when tabbing between elements), and not when they are focused using a mouse or other pointing device. See <a data-l10n-name="bugzilla">bug 1617600</a> for more details.

# The title of the experiment should be kept in English as it may be referenced
# by various online articles and is technical in nature.
experimental-features-web-api-beforeinput =
    .label = Web API: beforeinput Event
# The terms "beforeinput", "input", "textarea", and "contenteditable" are technical terms
# and shouldn't be translated.
experimental-features-web-api-beforeinput-description = The global <a data-l10n-name="mdn-beforeinput">beforeinput</a> event is fired on an <a data-l10n-name="mdn-input">&lt;input&gt;</a> and <a data-l10n-name="mdn-textarea">&lt;textarea&gt;</a> elements, or any element whose <a data-l10n-name="mdn-contenteditable">contenteditable</a> attribute is enabled, immediately before the element’s value changes. The event allows web apps to override the browser’s default behavior for user interaction, e.g., web apps can cancel user input only for specific characters or can modify pasting styled text only with approved styles.

# The title of the experiment should be kept in English as it may be referenced
# by various online articles and is technical in nature.
experimental-features-css-constructable-stylesheets =
    .label = CSS: Constructable Stylesheets
experimental-features-css-constructable-stylesheets-description = The addition of a constructor to the <a data-l10n-name="mdn-cssstylesheet">CSSStyleSheet</a> interface as well as a variety of related changes makes it possible to directly create new stylesheets without having to add the sheet to the HTML. This makes it much easier to create reusable stylesheets for use with <a data-l10n-name="mdn-shadowdom">Shadow DOM</a>. See <a data-l10n-name="bugzilla">bug 1520690</a> for more details.

# The title of the experiment should be kept in English as it may be referenced
# by various online articles and is technical in nature.
experimental-features-media-session-api =
    .label = Web API: Media Session API
experimental-features-media-session-api-description = The entire { -brand-short-name } implementation of the Media Session API is currently experimental. This API is used to customize the handling of media-related notifications, to manage events and data useful for presenting a user interface for managing media playback, and to obtain media file metadata. See <a data-l10n-name="bugzilla">bug 1112032</a> for more details.

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

# Desktop zooming experiment
experimental-features-graphics-desktop-zooming =
    .label = Graphics: Smooth Pinch Zoom
experimental-features-graphics-desktop-zooming-description = Enable support for smooth pinch zooming on touchscreens and precision touch pads.
