# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

contentanalysis-alert-title = Content Analysis

# Variables:
#   $content - Description of the content being warned about, such as "clipboard" or "aFile.txt"
contentanalysis-slow-agent-notification = The Content Analysis tool is taking a long time to respond for resource “{ $content }”
contentanalysis-slow-agent-dialog-title = Content analysis in progress

# Variables:
#   $content - Description of the content being warned about, such as "clipboard" or "aFile.txt"
contentanalysis-slow-agent-dialog-body = Content Analysis is analyzing resource “{ $content }”
contentanalysis-operationtype-clipboard = clipboard
contentanalysis-operationtype-dropped-text = dropped text

contentanalysis-notification-title = Content Analysis
# Variables:
#   $content - Description of the content being reported, such as "clipboard" or "aFile.txt"
#   $response - The response received from the content analysis agent, such as "REPORT_ONLY"
contentanalysis-genericresponse-message = Content Analysis responded with { $response } for resource: { $content }
# Variables:
#   $content - Description of the content being blocked, such as "clipboard" or "aFile.txt"
contentanalysis-block-message = Your organization uses data-loss prevention software that has blocked this content: { $content }.
# Variables:
#   $content - Description of the content being blocked, such as "clipboard" or "aFile.txt"
contentanalysis-error-message = An error occurred in communicating with the data-loss prevention software. Transfer denied for resource: { $content }.
