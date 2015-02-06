#! /usr/bin/env python

import argparse

def repo_url(remote, revision, path):
    '''
    Construct a url pointing to the _raw_ file in the given remote this function
    will handle url construction for both hg and github.
    '''

    # Ensure remote always ends in a slash...
    if remote[-1] != '/':
        remote = remote + '/'
    if 'hg.mozilla.org' in remote:
        return '{}raw-file/{}/{}'.format(remote, revision, path);
    else:
        return '{}raw/{}/{}'.format(remote, revision, path);

parser = argparse.ArgumentParser(
    description='Get url for raw file in remote repository'
)

parser.add_argument('remote', help='URL for remote repository')
parser.add_argument('revision', help='Revision in remote repository')
parser.add_argument('path', help='Path to file in remote repository')

args = parser.parse_args()
print(repo_url(args.remote, args.revision, args.path))
