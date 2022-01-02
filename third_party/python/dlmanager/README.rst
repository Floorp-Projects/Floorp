.. image:: https://badge.fury.io/py/dlmanager.svg
    :target: https://pypi.python.org/pypi/dlmanager

.. image:: https://readthedocs.org/projects/dlmanager/badge/?version=latest
    :target: http://dlmanager.readthedocs.org/en/latest/?badge=latest
    :alt: Documentation Status

.. image:: https://travis-ci.org/parkouss/dlmanager.svg?branch=master
    :target: https://travis-ci.org/parkouss/dlmanager

.. image:: https://codecov.io/github/parkouss/dlmanager/coverage.svg?branch=master
    :target: https://codecov.io/github/parkouss/dlmanager?branch=master

dlmanager
=========

**dlmanager** is Python 2 and 3 download manager library, with the following
features:

- Download files in background and in parallel
- Cancel downloads
- store downloads in a given directory, avoiding re-downloading files
- Limit the size of this directory, removing oldest files


Example
-------

.. code-block:: python

  from dlmanager import DownloadManager, PersistLimit

  manager = DownloadManager(
      "dlmanager-destir",
      persist_limit=PersistLimit(
          size_limit=1073741824,  #  1 GB max
          file_limit=10,  # force to keep 10 files even if size_limit is reached
      )
  )

  # Start downloads in background
  # Note that if files are already present, this is a no-op.
  manager.download(url1)
  manager.download(url2)

  # Wait for completion
  try:
      manager.wait()
  except:
      manager.cancel()
      raise


Installation
------------

Use pip: ::

  pip install -U dlmanager
