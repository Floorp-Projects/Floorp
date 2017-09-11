
"first-shutdown" ping
==================

This ping is a duplicate of the main-ping sent on first shutdown. Enabling pingsender
for first sessions will impact existing engagment metrics. The ``first-shutdown`` ping enables a
more accurate view of users that churn after the first session. This ping exists as a
stopgap until existing metrics are re-evaluated for use first session ``main-pings``.

See :doc:`main-ping` for details about this payload.
