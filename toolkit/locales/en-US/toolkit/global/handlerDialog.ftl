# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## Permission Dialog
## Variables:
##  $host - the hostname that is initiating the request
##  $scheme - the type of link that's being opened.
##  $appName - Name of the application that will be opened.

permission-dialog-description =
  Allow this site to open the { $scheme } link?

permission-dialog-description-file =
  Allow this file to open the { $scheme } link?

permission-dialog-description-host =
  Allow { $host } to open the { $scheme } link?

permission-dialog-description-app =
  Allow this site to open the { $scheme } link with { $appName }?

permission-dialog-description-host-app =
  Allow { $host } to open the { $scheme } link with { $appName }?

permission-dialog-description-file-app =
  Allow this file to open the { $scheme } link with { $appName }?

## Please keep the emphasis around the hostname and scheme (ie the
## `<strong>` HTML tags). Please also keep the hostname as close to the start
## of the sentence as your language's grammar allows.

permission-dialog-remember =
  Always allow <strong>{ $host }</strong> to open <strong>{ $scheme }</strong> links

permission-dialog-remember-file =
  Always allow this file to open <strong>{ $scheme }</strong> links

##

permission-dialog-btn-open-link =
      .label = Open Link
      .accessKey = O

permission-dialog-btn-choose-app =
      .label = Choose Application
      .accessKey = A

permission-dialog-unset-description = You’ll need to choose an application.

permission-dialog-set-change-app-link = Choose a different application.

## Chooser dialog
## Variables:
##  $scheme - the type of link that's being opened.

chooser-window =
      .title = Choose Application
      .style = min-width: 26em; min-height: 26em;

chooser-dialog =
      .buttonlabelaccept = Open Link
      .buttonaccesskeyaccept = O

chooser-dialog-description = Choose an application to open the { $scheme } link.

# Please keep the emphasis around the scheme (ie the `<strong>` HTML tags).
chooser-dialog-remember =
  Always use this application to open <strong>{ $scheme }</strong> links

chooser-dialog-remember-extra = {
  PLATFORM() ->
      [windows] This can be changed in { -brand-short-name }’s options.
     *[other] This can be changed in { -brand-short-name }’s preferences.
  }

choose-other-app-description = Choose other Application
choose-app-btn =
      .label = Choose…
      .accessKey = C
choose-other-app-window-title = Another Application…

# Displayed under the name of a protocol handler in the Launch Application dialog.
choose-dialog-privatebrowsing-disabled = Disabled in Private Windows
