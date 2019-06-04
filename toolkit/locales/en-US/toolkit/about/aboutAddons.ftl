# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

addons-window =
    .title = Add-ons Manager

search-header =
    .placeholder = Search addons.mozilla.org
    .searchbuttonlabel = Search

search-header-shortcut =
    .key = f

loading-label =
    .value = Loading…

list-empty-installed =
    .value = You don’t have any add-ons of this type installed

list-empty-available-updates =
    .value = No updates found

list-empty-recent-updates =
    .value = You haven’t recently updated any add-ons

list-empty-find-updates =
    .label = Check For Updates

list-empty-button =
    .label = Learn more about add-ons

install-addon-from-file =
    .label = Install Add-on From File…
    .accesskey = I

help-button = Add-ons Support

preferences =
    { PLATFORM() ->
        [windows] { -brand-short-name } Options
       *[other] { -brand-short-name } Preferences
    }

tools-menu =
    .tooltiptext = Tools for all add-ons

show-unsigned-extensions-button =
    .label = Some extensions could not be verified

show-all-extensions-button =
    .label = Show all extensions

debug-addons =
    .label = Debug Add-ons
    .accesskey = b

cmd-show-details =
    .label = Show More Information
    .accesskey = S

cmd-find-updates =
    .label = Find Updates
    .accesskey = F

cmd-preferences =
    .label =
        { PLATFORM() ->
            [windows] Options
           *[other] Preferences
        }
    .accesskey =
        { PLATFORM() ->
            [windows] O
           *[other] P
        }

cmd-enable-theme =
    .label = Wear Theme
    .accesskey = W

cmd-disable-theme =
    .label = Stop Wearing Theme
    .accesskey = W

cmd-install-addon =
    .label = Install
    .accesskey = I

cmd-contribute =
    .label = Contribute
    .accesskey = C
    .tooltiptext = Contribute to the development of this add-on

discover-title = What are Add-ons?

discover-description =
    Add-ons are applications that let you personalize { -brand-short-name } with
    extra functionality or style. Try a time-saving sidebar, a weather notifier, or a themed look to make { -brand-short-name }
    your own.

discover-footer =
    When you’re connected to the internet, this pane will feature
    some of the best and most popular add-ons for you to try out.

detail-version =
    .label = Version

detail-last-updated =
    .label = Last Updated

detail-contributions-description = The developer of this add-on asks that you help support its continued development by making a small contribution.

detail-contributions-button = Contribute
    .title = Contribute to the development of this add-on
    .accesskey = C

detail-update-type =
    .value = Automatic Updates

detail-update-default =
    .label = Default
    .tooltiptext = Automatically install updates only if that’s the default

detail-update-automatic =
    .label = On
    .tooltiptext = Automatically install updates

detail-update-manual =
    .label = Off
    .tooltiptext = Don’t automatically install updates

# Used as a description for the option to allow or block an add-on in private windows.
detail-private-browsing-label = Run in Private Windows

detail-private-browsing-description2 = When allowed, the extension will have access to your online activities while private browsing. <label data-l10n-name="detail-private-browsing-learn-more">Learn more</label>

# Some add-ons may elect to not run in private windows by setting incognito: not_allowed in the manifest.  This
# cannot be overriden by the user.
detail-private-disallowed-label = Not Allowed in Private Windows
detail-private-disallowed-description = This extension does not run while private browsing. <label data-l10n-name="detail-private-browsing-learn-more">Learn more</label>

# Some special add-ons are privileged, run in private windows automatically, and this permission can't be revoked
detail-private-required-label = Requires Access to Private Windows
detail-private-required-description = This extension has access to your online activities while private browsing. <label data-l10n-name="detail-private-browsing-learn-more">Learn more</label>

detail-private-browsing-on =
    .label = Allow
    .tooltiptext = Enable in Private Browsing

detail-private-browsing-off =
    .label = Don’t Allow
    .tooltiptext = Disable in Private Browsing

detail-home =
    .label = Homepage

detail-home-value =
    .value = { detail-home.label }

detail-repository =
    .label = Add-on Profile

detail-repository-value =
    .value = { detail-repository.label }

detail-check-for-updates =
    .label = Check for Updates
    .accesskey = U
    .tooltiptext = Check for updates for this add-on

detail-show-preferences =
    .label =
        { PLATFORM() ->
            [windows] Options
           *[other] Preferences
        }
    .accesskey =
        { PLATFORM() ->
            [windows] O
           *[other] P
        }
    .tooltiptext =
        { PLATFORM() ->
            [windows] Change this add-on’s options
           *[other] Change this add-on’s preferences
        }

detail-rating =
    .value = Rating

addon-restart-now =
    .label = Restart now

disabled-unsigned-heading =
    .value = Some add-ons have been disabled

disabled-unsigned-description =
    The following add-ons have not been verified for use in { -brand-short-name }. You can
    <label data-l10n-name="find-addons">find replacements</label> or ask the developer to get them verified.

disabled-unsigned-learn-more = Learn more about our efforts to help keep you safe online.

disabled-unsigned-devinfo =
    Developers interested in getting their add-ons verified can continue by reading our
    <label data-l10n-name="learn-more">manual</label>.

plugin-deprecation-description =
    Missing something? Some plugins are no longer supported by { -brand-short-name }. <label data-l10n-name="learn-more">Learn More.</label>

legacy-warning-show-legacy = Show legacy extensions

legacy-extensions =
    .value = Legacy Extensions

legacy-extensions-description =
    These extensions do not meet current { -brand-short-name } standards so they have been deactivated. <label data-l10n-name="legacy-learn-more">Learn about the changes to add-ons</label>

private-browsing-description2 =
    { -brand-short-name } is changing how extensions work in private browsing. Any new extensions you add to
    { -brand-short-name } won’t run by default in Private Windows. Unless you allow it in settings, the
    extension won’t work while private browsing, and won’t have access to your online activities
    there. We’ve made this change to keep your private browsing private.
    <label data-l10n-name="private-browsing-learn-more">Learn how to manage extension settings</label>

extensions-view-discopane =
    .name = Recommendations
    .tooltiptext = { extensions-view-discopane.name }

extensions-view-recent-updates =
    .name = Recent Updates
    .tooltiptext = { extensions-view-recent-updates.name }

extensions-view-available-updates =
    .name = Available Updates
    .tooltiptext = { extensions-view-available-updates.name }

## These are global warnings

extensions-warning-safe-mode-label =
    .value = All add-ons have been disabled by safe mode.
extensions-warning-safe-mode-container =
    .tooltiptext = { extensions-warning-safe-mode-label.value }

extensions-warning-check-compatibility-label =
    .value = Add-on compatibility checking is disabled. You may have incompatible add-ons.
extensions-warning-check-compatibility-container =
    .tooltiptext = { extensions-warning-check-compatibility-label.value }

extensions-warning-check-compatibility-enable =
    .label = Enable
    .tooltiptext = Enable add-on compatibility checking

extensions-warning-update-security-label =
    .value = Add-on update security checking is disabled. You may be compromised by updates.
extensions-warning-update-security-container =
    .tooltiptext = { extensions-warning-update-security-label.value }

extensions-warning-update-security-enable =
    .label = Enable
    .tooltiptext = Enable add-on update security checking

## Strings connected to add-on updates

extensions-updates-check-for-updates =
    .label = Check for Updates
    .accesskey = C

extensions-updates-view-updates =
    .label = View Recent Updates
    .accesskey = V

# This menu item is a checkbox that toggles the default global behavior for
# add-on update checking.

extensions-updates-update-addons-automatically =
    .label = Update Add-ons Automatically
    .accesskey = A

## Specific add-ons can have custom update checking behaviors ("Manually",
## "Automatically", "Use default global behavior"). These menu items reset the
## update checking behavior for all add-ons to the default global behavior
## (which itself is either "Automatically" or "Manually", controlled by the
## extensions-updates-update-addons-automatically.label menu item).

extensions-updates-reset-updates-to-automatic =
    .label = Reset All Add-ons to Update Automatically
    .accesskey = R

extensions-updates-reset-updates-to-manual =
    .label = Reset All Add-ons to Update Manually
    .accesskey = R

## Status messages displayed when updating add-ons

extensions-updates-updating =
    .value = Updating add-ons
extensions-updates-installed =
    .value = Your add-ons have been updated.
extensions-updates-downloaded =
    .value = Your add-on updates have been downloaded.
extensions-updates-restart =
    .label = Restart now to complete installation
extensions-updates-none-found =
    .value = No updates found
extensions-updates-manual-updates-found =
    .label = View Available Updates
extensions-updates-update-selected =
    .label = Install Updates
    .tooltiptext = Install available updates in this list

## Extension shortcut management

manage-extensions-shortcuts =
    .label = Manage Extension Shortcuts
    .accesskey = S
shortcuts-no-addons = You don’t have any extensions enabled.
shortcuts-no-commands = The following extensions do not have shortcuts:
shortcuts-input =
  .placeholder = Type a shortcut

shortcuts-browserAction = Activate extension
shortcuts-pageAction = Activate page action
shortcuts-sidebarAction = Toggle the sidebar

shortcuts-modifier-mac = Include Ctrl, Alt, or ⌘
shortcuts-modifier-other = Include Ctrl or Alt
shortcuts-invalid = Invalid combination
shortcuts-letter = Type a letter
shortcuts-system = Can’t override a { -brand-short-name } shortcut
# String displayed when a keyboard shortcut is already used by another add-on
# Variables:
#   $addon (string) - Name of the add-on
shortcuts-exists = Already in use by { $addon }

shortcuts-card-expand-button =
    { $numberToShow ->
        *[other] Show { $numberToShow } More
    }

shortcuts-card-collapse-button = Show Less

go-back-button =
    .tooltiptext = Go back

## Recommended add-ons page

# Explanatory introduction to the list of recommended add-ons. The action word
# ("recommends") in the final sentence is a link to external documentation.
discopane-intro =
    Extensions and themes are like apps for your browser, and they let you
    protect passwords, download videos, find deals, block annoying ads, change
    how your browser looks, and much more. These small software programs are
    often developed by a third party. Here’s a selection { -brand-product-name }
    <a data-l10n-name="learn-more-trigger">recommends</a> for exceptional
    security, performance, and functionality.

# Notice to make user aware that the recommendations are personalized.
discopane-notice-recommendations =
    Some of these recommendations are personalized. They are based on other
    extensions you’ve installed, profile preferences, and usage statistics.
discopane-notice-learn-more = Learn more

privacy-policy = Privacy Policy

# Refers to the author of an add-on, shown below the name of the add-on.
# Variables:
#   $author (string) - The name of the add-on developer.
created-by-author = by <a data-l10n-name="author">{ $author }</a>
# Shows the number of daily users of the add-on.
# Variables:
#   $dailyUsers (number) - The number of daily users.
user-count = Users: { $dailyUsers }
install-extension-button = Add to { -brand-product-name }
install-theme-button = Install Theme
# The label of the button that appears after installing an add-on. Upon click,
# the detailed add-on view is opened, from where the add-on can be managed.
manage-addon-button = Manage
find-more-addons = Find more add-ons

## Add-on actions
report-addon-button = Report
remove-addon-button = Remove
disable-addon-button = Disable
enable-addon-button = Enable
expand-addon-button = More Options
preferences-addon-button =
    { PLATFORM() ->
        [windows] Options
       *[other] Preferences
    }
details-addon-button = Details
release-notes-addon-button = Release Notes
permissions-addon-button = Permissions

addons-enabled-heading = Enabled
addons-disabled-heading = Disabled

ask-to-activate-button = Ask to Activate
always-activate-button = Always Activate
never-activate-button = Never Activate

addon-detail-author-label = Author
addon-detail-version-label = Version
addon-detail-last-updated-label = Last Updated
addon-detail-homepage-label = Homepage
addon-detail-rating-label = Rating

# The average rating that the add-on has received.
# Variables:
#   $rating (number) - A number between 0 and 5. The translation should show at most one digit after the comma.
five-star-rating =
  .title = Rated { NUMBER($rating, maximumFractionDigits: 1) } out of 5

# This string is used to show that an add-on is disabled.
# Variables:
#   $name (string) - The name of the add-on
addon-name-disabled = { $name } (disabled)

# The number of reviews that an add-on has received on AMO.
# Variables:
#   $numberOfReviews (number) - The number of reviews received
addon-detail-reviews-link =
    { $numberOfReviews ->
        [one] { $numberOfReviews } review
       *[other] { $numberOfReviews } reviews
    }

## Pending uninstall message bar

# Variables:
#   $addon (string) - Name of the add-on
pending-uninstall-description = <span data-l10n-name="addon-name">{ $addon }</span> has been removed.
pending-uninstall-undo-button = Undo

addon-detail-updates-label = Allow automatic updates
addon-detail-updates-radio-default = Default
addon-detail-updates-radio-on = On
addon-detail-updates-radio-off = Off
addon-detail-update-check-label = Check for Updates
install-update-button = Update

# This is the tooltip text for the private browsing badge in about:addons. The
# badge is the private browsing icon included next to the extension's name.
addon-badge-private-browsing-allowed =
    .title = Allowed in private windows
addon-detail-private-browsing-help = When allowed, the extension will have access to your online activities while private browsing. <a data-l10n-name="learn-more">Learn more</a>
addon-detail-private-browsing-allow = Allow
addon-detail-private-browsing-disallow = Don’t Allow

# This is the tooltip text for the recommended badge for an extension in about:addons. The
# badge is a small icon displayed next to an extension when it is recommended on AMO.
addon-badge-recommended =
  .title = Recommended
  .alt = Recommended

available-updates-heading = Available Updates
recent-updates-heading = Recent Updates

release-notes-loading = Loading…
release-notes-error = Sorry, but there was an error loading the release notes.

addon-permissions-empty = This extension doesn’t require any permissions

recommended-extensions-heading = Recommended Extensions
recommended-themes-heading = Recommended Themes

# A recommendation for the Firefox Color theme shown at the bottom of the theme
# list view. The "Firefox Color" name itself should not be translated.
recommended-theme-1 = Feeling creative? <a data-l10n-name="link">Build your own theme with Firefox Color.</a>
