#!/usr/bin/env python
#-*- coding: utf-8 -*-
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Implements a handler for adding build status data."""

__author__ = 'phoglund@webrtc.org (Patrik Höglund)'

import datetime
import logging

from google.appengine.ext import db

import oauth_post_request_handler

VALID_STATUSES = ['OK', 'failed', 'building', 'warnings']


class OrphanedBuildStatusesExistException(Exception):
  pass


class BuildStatusRoot(db.Model):
  """Exists solely to be the root parent for all build status data and to keep
     track of when the last update was made.

     Since all build status data will refer to this as their parent,
     we can run transactions on the build status data as a whole.
  """
  last_updated_at = db.DateTimeProperty()


class BuildStatusData(db.Model):
  """This represents one build status report from the build bot."""
  bot_name = db.StringProperty(required=True)
  revision = db.IntegerProperty(required=True)
  build_number = db.IntegerProperty(required=True)
  status = db.StringProperty(required=True)


def _ensure_build_status_root_exists():
  root = db.GqlQuery('SELECT * FROM BuildStatusRoot').get()
  if not root:
    # Create a new root, but ensure we don't have any orphaned build statuses
    # (in that case, we would not have a single entity group as we desire).
    orphans = db.GqlQuery('SELECT * FROM BuildStatusData').get()
    if orphans:
      raise OrphanedBuildStatusesExistException('Parent is gone and there are '
                                                'orphaned build statuses in '
                                                'the database!')
    root = BuildStatusRoot()
    root.put()

  return root


def _filter_oauth_parameters(post_keys):
  return filter(lambda post_key: not post_key.startswith('oauth_'),
                post_keys)


def _parse_status(build_number_and_status):
  parsed_status = build_number_and_status.split('--')
  if len(parsed_status) != 2:
    raise ValueError('Malformed status string %s.' % build_number_and_status)

  parsed_build_number = int(parsed_status[0])
  status = parsed_status[1]

  if status not in VALID_STATUSES:
    raise ValueError('Invalid status in %s.' % build_number_and_status)

  return (parsed_build_number, status)


def _parse_name(revision_and_bot_name):
  parsed_name = revision_and_bot_name.split('--')
  if len(parsed_name) != 2:
    raise ValueError('Malformed name string %s.' % revision_and_bot_name)

  revision = parsed_name[0]
  bot_name = parsed_name[1]
  if '\n' in bot_name:
    raise ValueError('Bot name %s can not contain newlines.' % bot_name)

  return (int(revision), bot_name)


def _delete_all_with_revision(revision, build_status_root):
  query_result = db.GqlQuery('SELECT * FROM BuildStatusData '
                             'WHERE revision = :1 AND ANCESTOR IS :2',
                             revision, build_status_root)
  for entry in query_result:
    entry.delete()


class AddBuildStatusData(oauth_post_request_handler.OAuthPostRequestHandler):
  """Used to report build status data.

     Build status data is reported as a POST request. The POST request, aside
     from the required oauth_* parameters should contain name-value entries that
     abide by the following rules:

     1) The name should be on the form <revision>--<bot name>, for instance
        1568--Win32Release.
     2) The value should be on the form <build number>--<status>, for instance
        553--OK, 554--building. The status is permitted to be failed, OK or
        building.

    Data is keyed by revision. This handler will delete all data from a revision
    if data with that revision is present in the current update, since we
    assume that more recent data is always better data. We also assume that
    an update always has complete information on a revision (e.g. the status
    for all the bots are reported in each update).

    In particular the revision arrangement solves the problem when the latest
    revision reports 'building' for a bot. Had we not deleted the old revision
    we would first store a 'building' status for that bot and revision, and
    later store a 'OK' or 'failed' status for that bot and revision. This is
    undesirable since we don't want multiple statuses for one bot-revision
    combination. Now we will effectively update the bot's status instead.
  """

  def _parse_and_store_data(self):
    build_status_root = _ensure_build_status_root_exists()
    build_status_data = _filter_oauth_parameters(self.request.arguments())

    db.run_in_transaction(self._parse_and_store_data_in_transaction,
                          build_status_root, build_status_data)

  def _parse_and_store_data_in_transaction(self, build_status_root,
                                           build_status_data):

    encountered_revisions = set()
    for revision_and_bot_name in build_status_data:
      build_number_and_status = self.request.get(revision_and_bot_name)

      try:
        (build_number, status) = _parse_status(build_number_and_status)
        (revision, bot_name) = _parse_name(revision_and_bot_name)
      except ValueError as error:
        logging.warn('Invalid parameter in request: %s.' % error)
        self.response.set_status(400)
        return

      if revision not in encountered_revisions:
        # There's new data on this revision in this update, so clear all status
        # entries with that revision. Only do this once when we first encounter
        # the revision.
        _delete_all_with_revision(revision, build_status_root)
        encountered_revisions.add(revision)

      # Finally, write the item.
      item = BuildStatusData(parent=build_status_root,
                             bot_name=bot_name,
                             revision=revision,
                             build_number=build_number,
                             status=status)
      item.put()

    request_posix_timestamp = float(self.request.get('oauth_timestamp'))
    request_datetime = datetime.datetime.fromtimestamp(request_posix_timestamp)
    build_status_root.last_updated_at = request_datetime
    build_status_root.put()

