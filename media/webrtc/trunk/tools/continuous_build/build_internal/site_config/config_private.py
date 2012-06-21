#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

# This file is based on the Chromium config.py located at
# /trunk/tools/build/site_config/config_default.py of the Chromium tools.

import socket

class Master(object):
  # Repository URLs used by the SVNPoller and 'gclient config'.
  server_url = 'http://webrtc.googlecode.com'
  git_server_url =  'http://webrtc.googlecode.com/git'
  repo_root = '/svn'

  # External repos.
  googlecode_url = 'http://%s.googlecode.com/svn'
  sourceforge_url = 'https://%(repo)s.svn.sourceforge.net/svnroot/%(repo)s'

  # Directly fetches from anonymous webkit svn server.
  webkit_root_url = 'http://svn.webkit.org/repository/webkit'
  nacl_trunk_url = 'http://src.chromium.org/native_client/trunk'

  llvm_url = 'http://llvm.org/svn/llvm-project'

  # Other non-redistributable repositories.
  repo_root_internal = None
  trunk_internal_url = None
  trunk_internal_url_src = None
  gears_url_internal = None
  o3d_url_internal = None
  nacl_trunk_url_internal = None
  nacl_url_internal = None

  syzygy_internal_url = None

  # Other non-redistributable repositories.
  repo_root_internal = None
  trunk_internal_url = None
  trunk_internal_url_src = None

  # Please change this accordingly.
  master_domain = 'webrtc.org'
  permitted_domains = ('webrtc.org',)

  # Your smtp server to enable mail notifications.
  smtp = 'smtp'

  # By default, bot_password will be filled in by config.GetBotPassword();
  # if the private config wants to override this, it can do so.
  bot_password = None

  class _Base(object):
    # Master address. You should probably copy this file in another svn repo
    # so you can override this value on both the slaves and the master.
    master_host = 'webrtc-cb-linux-master.cbf.corp.google.com'
    # If set to True, the master will do nasty stuff like closing the tree,
    # sending emails or other similar behaviors. Don't change this value unless
    # you modified the other settings extensively.
    is_production_host = socket.getfqdn() == master_host
    # Additional email addresses to send gatekeeper (automatic tree closage)
    # notifications. Unnecessary for experimental masters and try servers.
    tree_closing_notification_recipients = []
    # 'from:' field for emails sent from the server.
    from_address = 'webrtc-cb-watchlist@google.com'
    # Code review site to upload results. You should setup your own Rietveld
    # instance with the code at
    # http://code.google.com/p/rietveld/source/browse/#svn/branches/chromium
    # You can host your own private rietveld instance on Django, see
    # http://code.google.com/p/google-app-engine-django and
    # http://code.google.com/appengine/articles/pure_django.html
    code_review_site = 'https://webrtc-codereview.appspot.com/status_listener'

    # For the following values, they are used only if non-0. Do not set them
    # here, set them in the actual master configuration class.

    # Used for the waterfall URL and the waterfall's WebStatus object.
    master_port = 0
    # Which port slaves use to connect to the master.
    slave_port = 0
    # The alternate read-only page. Optional.
    master_port_alt = 0
    # HTTP port for try jobs.
    try_job_port = 0

  ## Chrome related

  class _ChromiumBase(_Base):
    # Tree status urls. You should fork the code from tools/chromium-status/ and
    # setup your own AppEngine instance (or use directly Django to create a
    # local instance).
    # Defaulting urls that are used to POST data to 'localhost' so a local dev
    # server can be used for testing and to make sure nobody updates the tree
    # status by error!
    #
    # This url is used for HttpStatusPush:
    base_app_url = 'http://localhost:8080'
    # HTTP url that should return 0 or 1, depending if the tree is open or
    # closed. It is also used as POST to update the tree status.
    tree_status_url = base_app_url + '/status'
    # Used by LKGR to POST data.
    store_revisions_url = base_app_url + '/revisions'
    # Used by the try server to sync to the last known good revision:
    last_good_url = 'http://webrtc-dashboard.appspot.com/lkgr'

  class WebRTC(_ChromiumBase):
    # Used by the waterfall display.
    project_name = 'WebRTC'
    master_port = 9010
    slave_port = 9112
    master_port_alt = 9014

  class WebRTCMemory(_ChromiumBase):
    project_name = 'WebRTC Memory'
    master_port = 9014
    slave_port = 9119
    master_port_alt = 9047

  class WebRTCPerf(_ChromiumBase):
    project_name = 'WebRTC Perf'
    master_port = 9050
    slave_port = 9151
    master_port_alt = 9052

  class TryServer(_ChromiumBase):
    project_name = 'WebRTC Try Server'
    master_port = 9011
    slave_port = 9113
    master_port_alt = 9015
    try_job_port = 9018
    # The svn repository to poll to grab try patches. For chrome, we use a
    # separate repo to put all the diff files to be tried.
    svn_url = None

class Archive(object):
  archive_host = 'localhost'
  # Skip any filenames (exes, symbols, etc.) starting with these strings
  # entirely, typically because they're not built for this distribution.
  exes_to_skip_entirely = []
  # Web server base path.
  www_dir_base = "\\\\" + archive_host + "\\www\\"

  @staticmethod
  def Internal():
    pass


class Distributed(object):
  """Not much to describe."""
