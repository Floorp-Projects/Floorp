#!/usr/bin/env python
# Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import datetime
import httplib2
import json
import subprocess
import time
import zlib

from tracing.value import histogram
from tracing.value import histogram_set
from tracing.value.diagnostics import generic_set
from tracing.value.diagnostics import reserved_infos


def _GenerateOauthToken():
    args = ['luci-auth', 'token']
    p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if p.wait() == 0:
        output = p.stdout.read()
        return output.strip()
    else:
        raise RuntimeError(
            'Error generating authentication token.\nStdout: %s\nStderr:%s' %
            (p.stdout.read(), p.stderr.read()))


def _SendHistogramSet(url, histograms, oauth_token):
    """Make a HTTP POST with the given JSON to the Performance Dashboard.

    Args:
      url: URL of Performance Dashboard instance, e.g.
          "https://chromeperf.appspot.com".
      histograms: a histogram set object that contains the data to be sent.
      oauth_token: An oauth token to use for authorization.
    """
    headers = {'Authorization': 'Bearer %s' % oauth_token}

    serialized = json.dumps(_ApplyHacks(histograms.AsDicts()), indent=4)

    if url.startswith('http://localhost'):
        # The catapult server turns off compression in developer mode.
        data = serialized
    else:
        data = zlib.compress(serialized)

    print 'Sending %d bytes to %s.' % (len(data), url + '/add_histograms')

    http = httplib2.Http()
    response, content = http.request(url + '/add_histograms',
                                     method='POST',
                                     body=data,
                                     headers=headers)
    return response, content


def _WaitForUploadConfirmation(url, oauth_token, upload_token, wait_timeout,
                               wait_polling_period):
    """Make a HTTP GET requests to the Performance Dashboard untill upload
    status is known or the time is out.

    Args:
      url: URL of Performance Dashboard instance, e.g.
          "https://chromeperf.appspot.com".
      oauth_token: An oauth token to use for authorization.
      upload_token: String that identifies Performance Dashboard and can be used
        for the status check.
      wait_timeout: (datetime.timedelta) Maximum time to wait for the
        confirmation.
      wait_polling_period: (datetime.timedelta) Performance Dashboard will be
        polled every wait_polling_period amount of time.
    """
    assert wait_polling_period <= wait_timeout

    headers = {'Authorization': 'Bearer %s' % oauth_token}
    http = httplib2.Http()

    response = None
    resp_json = None
    current_time = datetime.datetime.now()
    end_time = current_time + wait_timeout
    next_poll_time = current_time + wait_polling_period
    while datetime.datetime.now() < end_time:
        current_time = datetime.datetime.now()
        if next_poll_time > current_time:
            time.sleep((next_poll_time - current_time).total_seconds())
        next_poll_time = datetime.datetime.now() + wait_polling_period

        response, content = http.request(url + '/uploads' + upload_token,
                                         method='GET', headers=headers)
        resp_json = json.loads(content)

        print 'Upload state polled. Response: %s.' % content

        if (response.status != 200 or
            resp_json['state'] == 'COMPLETED' or
            resp_json['state'] == 'FAILED'):
            break

    return response, resp_json


# TODO(https://crbug.com/1029452): HACKHACK
# Remove once we have doubles in the proto and handle -infinity correctly.
def _ApplyHacks(dicts):
    for d in dicts:
        if 'running' in d:

            def _NoInf(value):
                if value == float('inf'):
                    return histogram.JS_MAX_VALUE
                if value == float('-inf'):
                    return -histogram.JS_MAX_VALUE
                return value

            d['running'] = [_NoInf(value) for value in d['running']]

    return dicts


def _LoadHistogramSetFromProto(options):
    hs = histogram_set.HistogramSet()
    with options.input_results_file as f:
        hs.ImportProto(f.read())

    return hs


def _AddBuildInfo(histograms, options):
    common_diagnostics = {
        reserved_infos.MASTERS: options.perf_dashboard_machine_group,
        reserved_infos.BOTS: options.bot,
        reserved_infos.POINT_ID: options.commit_position,
        reserved_infos.BENCHMARKS: options.test_suite,
        reserved_infos.WEBRTC_REVISIONS: str(options.webrtc_git_hash),
        reserved_infos.BUILD_URLS: options.build_page_url,
    }

    for k, v in common_diagnostics.items():
        histograms.AddSharedDiagnosticToAllHistograms(
            k.name, generic_set.GenericSet([v]))


def _DumpOutput(histograms, output_file):
    with output_file:
        json.dump(_ApplyHacks(histograms.AsDicts()), output_file, indent=4)


def UploadToDashboard(options):
    histograms = _LoadHistogramSetFromProto(options)
    _AddBuildInfo(histograms, options)

    if options.output_json_file:
        _DumpOutput(histograms, options.output_json_file)

    oauth_token = _GenerateOauthToken()
    response, content = _SendHistogramSet(
        options.dashboard_url, histograms, oauth_token)

    upload_token = json.loads(content).get('token')
    if not options.wait_for_upload or not upload_token:
        print 'Not waiting for upload status confirmation.'
        if response.status == 200:
            print 'Received 200 from dashboard.'
            return 0
        else:
            print('Upload failed with %d: %s\n\n%s' % (response.status,
                                                      response.reason, content))
            return 1

    response, resp_json = _WaitForUploadConfirmation(
        options.dashboard_url,
        oauth_token,
        upload_token,
        datetime.timedelta(seconds=options.wait_timeout_sec),
        datetime.timedelta(seconds=options.wait_polling_period_sec))

    if response.status != 200 or resp_json['state'] == 'FAILED':
        print('Upload failed with %d: %s\n\n%s' % (response.status,
                                                  response.reason,
                                                  str(resp_json)))
        return 1

    if resp_json['state'] == 'COMPLETED':
        print 'Upload completed.'
        return 0

    print('Upload wasn\'t completed in a given time: %d.', options.wait_timeout)
    return 1
