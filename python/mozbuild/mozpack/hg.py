# Copyright (C) 2015 Mozilla Contributors
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
# As a special exception, the copyright holders of this code give you
# permission to combine this code with the software known as 'mozbuild',
# and to distribute those combinations without any restriction
# coming from the use of this file. (The General Public License
# restrictions do apply in other respects; for example, they cover
# modification of the file, and distribution when not combined with
# mozbuild.)
#
# If you modify this code, you may extend this exception to your
# version of the code, but you are not obliged to do so. If you
# do not wish to do so, delete this exception statement from your
# version.

from __future__ import absolute_import

import mercurial.error as error
import mercurial.hg as hg
import mercurial.ui as hgui

from .files import (
    BaseFinder,
    MercurialFile,
)
import mozpack.path as mozpath


# This isn't a complete implementation of BaseFile. But it is complete
# enough for moz.build reading.
class MercurialNativeFile(MercurialFile):
    def __init__(self, data):
        self.data = data

    def read(self):
        return self.data


class MercurialNativeRevisionFinder(BaseFinder):
    def __init__(self, repo, rev='.', recognize_repo_paths=False):
        """Create a finder attached to a specific changeset.

        Accepts a Mercurial localrepo and changectx instance.
        """
        if isinstance(repo, (str, unicode)):
            path = repo
            repo = hg.repository(hgui.ui(), repo)
        else:
            path = repo.root

        super(MercurialNativeRevisionFinder, self).__init__(base=repo.root)

        self._repo = repo
        self._rev = rev
        self._root = mozpath.normpath(path)
        self._recognize_repo_paths = recognize_repo_paths

    def _find(self, pattern):
        if self._recognize_repo_paths:
            raise NotImplementedError('cannot use find with recognize_repo_path')

        return self._find_helper(pattern, self._repo[self._rev], self._get)

    def get(self, path):
        if self._recognize_repo_paths:
            if not path.startswith(self._root):
                raise ValueError('lookups in recognize_repo_paths mode must be '
                                 'prefixed with repo path: %s' % path)
            path = path[len(self._root) + 1:]

        return self._get(path)

    def _get(self, path):
        if isinstance(path, unicode):
            path = path.encode('utf-8', 'replace')

        try:
            fctx = self._repo.filectx(path, self._rev)
            return MercurialNativeFile(fctx.data())
        except error.LookupError:
            return None
