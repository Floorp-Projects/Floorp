# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

contentanalysis-alert-title = Content Analysis

# Variables:
#   $content - Description of the content being warned about, such as "clipboard" or "aFile.txt"
contentanalysis-slow-agent-notification = The Content Analysis tool is taking a long time to respond for resource “{ $content }”
contentanalysis-slow-agent-dialog-header = Scan in progress

# Variables:
#   $agent - The name of the DLP agent doing the analysis
#   $filename - Name of the file being analyzed, such as "aFile.txt"
contentanalysis-slow-agent-dialog-body-file = { $agent } is reviewing “{ $filename }” against your organization’s data policies. This may take a moment.
# Variables:
#   $agent - The name of the DLP agent doing the analysis
contentanalysis-slow-agent-dialog-body-clipboard = { $agent } is reviewing what you pasted against your organization’s data policies. This may take a moment.
# Note that this is shown when the user drag and drops text into the browser.
# Variables:
#   $agent - The name of the DLP agent doing the analysis
contentanalysis-slow-agent-dialog-body-dropped-text = { $agent } is reviewing the text you dropped against your organization’s data policies. This may take a moment.
# Variables:
#   $agent - The name of the DLP agent doing the analysis
contentanalysis-slow-agent-dialog-body-print = { $agent } is reviewing what you printed against your organization’s data policies. This may take a moment.
contentanalysis-operationtype-clipboard = clipboard
contentanalysis-operationtype-dropped-text = dropped text
contentanalysis-operationtype-print = print
#   $filename - The filename associated with the request, such as "aFile.txt"
contentanalysis-customdisplaystring-description = upload of “{ $filename }”

contentanalysis-warndialogtitle = This content may be unsafe

# Variables:
#   $content - Description of the content being warned about, such as "clipboard" or "aFile.txt"
contentanalysis-warndialogtext = Your organization uses data-loss prevention software that has flagged this content as unsafe: { $content }. Use it anyway?
contentanalysis-warndialog-response-allow = Use content
contentanalysis-warndialog-response-deny = Cancel

contentanalysis-notification-title = Content Analysis
# Variables:
#   $content - Description of the content being reported, such as "clipboard" or "aFile.txt"
#   $response - The response received from the content analysis agent, such as "REPORT_ONLY"
contentanalysis-genericresponse-message = Content Analysis responded with { $response } for resource: { $content }
# Variables:
#   $content - Description of the content being blocked, such as "clipboard" or "aFile.txt"
contentanalysis-block-message = Your organization uses data-loss prevention software that has blocked this content: { $content }.
# Variables:
#   $agent - The name of the DLP agent doing the analysis
#   $content - Localized text describing the content being blocked, such as "Paste denied."
contentanalysis-unspecified-error-message-content = An error occurred in communicating with { $agent }. { $content }
# Variables:
#   $agent - The name of the DLP agent doing the analysis
#   $content - Localized text describing the content being blocked, such as "Paste denied."
contentanalysis-no-agent-connected-message-content = Unable to connect to { $agent }. { $content }
# Variables:
#   $agent - The name of the DLP agent doing the analysis
#   $content - Localized text describing the content being blocked, such as "Paste denied."
contentanalysis-invalid-agent-signature-message-content = Failed signature verification for { $agent }. { $content }
# Variables:
#   $filename - Name of the file that was blocked, such as "aFile.txt"
contentanalysis-error-message-upload-file = Upload of “{ $filename }” denied.
contentanalysis-error-message-dropped-text = Drag and drop denied.
contentanalysis-error-message-clipboard = Paste denied.
contentanalysis-error-message-print = Print denied.

contentanalysis-block-dialog-title-upload-file = You’re not permitted to upload this file
# Variables:
#   $filename - Name of the file that was blocked, such as "aFile.txt"
contentanalysis-block-dialog-body-upload-file = Under your organization’s data protection policies, you’re not permitted to upload the file “{ $filename }”. Contact your administrator for more info.
contentanalysis-block-dialog-title-clipboard = You’re not permitted to paste this content
contentanalysis-block-dialog-body-clipboard = Under your organization’s data protection policies, you’re not permitted to paste this content. Contact your administrator for more info.
contentanalysis-block-dialog-title-dropped-text = You’re not permitted to drop this content
contentanalysis-block-dialog-body-dropped-text = Under your organization’s data protection policies, you’re not permitted to drag and drop this content. Contact your administrator for more info.
contentanalysis-block-dialog-title-print = You’re not permitted to print this document
contentanalysis-block-dialog-body-print = Under your organization’s data protection policies, you’re not permitted to print this document. Contact your administrator for more info.

contentanalysis-inprogress-quit-title = Quit { -brand-shorter-name }?
contentanalysis-inprogress-quit-message = Several actions are in progress. If you quit { -brand-shorter-name }, these actions will not be completed.
contentanalysis-inprogress-quit-yesbutton = Yes, quit
