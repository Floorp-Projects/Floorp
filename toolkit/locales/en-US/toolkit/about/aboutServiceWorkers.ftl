# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### The term "Service Workers" and "Workers" should not be translated

about-service-workers-title = About Service Workers
about-service-workers-main-title = Registered Service Workers
about-service-workers-warning-not-enabled = Service Workers are not enabled.
about-service-workers-warning-no-service-workers = No Service Workers registered.

# The original title of service workers' information
#
# Variables:
#   $originTitle: original title
origin-title = Origin: { $originTitle }

## These strings are for showing the information of workers.
##
## Variables:
##  $name: the name of scope, active cache, waiting cache and the push end point.
##  $url: the url of script specification and current worker.
scope = <strong>Scope:</strong> { $name }
script-spec = <strong>Script Spec:</strong> <a data-l10n-name="link">{ $url }</a>
current-worker-url = <strong>Current Worker URL:</strong> <a data-l10n-name="link">{ $url }</a>
active-cache-name = <strong>Active Cache Name:</strong> { $name }
waiting-cache-name = <strong>Waiting Cache Name:</strong> { $name }
push-end-point-waiting = <strong>Push Endpoint:</strong> { waiting }
push-end-point-result = <strong>Push Endpoint:</strong> { $name }

# This term is used as a button label (verb, not noun).
update-button = Update

unregister-button = Unregister

unregister-error = Failed to unregister this Service Worker.

waiting = Waiting…
