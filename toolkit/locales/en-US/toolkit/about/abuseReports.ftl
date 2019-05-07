# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

abuse-report-title-extension = Report This Extension to { -vendor-short-name }
abuse-report-title-theme = Report This Theme to { -vendor-short-name }
abuse-report-subtitle = What’s the issue?

# Variables:
#   $author-name (string) - Name of the add-on author
abuse-report-addon-authored-by = by <a data-l10n-name="author-name">{ $author-name }</a>

abuse-report-learnmore =
  Unsure what issue to select?
  <a data-l10n-name="learnmore-link">Learn more about reporting extensions and themes</a>

abuse-report-submit-description = Describe the problem (optional)
abuse-report-textarea =
  .placeholder = It’s easier for us to address a problem if we have specifics. Please describe what you’re experiencing. Thank you for helping us keep the web healthy.
abuse-report-submit-note =
  Note: Don’t include personal information (such as name, email address, phone number, physical address).
  { -vendor-short-name } keeps a permanent record of these reports.

## Panel buttons.

abuse-report-cancel-button = Cancel
abuse-report-next-button = Next
abuse-report-goback-button = Go back
abuse-report-submit-button = Submit

## Message bars descriptions.

## Variables:
##   $addon-name (string) - Name of the add-on
abuse-report-messagebar-aborted = Report for <span data-l10n-name="addon-name">{ $addon-name }</span> canceled.
abuse-report-messagebar-submitting = Sending report for <span data-l10n-name="addon-name">{ $addon-name }</span>.
abuse-report-messagebar-submitted = Thank you for submitting a report. Do you want to remove <span data-l10n-name="addon-name">{ $addon-name }</span>?
abuse-report-messagebar-removed-extension = Thank you for submitting a report. You’ve removed the extension <span data-l10n-name="addon-name">{ $addon-name }</span>.
abuse-report-messagebar-removed-theme = Thank you for submitting a report. You’ve removed the theme <span data-l10n-name="addon-name">{ $addon-name }</span>.
abuse-report-messagebar-error = There was an error sending the report for <span data-l10n-name="addon-name">{ $addon-name }</span>.
abuse-report-messagebar-error-recent-submit = The report for <span data-l10n-name="addon-name">{ $addon-name }</span> wasn’t sent because another report was submitted recently.

## Message bars actions.

abuse-report-messagebar-action-remove = Yes, Remove It
abuse-report-messagebar-action-keep = No, I’ll Keep It
abuse-report-messagebar-action-retry = Retry
abuse-report-messagebar-action-cancel = Cancel

## Abuse report reasons (optionally paired with related examples and/or suggestions)

abuse-report-damage-reason = Damages my computer and data
abuse-report-damage-example = Example: Injected malware or stole data

abuse-report-spam-reason = Creates spam or advertising
abuse-report-spam-example = Example: Insert ads on webpages

abuse-report-settings-reason = Changed my search engine, homepage, or new tab without informing or asking me
abuse-report-settings-suggestions = Before reporting the extension, you can try changing your settings:
abuse-report-settings-suggestions-search = Change your default search settings
abuse-report-settings-suggestions-homepage = Change your homepage and new tab

abuse-report-deceptive-reason = Pretend to be something it’s not
abuse-report-deceptive-example = Example: Misleading description or imagery

abuse-report-broken-reason-extension = Doesn’t work, breaks websites, or slows { -brand-product-name } down
abuse-report-broken-reason-theme = Doesn’t work or breaks browser display
abuse-report-broken-example =
  Example: Features are slow, hard to use, or don’t work; parts of websites won’t load or look unusual
abuse-report-broken-suggestions-extension =
  It sounds like you’ve identified a bug. In addition to submitting a report here, the best way
  to get a functionality issue resolved is to contact the extension developer.
  <a data-l10n-name="support-link">Visit the extension’s website</a> to get the developer information.
abuse-report-broken-suggestions-theme =
  It sounds like you’ve identified a bug. In addition to submitting a report here, the best way
  to get a functionality issue resolved is to contact the theme developer.
  <a data-l10n-name="support-link">Visit the theme’s website</a> to get the developer information.

abuse-report-policy-reason = Hateful, violent, or illegal content
abuse-report-policy-suggestions =
  Note: Copyright and trademark issues must be reported in a separate process.
  <a data-l10n-name="report-infringement-link">Use these instructions</a> to
  report the problem.

abuse-report-unwanted-reason = Never wanted this extension and can’t get rid of it
abuse-report-unwanted-example = Example: An application installed it without my permission

abuse-report-other-reason = Something else

