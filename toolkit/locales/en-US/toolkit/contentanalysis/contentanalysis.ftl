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
contentanalysis-operationtype-clipboard = clipboard
contentanalysis-operationtype-dropped-text = dropped text
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
#   $content - Description of the content being blocked, such as "clipboard" or "aFile.txt"
contentanalysis-unspecified-error-message = An error occurred in communicating with { $agent }. Transfer denied for resource: { $content }.
# Variables:
#   $agent - The name of the DLP agent doing the analysis
#   $content - Description of the content being blocked, such as "clipboard" or "aFile.txt"
contentanalysis-no-agent-connected-message = Unable to connect to { $agent }. Transfer denied for resource: { $content }.
# Variables:
#   $agent - The name of the DLP agent doing the analysis
#   $content - Description of the content being blocked, such as "clipboard" or "aFile.txt"
contentanalysis-invalid-agent-signature-message = Failed signature verification for { $agent }. Transfer denied for resource: { $content }.

contentanalysis-inprogress-quit-title = Quit { -brand-shorter-name }?
contentanalysis-inprogress-quit-message = Several actions are in progress. If you quit { -brand-shorter-name }, these actions will not be completed.
contentanalysis-inprogress-quit-yesbutton = Yes, quit
