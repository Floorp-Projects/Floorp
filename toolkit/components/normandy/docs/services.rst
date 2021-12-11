Normandy Services
=================
The Normandy Client relies on several external services for correct operation.

Normandy Server
---------------
This is the place where recipes are created, edited and approved. Normandy
Client interacts with it to get an index of services, and to fetch extensions
and their metadata for add-on studies and rollout.

Remote Settings
---------------
This is the primary way that recipes are loaded from the internet by
Normandy. It manages keeping the local list of recipes on the client up to
date and notifying Normandy Client of changes.

Classify Client
---------------
This is a service that helps Normandy with filtering. It determines the
region a user is in, based on IP. It also includes an up-to-date time and
date. This allows Normandy to perform location and time based filtering
without having to rely on the local clock.