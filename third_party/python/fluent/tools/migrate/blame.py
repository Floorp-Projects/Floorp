import argparse
import json
import hglib
from hglib.util import b, cmdbuilder
from compare_locales.parser import getParser, Junk


class Blame(object):
    def __init__(self, repopath):
        self.client = hglib.open(repopath)
        self.users = []
        self.blame = {}

    def main(self):
        args = cmdbuilder(
            b('annotate'), self.client.root(), d=True, u=True, T='json')
        blame_json = ''.join(self.client.rawcommand(args))
        file_blames = json.loads(blame_json)

        for file_blame in file_blames:
            self.handleFile(file_blame)

        return {'authors': self.users,
                'blame': self.blame}

    def handleFile(self, file_blame):
        abspath = file_blame['abspath']

        try:
            parser = getParser(abspath)
        except UserWarning:
            return

        self.blame[abspath] = {}

        parser.readFile(file_blame['path'])
        entities, emap = parser.parse()
        for e in entities:
            if isinstance(e, Junk):
                continue
            entity_lines = file_blame['lines'][
                (e.value_position()[0] - 1):e.value_position(-1)[0]
            ]
            # ignore timezone
            entity_lines.sort(key=lambda blame: -blame['date'][0])
            line_blame = entity_lines[0]
            user = line_blame['user']
            timestamp = line_blame['date'][0]  # ignore timezone
            if user not in self.users:
                self.users.append(user)
            userid = self.users.index(user)
            self.blame[abspath][e.key] = [userid, timestamp]

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("repopath")
    args = parser.parse_args()
    blame = Blame(args.repopath)
    blimey = blame.main()
    print(json.dumps(blimey, indent=4, separators=(',', ': ')))
