#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""Support for hg/git mapper
"""
import urllib2
import time
try:
    import simplejson as json
except ImportError:
    import json


class MapperMixin:
    def query_mapper(self, mapper_url, project, vcs, rev,
                     require_answer=True, attempts=30, sleeptime=30,
                     project_name=None):
        """
        Returns the mapped revision for the target vcs via a mapper service

        Args:
            mapper_url (str): base url to use for the mapper service
            project (str): The name of the mapper project to use for lookups
            vcs (str): Which vcs you want the revision for. e.g. "git" to get
                the git revision given an hg revision
            rev (str): The original revision you want the mapping for.
            require_answer (bool): Whether you require a valid answer or not.
                If None is acceptable (meaning mapper doesn't know about the
                revision you're asking about), then set this to False. If True,
                then will return the revision, or cause a fatal error.
            attempts (int): How many times to try to do the lookup
            sleeptime (int): How long to sleep between attempts
            project_name (str): Used for logging only to give a more
                descriptive name to the project, otherwise just uses the
                project parameter

        Returns:
            A revision string, or None
        """
        if project_name is None:
            project_name = project
        url = mapper_url.format(project=project, vcs=vcs, rev=rev)
        self.info('Mapping %s revision to %s using %s' % (project_name, vcs, url))
        n = 1
        while n <= attempts:
            try:
                r = urllib2.urlopen(url, timeout=10)
                j = json.loads(r.readline())
                if j['%s_rev' % vcs] is None:
                    if require_answer:
                        raise Exception("Mapper returned a revision of None; maybe it needs more time.")
                    else:
                        self.warning("Mapper returned a revision of None.  Accepting because require_answer is False.")
                return j['%s_rev' % vcs]
            except Exception, err:
                self.warning('Error: %s' % str(err))
                if n == attempts:
                    self.fatal('Giving up on %s %s revision for %s.' % (project_name, vcs, rev))
                if sleeptime > 0:
                    self.info('Sleeping %i seconds before retrying' % sleeptime)
                    time.sleep(sleeptime)
                continue
            finally:
                n += 1

    def query_mapper_git_revision(self, url, project, rev, **kwargs):
        """
        Returns the git revision for the given hg revision `rev`
        See query_mapper docs for supported parameters and docstrings
        """
        return self.query_mapper(url, project, "git", rev, **kwargs)

    def query_mapper_hg_revision(self, url, project, rev, **kwargs):
        """
        Returns the hg revision for the given git revision `rev`
        See query_mapper docs for supported parameters and docstrings
        """
        return self.query_mapper(url, project, "hg", rev, **kwargs)
