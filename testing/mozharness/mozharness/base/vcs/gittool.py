import os
import re
import urlparse

from mozharness.base.script import ScriptMixin
from mozharness.base.log import LogMixin, OutputParser
from mozharness.base.errors import GitErrorList, VCSException


class GittoolParser(OutputParser):
    """
    A class that extends OutputParser such that it can find the "Got revision"
    string from gittool.py output
    """

    got_revision_exp = re.compile(r'Got revision (\w+)')
    got_revision = None

    def parse_single_line(self, line):
        m = self.got_revision_exp.match(line)
        if m:
            self.got_revision = m.group(1)
        super(GittoolParser, self).parse_single_line(line)


class GittoolVCS(ScriptMixin, LogMixin):
    def __init__(self, log_obj=None, config=None, vcs_config=None,
                 script_obj=None):
        super(GittoolVCS, self).__init__()

        self.log_obj = log_obj
        self.script_obj = script_obj
        if config:
            self.config = config
        else:
            self.config = {}
        # vcs_config = {
        #  repo: repository,
        #  branch: branch,
        #  revision: revision,
        #  ssh_username: ssh_username,
        #  ssh_key: ssh_key,
        # }
        self.vcs_config = vcs_config
        self.gittool = self.query_exe('gittool.py', return_type='list')

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
        clean = c.get('clean')
        share_base = c.get('vcs_share_base', os.environ.get("GIT_SHARE_BASE_DIR", None))
        env = {'PATH': os.environ.get('PATH')}
        env.update(c.get('env', {}))
        if self._is_windows():
            # git.exe is not in the PATH by default
            env['PATH'] = '%s;C:/mozilla-build/Git/bin' % env['PATH']
            # SYSTEMROOT is needed for 'import random'
            if 'SYSTEMROOT' not in env:
                env['SYSTEMROOT'] = os.environ.get('SYSTEMROOT')
        if share_base is not None:
            env['GIT_SHARE_BASE_DIR'] = share_base

        cmd = self.gittool[:]
        if branch:
            cmd.extend(['-b', branch])
        if revision:
            cmd.extend(['-r', revision])
        if clean:
            cmd.append('--clean')

        for base_mirror_url in self.config.get('gittool_base_mirror_urls', self.config.get('vcs_base_mirror_urls', [])):
            bits = urlparse.urlparse(repo)
            mirror_url = urlparse.urljoin(base_mirror_url, bits.path)
            cmd.extend(['--mirror', mirror_url])

        cmd.extend([repo, dest])
        parser = GittoolParser(config=self.config, log_obj=self.log_obj,
                               error_list=GitErrorList)
        retval = self.run_command(cmd, error_list=GitErrorList, env=env, output_parser=parser)

        if retval != 0:
            raise VCSException("Unable to checkout")

        return parser.got_revision
