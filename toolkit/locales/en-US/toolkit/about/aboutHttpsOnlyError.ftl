# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

about-httpsonly-title-alert = HTTPS-Only Mode Alert
about-httpsonly-title-connection-not-available = Secure Connection Not Available

# Variables:
#   $websiteUrl (String) - Url of the website that failed to load. Example: www.example.com
about-httpsonly-explanation-unavailable2 = You’ve enabled HTTPS-Only Mode for enhanced security, and a HTTPS version of <em>{ $websiteUrl }</em> is not available.
about-httpsonly-explanation-question = What could be causing this?
about-httpsonly-explanation-nosupport = Most likely, the website simply does not support HTTPS.
about-httpsonly-explanation-risk = It’s also possible that an attacker is involved. If you decide to visit the website, you should not enter any sensitive information like passwords, emails, or credit card details.
about-httpsonly-explanation-continue = If you continue, HTTPS-Only Mode will be turned off temporarily for this site.

about-httpsonly-button-continue-to-site = Continue to HTTP Site
about-httpsonly-button-go-back = Go Back
about-httpsonly-link-learn-more = Learn More…

## Suggestion Box that only shows up if a secure connection to www can be established
## Variables:
##   $websiteUrl (String) - Url of the website that can be securely loded with these alternatives. Example: example.com

about-httpsonly-suggestion-box-header = Possible Alternative
about-httpsonly-suggestion-box-www-text = There is a secure version of <em>www.{ $websiteUrl }</em>. You can visit this page instead of <em>{ $websiteUrl }</em>.
about-httpsonly-suggestion-box-www-button = Go to www.{ $websiteUrl }
