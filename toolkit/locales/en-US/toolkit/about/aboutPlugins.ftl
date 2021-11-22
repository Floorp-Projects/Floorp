# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

title-label = About Plugins

installed-plugins-label = Installed plugins
no-plugins-are-installed-label = No installed plugins found

deprecation-description = Missing something? Some plugins are no longer supported. <a data-l10n-name="deprecation-link">Learn More.</a>

## The information of plugins
##
## Variables:
##   $pluginLibraries: the plugin library
##   $pluginFullPath: path of the plugin
##   $version: version of the plugin

file-dd = <span data-l10n-name="file">File:</span> { $pluginLibraries }
path-dd = <span data-l10n-name="path">Path:</span> { $pluginFullPath }
version-dd = <span data-l10n-name="version">Version:</span> { $version }

## These strings describe the state of plugins
##
## Variables:
##   $blockListState: show some special state of the plugin, such as blocked, outdated

state-dd-enabled = <span data-l10n-name="state">State:</span> Enabled
state-dd-enabled-block-list-state = <span data-l10n-name="state">State:</span> Enabled ({ $blockListState })
state-dd-Disabled = <span data-l10n-name="state">State:</span> Disabled
state-dd-Disabled-block-list-state = <span data-l10n-name="state">State:</span> Disabled ({ $blockListState })

mime-type-label = MIME Type
description-label = Description
suffixes-label = Suffixes

## Gecko Media Plugins (GMPs)

plugins-gmp-license-info = License information
plugins-gmp-privacy-info = Privacy Information

plugins-openh264-name = OpenH264 Video Codec provided by Cisco Systems, Inc.
plugins-openh264-description = This plugin is automatically installed by Mozilla to comply with the WebRTC specification and to enable WebRTC calls with devices that require the H.264 video codec. Visit https://www.openh264.org/ to view the codec source code and learn more about the implementation.

plugins-widevine-name = Widevine Content Decryption Module provided by Google Inc.
plugins-widevine-description = This plugin enables playback of encrypted media in compliance with the Encrypted Media Extensions specification. Encrypted media is typically used by sites to protect against copying of premium media content. Visit https://www.w3.org/TR/encrypted-media/ for more information on Encrypted Media Extensions.
