#!/usr/bin/env python
#-*- coding: utf-8 -*-
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""This script request an access token from the appengine running the dashboard.

   The script is intended to be run manually whenever we wish to change which
   dashboard administrator we act on behalf of when running the
   track_coverage.py script. For example, this will be useful if the current
   dashboard administrator leaves the project. This script can also be used to
   launch a new dashboard if that is desired.

   This script should be run on the build bot which runs the track_coverage.py
   script. This script will present a link during its execution, which the new
   administrator should follow and then click approve on the web page that
   appears. The new administrator should have admin rights on the coverage
   dashboard, otherwise the track_* scripts will not work.

   If successful, this script will write the access token to a file access.token
   in the current directory, which later can be read by the track_* scripts.
   The token is stored in string form (as reported by the web server) using the
   shelve module. The consumer secret passed in as an argument to this script
   will also similarly be stored in a file consumer.secret. The shelve keys
   will be 'access_token' and 'consumer_secret', respectively.
"""

__author__ = 'phoglund@webrtc.org (Patrik Höglund)'

import shelve
import sys
import urlparse
import oauth2 as oauth

import constants


class FailedToRequestPermissionException(Exception):
  pass


def _ensure_token_response_is_200(response, queried_url, token_type):
  if response.status != 200:
    raise FailedToRequestPermissionException('Failed to request %s from %s: '
                                             'received status %d, reason %s.' %
                                             (token_type,
                                              queried_url,
                                              response.status,
                                              response.reason))


def _request_unauthorized_token(consumer, request_token_url):
  """Requests the initial token from the dashboard service.

     Given that the response from the server is correct, we will return a
     dictionary containing oauth_token and oauth_token_secret mapped to the
     token and secret value, respectively.
  """
  client = oauth.Client(consumer)

  try:
    response, content = client.request(request_token_url, 'POST')
  except AttributeError as error:
    # This catch handler is here since we'll get very confusing messages
    # if the target server is down for some reason.
    raise FailedToRequestPermissionException('Failed to request token: '
                                             'the dashboard is likely down.',
                                             error)

  _ensure_token_response_is_200(response, request_token_url,
                                'unauthorized token')

  return dict(urlparse.parse_qsl(content))


def _ask_user_to_authorize_us(unauthorized_token):
  """This function will block until the user enters y + newline."""
  print 'Go to the following link in your browser:'
  print '%s?oauth_token=%s' % (constants.AUTHORIZE_TOKEN_URL,
                               unauthorized_token['oauth_token'])

  accepted = 'n'
  while accepted.lower() != 'y':
    accepted = raw_input('Have you authorized me yet? (y/n) ')


def _request_access_token(consumer, unauthorized_token):
  token = oauth.Token(unauthorized_token['oauth_token'],
                      unauthorized_token['oauth_token_secret'])
  client = oauth.Client(consumer, token)
  response, content = client.request(constants.ACCESS_TOKEN_URL, 'POST')

  _ensure_token_response_is_200(response, constants.ACCESS_TOKEN_URL,
                                'access token')

  return content


def _write_access_token_to_file(access_token, filename):
  output = shelve.open(filename)
  output['access_token'] = access_token
  output.close()

  print 'Wrote the access token to the file %s.' % filename


def _write_consumer_secret_to_file(consumer_secret, filename):
  output = shelve.open(filename)
  output['consumer_secret'] = consumer_secret
  output.close()

  print 'Wrote the consumer secret to the file %s.' % filename


def _main():
  if len(sys.argv) != 2:
    print ('Usage: %s <consumer secret>.\n\nThe consumer secret is an OAuth '
           'concept and is obtained from the Google Accounts domain dashboard.'
           % sys.argv[0])
    return

  consumer_secret = sys.argv[1]
  consumer = oauth.Consumer(constants.CONSUMER_KEY, consumer_secret)

  unauthorized_token = _request_unauthorized_token(consumer,
                                                   constants.REQUEST_TOKEN_URL)

  _ask_user_to_authorize_us(unauthorized_token)

  access_token_string = _request_access_token(consumer, unauthorized_token)

  _write_access_token_to_file(access_token_string, constants.ACCESS_TOKEN_FILE)
  _write_consumer_secret_to_file(consumer_secret,
                                 constants.CONSUMER_SECRET_FILE)

if __name__ == '__main__':
  _main()
