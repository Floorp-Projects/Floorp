idna-ssl
========

:info: Patch ssl.match_hostname for Unicode(idna) domains support

.. image:: https://travis-ci.com/aio-libs/idna-ssl.svg?branch=master
    :target: https://travis-ci.com/aio-libs/idna-ssl

.. image:: https://img.shields.io/pypi/v/idna_ssl.svg
    :target: https://pypi.python.org/pypi/idna_ssl

.. image:: https://codecov.io/gh/aio-libs/idna-ssl/branch/master/graph/badge.svg
    :target: https://codecov.io/gh/aio-libs/idna-ssl

Installation
------------

.. code-block:: shell

    pip install idna-ssl

Usage
-----

.. code-block:: python

    from idna_ssl import patch_match_hostname  # noqa isort:skip
    patch_match_hostname()  # noqa isort:skip

    import asyncio

    import aiohttp

    URL = 'https://цфоут.мвд.рф/news/item/8065038/'


    async def main():
        async with aiohttp.ClientSession() as session:
            async with session.get(URL) as response:
                print(response)


    loop = asyncio.get_event_loop()
    loop.run_until_complete(main())

Motivation
----------

* Here is 100% backward capability
* Related aiohttp `issue <https://github.com/aio-libs/aiohttp/issues/949>`_
* Related Python `bug <https://bugs.python.org/issue31872>`_
* Related Python `pull request <https://github.com/python/cpython/pull/3462>`_
* It is fixed (by January 27 2018) in upcoming Python 3.7, but `IDNA2008 <https://tools.ietf.org/html/rfc5895>`_ is still broken

Thanks
------

The library was donated by `Ocean S.A. <https://ocean.io/>`_

Thanks to the company for contribution.
