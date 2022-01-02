
"xfocsp-error-report" ping
==========================

This opt-in ping is sent when an X-Frame-Options error or a CSP: frame-ancestors
happens to report the error. Users can opt-in this by checking the reporting
checkbox. After users opt-in, this ping will be sent every time the error
happens. Users can opt-out this by un-checking the reporting checkbox on the
error page. The client_id and environment are not sent with this ping.

Structure:

.. code-block:: js

    {
      "type": "xfocsp-error-report",
      ... common ping data
      "payload": {
        "error_type": <string>,
        "xfo_header": <string>,
        "csp_header": <string>,
        "frame_hostname": <string>,
        "top_hostname": <string>,
        "frame_uri": <string>,
        "top_uri": <string>,
      }
    }

info
----

error_type
~~~~~~~~~~

The type of what error triggers this ping. This could be either "xfo" or "csp".

xfo_header
~~~~~~~~~~

The X-Frame-Options value in the response HTTP header.

csp_header
~~~~~~~~~~

The CSP: frame-ancestors value in the response HTTP header.

frame_hostname
~~~~~~~~~~~~~~

The hostname of the frame which triggers the error.

top_hostname
~~~~~~~~~~~~

The hostname of the top-level page which loads the frame.

frame_uri
~~~~~~~~~

The uri of the frame which triggers the error. This excludes the query strings.

top_uri
~~~~~~~

The uri of the top-level page which loads the frame. This excludes the query
strings.


See also: :doc:`common ping fields <common-ping>`
