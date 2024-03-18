Softvision Mobile QA - Fenix testing tasks and process
=============

Overview
--------

## Release
- Frequency: Fenix release schedule
- Tasks performed by the QA team:
  - Smoke and sanity testing
  - Exploratory testing
  - Localization testing
  - Bug triage
- Specific Release testing tasks: none
- Feature coverage: yes
- Bug verification coverage: uplifts


## Beta
- Frequency: Fenix release schedule
- Tasks performed by the QA team:
  - Smoke and sanity testing
  - Exploratory testing
  - Localization testing
  - Bug triage
- Specific Beta testing tasks:
  - Full functional & UI testing
  - TalkBack & Accessibility testing
  - Full Search testing
- Feature coverage: yes
- Bug verification coverage: uplifts

## Nightly
- Frequency: daily
- Tasks performed by the QA team
  - Smoke and sanity testing
  - Exploratory testing
  - Bug triage
- Specific Nightly testing tasks:
  - Bug verification (qa label)
  - Feature testing
  - Test case creation (including a11y)
- Feature coverage: yes
- Bug verification coverage: yes

### Device defaults
- Device coverage: (unless otherwise specified): Pixel, Samsung, Xiaomi, OnePlus, Huawei
  - Phone & tablets
  - Android version: all

## Detailed informations about the tasks performed

#### Full-functional & UI testing
- Duration: 2 days
- Frequency:
  - Upon Geckoview release (Beta 1)
  - After Geckoview release, depending on the issues uplifted (if > 10 issues)
- Description:
  - Set of tests that cover all functionalities
  - 2 runs: 1 tablet, and 1 for phone

#### Smoke & sanity testing
- Duration: 1 day
- Frequency:
  - Release & Beta: Fenix release schedule
  - Nightly: 2-3  times per week (depending of other tasks priority)
- Description:
  - Small suite of tests focused on all major functionalities

#### Feature testing
- Duration: based on feature complexity
- Frequency: when a new feature is implemented
- Description:
  - Creation of test cases (a11y included)
  - Feature bug verification (also duplicates, if it is the case)
  - Exploratory testing around the new implementation and different areas that might be affected

#### Bug verification (qa label & uplifts)
- Duration: based on bug complexity
- Frequency: daily/when qa label is added to fixed bugs
- Description:
  - Different devices covered
  - Verify the steps provided in the description on an affected build, in order to reproduce the bug (if it wasn't earlier) and on the build that contains the patch, to confirm the fix.

#### Localization testing
- Duration
  - Beta: 9hrs
  - Release: 6hrs
- Frequency: Upon Geckoview release
- Description
  - Suite of tests based on the most important languages and pseudo locale tests.
  - Additionally, the number of languages listed ( Fenix settings) are verified to be the same number as in Pontoon.

#### Search testing
- Duration: 1 day
- Frequency: Upon Geckoview release
- Description
  - Set of tests that cover the interaction of users with URL bar, search engines & search codes (VPN).

#### Accessibility testing
- Duration
  - TalkBack: 1,5 day
  - Scanner app: 1 day
- Frequency: Upon Geckoview release
- Description
  - Tests are focused on the important functionalities
  - TalkBack: check for issues when interacting with the app and the description of actions that are being performed
  - Scanner: Menus, snackbars, others are being scanned in order to find suggestions for text contrast and touch target size

#### Bug triage
- Duration: based on issue complexity
- Frequency:
  - Daily/depending on the impact logged by the users
- Description:
  - Issue investigation based on the information provided by the user

#### Exploratory testing
- Duration: based on area tested
- Frequency (performed with):
  - Smoke & sanity testing
  - Full functional & UI tests
  - Bug verification
  - Bug triage
- Description:
  - Testing scenarios that are not covered in test runs
