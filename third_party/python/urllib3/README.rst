urllib3
=======

urllib3 is a powerful, *sanity-friendly* HTTP client for Python. Much of the
Python ecosystem already uses urllib3 and you should too.
urllib3 brings many critical features that are missing from the Python
standard libraries:

- Thread safety.
- Connection pooling.
- Client-side SSL/TLS verification.
- File uploads with multipart encoding.
- Helpers for retrying requests and dealing with HTTP redirects.
- Support for gzip, deflate, and brotli encoding.
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


Installing
----------

urllib3 can be installed with `pip <https://pip.pypa.io>`_::

    $ pip install urllib3

Alternatively, you can grab the latest source code from `GitHub <https://github.com/urllib3/urllib3>`_::

    $ git clone git://github.com/urllib3/urllib3.git
    $ python setup.py install


Documentation
-------------

urllib3 has usage and reference documentation at `urllib3.readthedocs.io <https://urllib3.readthedocs.io>`_.


Contributing
------------

urllib3 happily accepts contributions. Please see our
`contributing documentation <https://urllib3.readthedocs.io/en/latest/contributing.html>`_
for some tips on getting started.


Security Disclosures
--------------------

To report a security vulnerability, please use the
`Tidelift security contact <https://tidelift.com/security>`_.
Tidelift will coordinate the fix and disclosure with maintainers.

Maintainers
-----------

- `@sethmlarson <https://github.com/sethmlarson>`_ (Seth M. Larson)
- `@pquentin <https://github.com/pquentin>`_ (Quentin Pradet)
- `@theacodes <https://github.com/theacodes>`_ (Thea Flowers)
- `@haikuginger <https://github.com/haikuginger>`_ (Jess Shapiro)
- `@lukasa <https://github.com/lukasa>`_ (Cory Benfield)
- `@sigmavirus24 <https://github.com/sigmavirus24>`_ (Ian Stapleton Cordasco)
- `@shazow <https://github.com/shazow>`_ (Andrey Petrov)

👋


Sponsorship
-----------

.. |tideliftlogo| image:: https://nedbatchelder.com/pix/Tidelift_Logos_RGB_Tidelift_Shorthand_On-White_small.png
   :width: 75
   :alt: Tidelift

.. list-table::
   :widths: 10 100

   * - |tideliftlogo|
     - Professional support for urllib3 is available as part of the `Tidelift
       Subscription`_.  Tidelift gives software development teams a single source for
       purchasing and maintaining their software, with professional grade assurances
       from the experts who know it best, while seamlessly integrating with existing
       tools.

.. _Tidelift Subscription: https://tidelift.com/subscription/pkg/pypi-urllib3?utm_source=pypi-urllib3&utm_medium=referral&utm_campaign=readme

If your company benefits from this library, please consider `sponsoring its
development <https://urllib3.readthedocs.io/en/latest/contributing.html#sponsorship-project-grants>`_.

Sponsors include:

- Abbott (2018-2019), sponsored `@sethmlarson <https://github.com/sethmlarson>`_'s work on urllib3.
- Google Cloud Platform (2018-2019), sponsored `@theacodes <https://github.com/theacodes>`_'s work on urllib3.
- Akamai (2017-2018), sponsored `@haikuginger <https://github.com/haikuginger>`_'s work on urllib3
- Hewlett Packard Enterprise (2016-2017), sponsored `@Lukasa’s <https://github.com/Lukasa>`_ work on urllib3.
