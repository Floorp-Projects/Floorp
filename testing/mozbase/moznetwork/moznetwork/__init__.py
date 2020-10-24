# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""
moznetwork is a very simple module designed for one task: getting the
network address of the current machine.

Example usage:

::

  import moznetwork

  try:
      ip = moznetwork.get_ip()
      print "The external IP of your machine is '%s'" % ip
  except moznetwork.NetworkError:
      print "Unable to determine IP address of machine"
      raise

"""

from __future__ import absolute_import

from .moznetwork import get_ip

__all__ = ['get_ip']
