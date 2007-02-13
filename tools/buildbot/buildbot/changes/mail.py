# -*- test-case-name: buildbot.test.test_mailparse -*-

"""
Parse various kinds of 'CVS notify' email.
"""
import os, re
from rfc822 import Message

from buildbot import util
from buildbot.twcompat import implements
from buildbot.changes import base, changes, maildirtwisted

def parseFreshCVSMail(self, fd, prefix=None, sep="/"):
    """Parse mail sent by FreshCVS"""
    # this uses rfc822.Message so it can run under python2.1 . In the future
    # it will be updated to use python2.2's "email" module.

    m = Message(fd)
    # FreshCVS sets From: to "user CVS <user>", but the <> part may be
    # modified by the MTA (to include a local domain)
    name, addr = m.getaddr("from")
    if not name:
        return None # no From means this message isn't from FreshCVS
    cvs = name.find(" CVS")
    if cvs == -1:
        return None # this message isn't from FreshCVS
    who = name[:cvs]

    # we take the time of receipt as the time of checkin. Not correct, but it
    # avoids the out-of-order-changes issue. See the comment in parseSyncmail
    # about using the 'Date:' header
    when = util.now()

    files = []
    comments = ""
    isdir = 0
    lines = m.fp.readlines()
    while lines:
        line = lines.pop(0)
        if line == "Modified files:\n":
            break
    while lines:
        line = lines.pop(0)
        if line == "\n":
            break
        line = line.rstrip("\n")
        linebits = line.split(None, 1)
        file = linebits[0]
        if prefix:
            # insist that the file start with the prefix: FreshCVS sends
            # changes we don't care about too
            bits = file.split(sep)
            if bits[0] == prefix:
                file = sep.join(bits[1:])
            else:
                break
        if len(linebits) == 1:
            isdir = 1
        elif linebits[1] == "0 0":
            isdir = 1
        files.append(file)
    while lines:
        line = lines.pop(0)
        if line == "Log message:\n":
            break
    # message is terminated by "ViewCVS links:" or "Index:..." (patch)
    while lines:
        line = lines.pop(0)
        if line == "ViewCVS links:\n":
            break
        if line.find("Index: ") == 0:
            break
        comments += line
    comments = comments.rstrip() + "\n"

    if not files:
        return None
    
    change = changes.Change(who, files, comments, isdir, when=when)

    return change

def parseSyncmail(self, fd, prefix=None, sep="/"):
    """Parse messages sent by the 'syncmail' program, as suggested by the
    sourceforge.net CVS Admin documentation. Syncmail is maintained at
    syncmail.sf.net .
    """
    # pretty much the same as freshcvs mail, not surprising since CVS is the
    # one creating most of the text

    m = Message(fd)
    # The mail is sent from the person doing the checkin. Assume that the
    # local username is enough to identify them (this assumes a one-server
    # cvs-over-rsh environment rather than the server-dirs-shared-over-NFS
    # model)
    name, addr = m.getaddr("from")
    if not addr:
        return None # no From means this message isn't from FreshCVS
    at = addr.find("@")
    if at == -1:
        who = addr # might still be useful
    else:
        who = addr[:at]

    # we take the time of receipt as the time of checkin. Not correct (it
    # depends upon the email latency), but it avoids the out-of-order-changes
    # issue. Also syncmail doesn't give us anything better to work with,
    # unless you count pulling the v1-vs-v2 timestamp out of the diffs, which
    # would be ugly. TODO: Pulling the 'Date:' header from the mail is a
    # possibility, and email.Utils.parsedate_tz may be useful. It should be
    # configurable, however, because there are a lot of broken clocks out
    # there.
    when = util.now()

    subject = m.getheader("subject")
    # syncmail puts the repository-relative directory in the subject:
    # mprefix + "%(dir)s %(file)s,%(oldversion)s,%(newversion)s", where
    # 'mprefix' is something that could be added by a mailing list
    # manager.
    # this is the only reasonable way to determine the directory name
    space = subject.find(" ")
    if space != -1:
        directory = subject[:space]
    else:
        directory = subject
    
    files = []
    comments = ""
    isdir = 0
    branch = None

    lines = m.fp.readlines()
    while lines:
        line = lines.pop(0)

        if (line == "Modified Files:\n" or
            line == "Added Files:\n" or
            line == "Removed Files:\n"):
            break

    while lines:
        line = lines.pop(0)
        if line == "\n":
            break
        if line == "Log Message:\n":
            lines.insert(0, line)
            break
        line = line.lstrip()
        line = line.rstrip()
        # note: syncmail will send one email per directory involved in a
        # commit, with multiple files if they were in the same directory.
        # Unlike freshCVS, it makes no attempt to collect all related
        # commits into a single message.

        # note: syncmail will report a Tag underneath the ... Files: line
        # e.g.:       Tag: BRANCH-DEVEL

        if line.startswith('Tag:'):
            branch = line.split(' ')[-1].rstrip()
            continue

        # note: it doesn't actually make sense to use portable functions
        # like os.path.join and os.sep, because these filenames all use
        # separator conventions established by the remote CVS server (which
        # is probably running on unix), not the local buildmaster system.
        thesefiles = line.split(" ")
        for f in thesefiles:
            f = sep.join([directory, f])
            if prefix:
                # insist that the file start with the prefix: we may get
                # changes we don't care about too
                bits = f.split(sep)
                if bits[0] == prefix:
                    f = sep.join(bits[1:])
                else:
                    break
            # TODO: figure out how new directories are described, set .isdir
            files.append(f)

    if not files:
        return None

    while lines:
        line = lines.pop(0)
        if line == "Log Message:\n":
            break
    # message is terminated by "Index:..." (patch) or "--- NEW FILE.."
    # or "--- filename DELETED ---". Sigh.
    while lines:
        line = lines.pop(0)
        if line.find("Index: ") == 0:
            break
        if re.search(r"^--- NEW FILE", line):
            break
        if re.search(r" DELETED ---$", line):
            break
        comments += line
    comments = comments.rstrip() + "\n"
    
    change = changes.Change(who, files, comments, isdir, when=when,
                            branch=branch)

    return change

# Bonsai mail parser by Stephen Davis.
#
# This handles changes for CVS repositories that are watched by Bonsai
# (http://www.mozilla.org/bonsai.html)

# A Bonsai-formatted email message looks like:
# 
# C|1071099907|stephend|/cvs|Sources/Scripts/buildbot|bonsai.py|1.2|||18|7
# A|1071099907|stephend|/cvs|Sources/Scripts/buildbot|master.cfg|1.1|||18|7
# R|1071099907|stephend|/cvs|Sources/Scripts/buildbot|BuildMaster.py|||
# LOGCOMMENT
# Updated bonsai parser and switched master config to buildbot-0.4.1 style.
# 
# :ENDLOGCOMMENT
#
# In the first example line, stephend is the user, /cvs the repository,
# buildbot the directory, bonsai.py the file, 1.2 the revision, no sticky
# and branch, 18 lines added and 7 removed. All of these fields might not be
# present (during "removes" for example).
#
# There may be multiple "control" lines or even none (imports, directory
# additions) but there is one email per directory. We only care about actual
# changes since it is presumed directory additions don't actually affect the
# build. At least one file should need to change (the makefile, say) to
# actually make a new directory part of the build process. That's my story
# and I'm sticking to it.

def parseBonsaiMail(self, fd, prefix=None, sep="/"):
    """Parse mail sent by the Bonsai cvs loginfo script."""

    msg = Message(fd)

    # we don't care who the email came from b/c the cvs user is in the msg
    # text
    
    who = "unknown"
    timestamp = None
    files = []
    lines = msg.fp.readlines()

    # read the control lines (what/who/where/file/etc.)
    while lines:
        line = lines.pop(0)
        if line == "LOGCOMMENT\n":
            break;
        line = line.rstrip("\n")
        
        # we'd like to do the following but it won't work if the number of
        # items doesn't match so...
        #   what, timestamp, user, repo, module, file = line.split( '|' )
        items = line.split('|')
        if len(items) < 6:
            # not a valid line, assume this isn't a bonsai message
            return None

        try:
            # just grab the bottom-most timestamp, they're probably all the
            # same. TODO: I'm assuming this is relative to the epoch, but
            # this needs testing.
            timestamp = int(items[1])
        except ValueError:
            pass

        user = items[2]
        if user:
            who = user

        module = items[4]
        file = items[5]
        if module and file:
            path = "%s/%s" % (module, file)
            files.append(path)
        sticky = items[7]
        branch = items[8]

    # if no files changed, return nothing
    if not files:
        return None

    # read the comments
    comments = ""
    while lines:
        line = lines.pop(0)
        if line == ":ENDLOGCOMMENT\n":
            break
        comments += line
    comments = comments.rstrip() + "\n"

    # return buildbot Change object
    return changes.Change(who, files, comments, when=timestamp, branch=branch)



class MaildirSource(maildirtwisted.MaildirTwisted, base.ChangeSource):
    """This source will watch a maildir that is subscribed to a FreshCVS
    change-announcement mailing list.
    """
    # we need our own implements() here, at least for twisted-1.3, because
    # the double-inheritance of Service shadows __implements__ from
    # ChangeSource.
    if not implements:
        __implements__ = base.ChangeSource.__implements__

    compare_attrs = ["basedir", "newdir", "pollinterval", "parser"]
    parser = None
    name = None

    def __init__(self, maildir, prefix=None, sep="/"):
        maildirtwisted.MaildirTwisted.__init__(self, maildir)
        self.prefix = prefix
        self.sep = sep

    def describe(self):
        return "%s mailing list in maildir %s" % (self.name, self.basedir)

    def messageReceived(self, filename):
        path = os.path.join(self.basedir, "new", filename)
        change = self.parser(open(path, "r"), self.prefix, self.sep)
        if change:
            self.parent.addChange(change)
        os.rename(os.path.join(self.basedir, "new", filename),
                  os.path.join(self.basedir, "cur", filename))

class FCMaildirSource(MaildirSource):
    parser = parseFreshCVSMail
    name = "FreshCVS"

class SyncmailMaildirSource(MaildirSource):
    parser = parseSyncmail
    name = "Syncmail"

class BonsaiMaildirSource(MaildirSource):
    parser = parseBonsaiMail
    name = "Bonsai"
