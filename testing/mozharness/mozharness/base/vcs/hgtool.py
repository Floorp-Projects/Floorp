import os
import re
import urlparse
from collections import namedtuple

from mozharness.base.script import ScriptMixin
from mozharness.base.log import LogMixin, OutputParser, WARNING
from mozharness.base.errors import HgErrorList, VCSException
from mozharness.base.transfer import TransferMixin

HgtoolErrorList = [{
    'substr': 'abort: HTTP Error 404: Not Found',
    'level': WARNING,
}] + HgErrorList


class HgtoolParser(OutputParser):
    """
    A class that extends OutputParser such that it can find the "Got revision"
    string from hgtool.py output
    """

    got_revision_exp = re.compile(r'Got revision (\w+)')
    got_revision = None

    def parse_single_line(self, line):
        m = self.got_revision_exp.match(line)
        if m:
            self.got_revision = m.group(1)
        super(HgtoolParser, self).parse_single_line(line)


class HgtoolVCS(ScriptMixin, LogMixin, TransferMixin):
    def __init__(self, log_obj=None, config=None, vcs_config=None,
                 script_obj=None):
        super(HgtoolVCS, self).__init__()

        self.log_obj = log_obj
        self.script_obj = script_obj
        if config:
            self.config = config
        else:
            self.config = {}
        # vcs_config = {
        #  hg_host: hg_host,
        #  repo: repository,
        #  branch: branch,
        #  revision: revision,
        #  ssh_username: ssh_username,
        #  ssh_key: ssh_key,
        #  clone_by_revision: clone_by_revision,
        #  clone_with_purge: clone_with_purge,
        # }
        self.vcs_config = vcs_config
        self.hgtool = self.query_exe('hgtool.py', return_type='list')

    def ensure_repo_and_revision(self):
        """Makes sure that `dest` is has `revision` or `branch` checked out
        from `repo`.

        Do what it takes to make that happen, including possibly clobbering
        dest.
        """
        c = self.vcs_config
        for conf_item in ('dest', 'repo'):
            assert self.vcs_config[conf_item]
        dest = os.path.abspath(c['dest'])
        repo = c['repo']
        revision = c.get('revision')
        branch = c.get('branch')
        share_base = c.get('vcs_share_base', os.environ.get("HG_SHARE_BASE_DIR", None))
        clone_by_rev = c.get('clone_by_revision')
        clone_with_purge = c.get('clone_with_purge')
        env = {'PATH': os.environ.get('PATH')}
        if c.get('env'):
            env.update(c['env'])
        if share_base is not None:
            env['HG_SHARE_BASE_DIR'] = share_base
        if self._is_windows():
            # SYSTEMROOT is needed for 'import random'
            if 'SYSTEMROOT' not in env:
                env['SYSTEMROOT'] = os.environ.get('SYSTEMROOT')
            # HOME is needed for the 'hg help share' check
            if 'HOME' not in env:
                env['HOME'] = os.environ.get('HOME')

        cmd = self.hgtool[:]

        if clone_by_rev:
            cmd.append('--clone-by-revision')
        if branch:
            cmd.extend(['-b', branch])
        if revision:
            cmd.extend(['-r', revision])

        for base_mirror_url in self.config.get('hgtool_base_mirror_urls', self.config.get('vcs_base_mirror_urls', [])):
            bits = urlparse.urlparse(repo)
            mirror_url = urlparse.urljoin(base_mirror_url, bits.path)
            cmd.extend(['--mirror', mirror_url])

        for base_bundle_url in self.config.get('hgtool_base_bundle_urls', self.config.get('vcs_base_bundle_urls', [])):
            bundle_url = "%s/%s.hg" % (base_bundle_url, os.path.basename(repo))
            cmd.extend(['--bundle', bundle_url])

        cmd.extend([repo, dest])

        if clone_with_purge:
            cmd.append('--purge')

        parser = HgtoolParser(config=self.config, log_obj=self.log_obj,
                              error_list=HgtoolErrorList)
        retval = self.run_command(cmd, error_list=HgtoolErrorList, env=env, output_parser=parser)

        if retval != 0:
            raise VCSException("Unable to checkout")

        return parser.got_revision

    def query_pushinfo(self, repository, revision):
        """Query the pushdate and pushid of a repository/revision.
        This is intended to be used on hg.mozilla.org/mozilla-central and
        similar. It may or may not work for other hg repositories.
        """
        PushInfo = namedtuple('PushInfo', ['pushid', 'pushdate'])

        try:
            url = '%s/json-pushes?changeset=%s' % (repository, revision)
            self.info('Pushdate URL is: %s' % url)
            contents = self.retry(self.load_json_from_url, args=(url,))

            # The contents should be something like:
            # {
            #   "28537": {
            #    "changesets": [
            #     "1d0a914ae676cc5ed203cdc05c16d8e0c22af7e5",
            #    ],
            #    "date": 1428072488,
            #    "user": "user@mozilla.com"
            #   }
            # }
            #
            # So we grab the first element ("28537" in this case) and then pull
            # out the 'date' field.
            pushid = contents.iterkeys().next()
            self.info('Pushid is: %s' % pushid)
            pushdate = contents[pushid]['date']
            self.info('Pushdate is: %s' % pushdate)
            return PushInfo(pushid, pushdate)

        except Exception:
            self.exception("Failed to get push info from hg.mozilla.org")
            raise
