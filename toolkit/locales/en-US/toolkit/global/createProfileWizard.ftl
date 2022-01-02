# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

create-profile-window =
  .title = Create Profile Wizard
  .style = width: 45em; height: 32em;

## First wizard page

create-profile-first-page-header =
  { PLATFORM() ->
    [macos] Introduction
   *[other] Welcome to the { create-profile-window.title }
  }

profile-creation-explanation-1 = { -brand-short-name } stores information about your settings and preferences in your personal profile.

profile-creation-explanation-2 = If you are sharing this copy of { -brand-short-name } with other users, you can use profiles to keep each user’s information separate. To do this, each user should create his or her own profile.

profile-creation-explanation-3 = If you are the only person using this copy of { -brand-short-name }, you must have at least one profile. If you would like, you can create multiple profiles for yourself to store different sets of settings and preferences. For example, you may want to have separate profiles for business and personal use.

profile-creation-explanation-4 =
  { PLATFORM() ->
    [macos] To begin creating your profile, click Continue.
   *[other] To begin creating your profile, click Next.
  }

## Second wizard page

create-profile-last-page-header =
  { PLATFORM() ->
    [macos] Conclusion
   *[other] Completing the { create-profile-window.title }
  }

profile-creation-intro = If you create several profiles you can tell them apart by the profile names. You may use the name provided here or use one of your own.

profile-prompt = Enter new profile name:
  .accesskey = E

profile-default-name =
  .value = Default User

profile-directory-explanation = Your user settings, preferences and other user-related data will be stored in:

create-profile-choose-folder =
  .label = Choose Folder…
  .accesskey = C

create-profile-use-default =
  .label = Use Default Folder
  .accesskey = U
