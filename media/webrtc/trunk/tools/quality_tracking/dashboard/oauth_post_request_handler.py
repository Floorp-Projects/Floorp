#!/usr/bin/env python
#-*- coding: utf-8 -*-
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Provides a OAuth request handler base class."""

__author__ = 'phoglund@webrtc.org (Patrik Höglund)'

from google.appengine.api import oauth
import logging
import webapp2


class UserNotAuthenticatedException(Exception):
  """Gets thrown if a user is not permitted to store data."""
  pass


class OAuthPostRequestHandler(webapp2.RequestHandler):
  """Works like a normal request handler but adds OAuth authentication.

     This handler will expect a proper OAuth request over POST. This abstract
     class deals with the authentication but leaves user-defined data handling
     to its subclasses. Subclasses should not implement the post() method but
     the _parse_and_store_data() method. Otherwise they may act like regular
     request handlers. Subclasses should NOT override the get() method.

     The handler will accept an OAuth request if it is correctly formed and
     the consumer is acting on behalf of an administrator for the dashboard.
  """

  def post(self):
    try:
      self._authenticate_user()
    except UserNotAuthenticatedException as exception:
      logging.warn('Failed to authenticate: %s.' % exception)
      self.response.set_status(403)
      return

    # Do the actual work.
    self._parse_and_store_data()

  def _parse_and_store_data(self):
    """Reads data from POST request and responds accordingly."""
    raise NotImplementedError('You must override this method!')

  def _authenticate_user(self):
    try:
      if oauth.is_current_user_admin():
        # The user on whose behalf we are acting is indeed an administrator
        # of this application, so we're good to go.
        logging.info('Authenticated on behalf of user %s.' %
                     oauth.get_current_user())
        return
      else:
        raise UserNotAuthenticatedException('We are acting on behalf of '
                                            'user %s, but that user is not '
                                            'an administrator.' %
                                            oauth.get_current_user())
    except oauth.OAuthRequestError as exception:
      raise UserNotAuthenticatedException('Invalid OAuth request: %s' %
                                          exception.__class__.__name__)
