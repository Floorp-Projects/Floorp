#!/usr/bin/env python
#-*- coding: utf-8 -*-
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Contains utilities for communicating with the dashboard."""

__author__ = 'phoglund@webrtc.org (Patrik Höglund)'

import httplib
import shelve
import urlparse
import oauth.oauth as oauth

import constants


class FailedToReadRequiredInputFile(Exception):
  pass


class FailedToReportToDashboard(Exception):
  pass


class DashboardConnection:
  """Helper class for pushing data to the dashboard.

     This class deals with most of details for accessing protected resources
     (i.e. data-writing operations) on the dashboard. Such operations are
     authenticated using OAuth. This class requires a consumer secret and a
     access token.

     The access token and consumer secrets are stored as files on disk in the
     working directory of the scripts. Both files are created by the
     request_oauth_permission script.
  """

  def __init__(self, consumer_key):
    self.consumer_key_ = consumer_key

  def read_required_files(self, consumer_secret_file, access_token_file):
    """Reads required data for making OAuth requests.

       Args:
           consumer_secret_file: A shelve file with an entry consumer_secret
               containing the consumer secret in string form.
           access_token_file: A shelve file with an entry access_token
               containing the access token in string form.
    """
    self.access_token_string_ = self._read_access_token(access_token_file)
    self.consumer_secret_ = self._read_consumer_secret(consumer_secret_file)

  def send_post_request(self, url, parameters):
    """Sends an OAuth request for a protected resource in the dashboard.

       Use this when you want to report new data to the dashboard. You must have
       called the read_required_files method prior to calling this method, since
       that method will read in the consumer secret and access token we need to
       make the OAuth request. These concepts are described in the class
       description.

       The server is expected to respond with HTTP status 200 and a completely
       empty response if the call failed. The server may put diagnostic
       information in the response.

       Args:
           url: An absolute url within the dashboard domain, for example
               http://webrtc-dashboard.appspot.com/add_coverage_data.
           parameters: A dict which maps from POST parameter names to values.

       Raises:
           FailedToReportToDashboard: If the dashboard didn't respond
               with HTTP 200 to our request or if the response is non-empty.
    """
    consumer = oauth.OAuthConsumer(self.consumer_key_, self.consumer_secret_)
    access_token = oauth.OAuthToken.from_string(self.access_token_string_)

    oauth_request = oauth.OAuthRequest.from_consumer_and_token(
                        consumer,
                        token=access_token,
                        http_method='POST',
                        http_url=url,
                        parameters=parameters)

    signature_method_hmac_sha1 = oauth.OAuthSignatureMethod_HMAC_SHA1()
    oauth_request.sign_request(signature_method_hmac_sha1, consumer,
                               access_token)

    connection = httplib.HTTPConnection(constants.DASHBOARD_SERVER)

    headers = {'Content-Type': 'application/x-www-form-urlencoded'}
    connection.request('POST', url, body=oauth_request.to_postdata(),
                       headers=headers)

    response = connection.getresponse()
    connection.close()

    if response.status != 200:
      message = ('Failed to report to %s: got response %d (%s)' %
                 (url, response.status, response.reason))
      raise FailedToReportToDashboard(message)

    # The response content should be empty on success, so check that:
    response_content = response.read()
    if response_content:
      message = ('Dashboard reported the following error: %s.' %
                 response_content)
      raise FailedToReportToDashboard(message)

  def _read_access_token(self, filename):
    return self._read_shelve(filename, 'access_token')

  def _read_consumer_secret(self, filename):
    return self._read_shelve(filename, 'consumer_secret')

  def _read_shelve(self, filename, key):
    input_file = shelve.open(filename)

    if not input_file.has_key(key):
      raise FailedToReadRequiredInputFile('Missing correct %s file in current '
                                          'directory. You may have to run '
                                          'request_oauth_permission.py.' %
                                          filename)

    result = input_file[key]
    input_file.close()

    return result
