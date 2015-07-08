#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
'''Interact with mozpool/lifeguard/bmm.
'''

import os
import socket
import sys

from time import sleep
from mozharness.mozilla.buildbot import TBPL_RETRY, TBPL_EXCEPTION

#TODO - adjust these values
MAX_RETRIES = 20
RETRY_INTERVAL = 60

# MozpoolMixin {{{1
class MozpoolMixin(object):
    mozpool_handler = None
    mobile_imaging_format= "http://mobile-imaging"

    def determine_mozpool_host(self, device):
        if "mobile_imaging_format" in self.config:
            self.mobile_imaging_format = self.config["mobile_imaging_format"]
        hostname = str(self.mobile_imaging_format)[7:]
        fqdn = socket.getfqdn(hostname)
        imaging_server_fqdn = (str(self.mobile_imaging_format)).replace(hostname, fqdn)
        return imaging_server_fqdn

    def query_mozpool_handler(self, device=None, mozpool_api_url=None):
        if self.mozpool_handler != None:
            return self.mozpool_handler
        else:
            self.mozpool_api_url = self.determine_mozpool_host(device) if device else mozpool_api_url
            assert self.mozpool_api_url != None, \
                "query_mozpool_handler() requires either a device or mozpool_api_url!"

            site_packages_path = self.query_python_site_packages_path()
            mph_path = os.path.join(site_packages_path, 'mozpoolclient')
            sys.path.append(mph_path)
            sys.path.append(site_packages_path)
            try:
                from mozpoolclient import MozpoolHandler, MozpoolException, MozpoolConflictException
                self.MozpoolException = MozpoolException
                self.MozpoolConflictException = MozpoolConflictException
                self.mozpool_handler = MozpoolHandler(self.mozpool_api_url, log_obj=self)
            except ImportError, e:
                self.fatal("Can't instantiate MozpoolHandler until mozpoolclient python "
                           "package is installed! (VirtualenvMixin?): \n%s" % str(e))
            return self.mozpool_handler

    def retrieve_b2g_device(self, b2gbase):
        mph = self.query_mozpool_handler(self.mozpool_device)
        for retry in self._retry_sleep(
                error_message="INFRA-ERROR: Could not request device '%s'" % self.mozpool_device,
                tbpl_status=TBPL_EXCEPTION):
            try:
                image = 'b2g'
                duration = 4 * 60 * 60 # request valid for 14400 seconds == 4 hours
                response = mph.request_device(self.mozpool_device, image, assignee=self.mozpool_assignee, \
                               b2gbase=b2gbase, pxe_config=None, duration=duration)
                break
            except self.MozpoolConflictException:
                self.warning("Device unavailable. Retry#%i.." % retry)
            except self.MozpoolException, e:
                self.buildbot_status(TBPL_RETRY)
                self.fatal("We could not request the device: %s" % str(e))

        self.request_url = response['request']['url']
        self.info("Got request, url=%s" % self.request_url)
        self._wait_for_request_ready()

    def retrieve_android_device(self, b2gbase):
        mph = self.query_mozpool_handler(self.mozpool_device)
        for retry in self._retry_sleep(
                error_message="INFRA-ERROR: Could not request device '%s'" % self.mozpool_device,
                tbpl_status=TBPL_RETRY):
            try:
                image = 'panda-android-4.0.4_v3.3'
                duration = 4 * 60 * 60 # request valid for 14400 seconds == 4 hours
                response = mph.request_device(self.mozpool_device, image, assignee=self.mozpool_assignee, \
                               b2gbase=b2gbase, pxe_config=None, duration=duration)
                break
            except self.MozpoolConflictException:
                self.warning("Device unavailable. Retry#%i.." % retry)
            except self.MozpoolException, e:
                self.buildbot_status(TBPL_RETRY)
                self.fatal("We could not request the device: %s" % str(e))

        self.request_url = response['request']['url']
        self.info("Got request, url=%s" % self.request_url)
        self._wait_for_request_ready()

    def _retry_job_and_close_request(self, message, exception=None):
        mph = self.query_mozpool_handler(self.mozpool_device)
        exception_message = str(exception) if exception!=None and str(exception) != None else ""
        self.error("%s -> %s" % (message, exception_message))
        if self.request_url:
            mph.close_request(self.request_url)
        self.buildbot_status(TBPL_RETRY)
        self.fatal(message)

    def _retry_sleep(self, sleep_time=RETRY_INTERVAL, max_retries=MAX_RETRIES,
                     error_message=None, tbpl_status=None, fail_cb=None):
        for x in range(1, max_retries + 1):
            yield x
            sleep(sleep_time)
        if error_message:
            self.error(error_message)
        if tbpl_status:
            self.buildbot_status(tbpl_status)
        if fail_cb:
            assert callable(fail_cb)
            fail_cb()
        self.fatal('Retries limit exceeded')

    def _wait_for_request_ready(self):
        mph = self.query_mozpool_handler(self.mozpool_device)
        def on_fail():
            # Device is not ready after retries...
            self.info("Aborting mozpool request.")
            self.close_request()
        for retry in self._retry_sleep(sleep_time=RETRY_INTERVAL, max_retries=MAX_RETRIES,
                error_message="INFRA-ERROR: Request did not become ready in time",
                tbpl_status=TBPL_EXCEPTION, fail_cb=on_fail):
            response = mph.query_request_status(self.request_url)
            state = response['state']
            if state == 'ready':
                return
            self.info("Waiting for request 'ready' stage.  Current state: '%s'" % state)
