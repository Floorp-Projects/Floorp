urllib3
=======

.. toctree::
   :hidden:
   :maxdepth: 2

   For Enterprise <https://tidelift.com/subscription/pkg/pypi-urllib3?utm_source=pypi-urllib3&utm_medium=referral&utm_campaign=docs>
   user-guide
   advanced-usage
   reference/index
   contributing

urllib3 is a powerful, *sanity-friendly* HTTP client for Python. Much of the
Python ecosystem :ref:`already uses <who-uses>` urllib3 and you should too.
urllib3 brings many critical features that are missing from the Python
standard libraries:

- Thread safety.
- Connection pooling.
- Client-side SSL/TLS verification.
- File uploads with multipart encoding.
- Helpers for retrying requests and dealing with HTTP redirects.
- Support for gzip and deflate encoding.
- Proxy support for HTTP and SOCKS.
- 100% test coverage.

urllib3 is powerful and easy to use::

    >>> import urllib3
    >>> http = urllib3.PoolManager()
    >>> r = http.request('GET', 'http://httpbin.org/robots.txt')
    >>> r.status
    200
    >>> r.data
    'User-agent: *\nDisallow: /deny\n'

For Enterprise
--------------

`urllib3 is available as part of the Tidelift Subscription <https://tidelift.com/subscription/pkg/pypi-urllib3?utm_source=pypi-urllib3&utm_medium=referral&utm_campaign=docs>`_

urllib3 and the maintainers of thousands of other packages are working with Tidelift to deliver one enterprise subscription that covers all of the open source you use.
If you want the flexibility of open source and the confidence of commercial-grade software, this is for you.

|learn-more|_ |request-a-demo|_

.. |learn-more| image:: https://raw.githubusercontent.com/urllib3/urllib3/master/docs/images/learn-more-button.png
.. _learn-more: https://tidelift.com/subscription/pkg/pypi-urllib3?utm_source=pypi-urllib3&utm_medium=referral&utm_campaign=docs
.. |request-a-demo| image:: https://raw.githubusercontent.com/urllib3/urllib3/master/docs/images/demo-button.png
.. _request-a-demo: https://tidelift.com/subscription/request-a-demo?utm_source=pypi-urllib3&utm_medium=referral&utm_campaign=docs

Installing
----------

urllib3 can be installed with `pip <https://pip.pypa.io>`_::

    $ pip install urllib3

Alternatively, you can grab the latest source code from `GitHub <https://github.com/urllib3/urllib3>`_::

    $ git clone git://github.com/urllib3/urllib3.git
    $ python setup.py install

Usage
-----

The :doc:`user-guide` is the place to go to learn how to use the library and
accomplish common tasks. The more in-depth :doc:`advanced-usage` guide is the place to go for lower-level tweaking.

The :doc:`reference/index` documentation provides API-level documentation.

.. _who-uses:

Who uses urllib3?
-----------------

* `Requests <http://python-requests.org/>`_
* `Pip <https://pip.pypa.io>`_
* & more!

License
-------

urllib3 is made available under the MIT License. For more details, see `LICENSE.txt <https://github.com/urllib3/urllib3/blob/master/LICENSE.txt>`_.

Contributing
------------

We happily welcome contributions, please see :doc:`contributing` for details.
