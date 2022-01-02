
"first-shutdown" ping
=====================

This ping is a duplicate of the main-ping sent on first shutdown. Enabling pingsender
for first sessions will impact existing engagement metrics. The ``first-shutdown`` ping enables a
more accurate view of users that churn after the first session. This ping exists as a
stopgap until existing metrics are re-evaluated, allowing us to use the first session
``main-pings`` directly.

See :doc:`main-ping` for details about this payload.
