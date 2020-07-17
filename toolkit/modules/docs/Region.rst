.. _Region:

======
Region
======

Firefox monitors the users region in order to show relevant local
search engines and content. The region is tracked in 2 properties:

 * ``Region.current`` - The most recent location we detected for the user.
 * ``Region.home`` - Where we consider the users home location.

These are tracked separately as to avoid updating the users
experience repeatedly as they travel for example. In general
callers should use ``Region.home``.

If the user is detected in a current region that is not there `home` region
for a continuous period (current 2 weeks) then their `home` region
will be updated.

Testing
=======

To set the users region for testing you can use ``Region._setHomeRegion("US", false)``, the second parameter ``notify``
will send a notification that the region has changed and trigger a
reload of search engines and other content.
