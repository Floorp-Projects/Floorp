import subprocess
from hglib import client, util, error

HGPATH = 'hg'

def open(path=None, encoding=None, configs=None):
    '''starts a cmdserver for the given path (or for a repository found
    in the cwd). HGENCODING is set to the given encoding. configs is a
    list of key, value, similar to those passed to hg --config.
    '''
    return client.hgclient(path, encoding, configs)

def init(dest=None, ssh=None, remotecmd=None, insecure=False,
         encoding=None, configs=None):
    args = util.cmdbuilder('init', dest, e=ssh, remotecmd=remotecmd,
                           insecure=insecure)

    args.insert(0, HGPATH)
    proc = util.popen(args)
    out, err = proc.communicate()
    if proc.returncode:
        raise error.CommandError(args, proc.returncode, out, err)

    return client.hgclient(dest, encoding, configs, connect=False)

def clone(source=None, dest=None, noupdate=False, updaterev=None, rev=None,
          branch=None, pull=False, uncompressed=False, ssh=None, remotecmd=None,
          insecure=False, encoding=None, configs=None):
    args = util.cmdbuilder('clone', source, dest, noupdate=noupdate,
                           updaterev=updaterev, rev=rev, branch=branch,
                           pull=pull, uncompressed=uncompressed,
                           e=ssh, remotecmd=remotecmd, insecure=insecure)

    args.insert(0, HGPATH)
    proc = util.popen(args)
    out, err = proc.communicate()
    if proc.returncode:
        raise error.CommandError(args, proc.returncode, out, err)

    return client.hgclient(dest, encoding, configs, connect=False)
