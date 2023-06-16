# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## Headers used in the webextension permissions dialog,
## See https://bug1308309.bmoattachments.org/attachment.cgi?id=8814612
## for an example of the full dialog.
## Note: This string will be used as raw markup. Avoid characters like <, >, &
## Variables:
##   $extension (String): replaced with the localized name of the extension.

webext-perms-header = Add { $extension }?
webext-perms-header-with-perms = Add { $extension }? This extension will have permission to:
webext-perms-header-unsigned = Add { $extension }? This extension is unverified. Malicious extensions can steal your private information or compromise your computer. Only add it if you trust the source.
webext-perms-header-unsigned-with-perms = Add { $extension }? This extension is unverified. Malicious extensions can steal your private information or compromise your computer. Only add it if you trust the source. This extension will have permission to:
webext-perms-sideload-header = { $extension } added
webext-perms-optional-perms-header = { $extension } requests additional permissions.

##

webext-perms-add =
    .label = Add
    .accesskey = A
webext-perms-cancel =
    .label = Cancel
    .accesskey = C

webext-perms-sideload-text = Another program on your computer installed an add-on that may affect your browser. Please review this add-on’s permissions requests and choose to Enable or Cancel (to leave it disabled).
webext-perms-sideload-text-no-perms = Another program on your computer installed an add-on that may affect your browser. Please choose to Enable or Cancel (to leave it disabled).
webext-perms-sideload-enable =
    .label = Enable
    .accesskey = E
webext-perms-sideload-cancel =
    .label = Cancel
    .accesskey = C

# Variables:
#   $extension (String): replaced with the localized name of the extension.
webext-perms-update-text = { $extension } has been updated. You must approve new permissions before the updated version will install. Choosing “Cancel” will maintain your current extension version. This extension will have permission to:
webext-perms-update-accept =
    .label = Update
    .accesskey = U

webext-perms-optional-perms-list-intro = It wants to:
webext-perms-optional-perms-allow =
    .label = Allow
    .accesskey = A
webext-perms-optional-perms-deny =
    .label = Deny
    .accesskey = D

webext-perms-host-description-all-urls = Access your data for all websites

# Variables:
#   $domain (String): will be replaced by the DNS domain for which a webextension is requesting access (e.g., mozilla.org)
webext-perms-host-description-wildcard = Access your data for sites in the { $domain } domain

# Variables:
#   $domainCount (Number): Integer indicating the number of additional
#     hosts for which this webextension is requesting permission.
webext-perms-host-description-too-many-wildcards =
    { $domainCount ->
        [one] Access your data in { $domainCount } other domain
       *[other] Access your data in { $domainCount } other domains
    }
# Variables:
#   $domain (String): will be replaced by the DNS host name for which a webextension is requesting access (e.g., www.mozilla.org)
webext-perms-host-description-one-site = Access your data for { $domain }

# Variables:
#   $domainCount (Number): Integer indicating the number of additional
#     hosts for which this webextension is requesting permission.
webext-perms-host-description-too-many-sites =
    { $domainCount ->
        [one] Access your data on { $domainCount } other site
       *[other] Access your data on { $domainCount } other sites
    }

## Headers used in the webextension permissions dialog for synthetic add-ons.
## The part of the string describing what privileges the extension gives should be consistent
## with the value of webext-site-perms-description-gated-perms-{sitePermission}.
## Note, this string will be used as raw markup. Avoid characters like <, >, &
## Variables:
##   $hostname (String): the hostname of the site the add-on is being installed from.

webext-site-perms-header-with-gated-perms-midi = This add-on gives { $hostname } access to your MIDI devices.
webext-site-perms-header-with-gated-perms-midi-sysex = This add-on gives { $hostname } access to your MIDI devices (with SysEx support).

##

# This string is used as description in the webextension permissions dialog for synthetic add-ons.
# Note, the empty line is used to create a line break between the two sections.
# Note, this string will be used as raw markup. Avoid characters like <, >, &
webext-site-perms-description-gated-perms-midi =
    These are usually plug-in devices like audio synthesizers, but might also be built into your computer.

    Websites are normally not allowed to access MIDI devices. Improper usage could cause damage or compromise security.

## Headers used in the webextension permissions dialog.
## Note: This string will be used as raw markup. Avoid characters like <, >, &
## Variables:
##   $extension (String): replaced with the localized name of the extension being installed.
##   $hostname (String): will be replaced by the DNS host name for which a webextension enables permissions.

webext-site-perms-header-with-perms = Add { $extension }? This extension grants the following capabilities to { $hostname }:
webext-site-perms-header-unsigned-with-perms = Add { $extension }? This extension is unverified. Malicious extensions can steal your private information or compromise your computer. Only add it if you trust the source. This extension grants the following capabilities to { $hostname }:

## These should remain in sync with permissions.NAME.label in sitePermissions.properties

webext-site-perms-midi = Access MIDI devices
webext-site-perms-midi-sysex = Access MIDI devices with SysEx support
