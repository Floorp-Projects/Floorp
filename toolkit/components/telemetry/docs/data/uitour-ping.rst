
"uitour-tag" ping
=================

This ping is submitted via the UITour setTreatmentTag API. It may be used by
the tour to record what settings were made by a user or to track the result of
A/B experiments.

The client ID is submitted with this ping.

Structure:

.. code-block:: js

    {
      version: 1,
      type: "uitour-tag",
      clientId: <string>,
      payload: {
        tagName: <string>,
        tagValue: <string>
      }
    }

See also: :doc:`common ping fields <common-ping>`

