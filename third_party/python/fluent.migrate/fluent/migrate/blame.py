# coding=utf8
from __future__ import unicode_literals
from __future__ import absolute_import

import argparse
import json
import os

from compare_locales.parser import getParser, Junk
from compare_locales.parser.fluent import FluentEntity
from compare_locales import mozpath
import hglib
from hglib.util import b, cmdbuilder


class Blame(object):
    def __init__(self, client):
        self.client = client
        self.users = []
        self.blame = {}

    def attribution(self, file_paths):
        args = cmdbuilder(
            b('annotate'), *[b(p) for p in file_paths], template='json',
            date=True, user=True, cwd=self.client.root())
        blame_json = self.client.rawcommand(args)
        file_blames = json.loads(blame_json)

        for file_blame in file_blames:
            self.handleFile(file_blame)

        return {'authors': self.users,
                'blame': self.blame}

    def handleFile(self, file_blame):
        path = mozpath.normsep(file_blame['path'])

        try:
            parser = getParser(path)
        except UserWarning:
            return

        self.blame[path] = {}

        self.readFile(parser, path)
        entities = parser.parse()
        for e in entities:
            if isinstance(e, Junk):
                continue
            if e.val_span:
                key_vals = [(e.key, e.val_span)]
            else:
                key_vals = []
            if isinstance(e, FluentEntity):
                key_vals += [
                    ('{}.{}'.format(e.key, attr.key), attr.val_span)
                    for attr in e.attributes
                ]
            for key, (val_start, val_end) in key_vals:
                entity_lines = file_blame['lines'][
                    (e.ctx.linecol(val_start)[0] - 1):e.ctx.linecol(val_end)[0]
                ]
                # ignore timezone
                entity_lines.sort(key=lambda blame: -blame['date'][0])
                line_blame = entity_lines[0]
                user = line_blame['user']
                timestamp = line_blame['date'][0]  # ignore timezone
                if user not in self.users:
                    self.users.append(user)
                userid = self.users.index(user)
                self.blame[path][key] = [userid, timestamp]

    def readFile(self, parser, path):
        parser.readFile(os.path.join(self.client.root().decode('utf-8'), path))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('repo_path')
    parser.add_argument('file_path', nargs='+')
    args = parser.parse_args()
    blame = Blame(hglib.open(args.repo_path))
    attrib = blame.attribution(args.file_path)
    print(json.dumps(attrib, indent=4, separators=(',', ': ')))
