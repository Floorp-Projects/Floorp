#! /usr/bin/python

# This is a class which watches a maildir for new messages. It uses the
# linux dirwatcher API (if available) to look for new files. The
# .messageReceived method is invoked with the filename of the new message,
# relative to the 'new' directory of the maildir.

# this is an abstract base class. It must be subclassed by something to
# provide a delay function (which polls in the case that DNotify isn't
# available) and a way to safely schedule code to run after a signal handler
# has fired. See maildirgtk.py and maildirtwisted.py for forms that use the
# event loops provided by Gtk+ and Twisted.

try:
    from dnotify import DNotify
    have_dnotify = 1
except:
    have_dnotify = 0
import os

class Maildir:
    """This is a class which watches a maildir for new messages. Once
    started, it will run its .messageReceived method when a message is
    available.
    """
    def __init__(self, basedir=None):
        """Create the Maildir watcher. BASEDIR is the maildir directory (the
        one which contains new/ and tmp/)
        """
        self.basedir = basedir
        self.files = []
        self.pollinterval = 10  # only used if we don't have DNotify
        self.running = 0
        self.dnotify = None

    def setBasedir(self, basedir):
        self.basedir = basedir

    def start(self):
        """You must run start to receive any messages."""
        assert self.basedir
        self.newdir = os.path.join(self.basedir, "new")
        if self.running:
            return
        self.running = 1
        if not os.path.isdir(self.basedir) or not os.path.isdir(self.newdir):
            raise "invalid maildir '%s'" % self.basedir
        # we must hold an fd open on the directory, so we can get notified
        # when it changes.
        global have_dnotify
        if have_dnotify:
            try:
                self.dnotify = DNotify(self.newdir, self.dnotify_callback,
                                       [DNotify.DN_CREATE])
            except (IOError, OverflowError):
                # IOError is probably linux<2.4.19, which doesn't support
                # dnotify. OverflowError will occur on some 64-bit machines
                # because of a python bug
                print "DNotify failed, falling back to polling"
                have_dnotify = 0

        self.poll()

    def startTimeout(self):
        raise NotImplemented
    def stopTimeout(self):
        raise NotImplemented
    def dnotify_callback(self):
        print "callback"
        self.poll()
        raise NotImplemented
        
    def stop(self):
        if self.dnotify:
            self.dnotify.remove()
            self.dnotify = None
        else:
            self.stopTimeout()
        self.running = 0

    def poll(self):
        assert self.basedir
        # see what's new
        for f in self.files:
            if not os.path.isfile(os.path.join(self.newdir, f)):
                self.files.remove(f)
        newfiles = []
        for f in os.listdir(self.newdir):
            if not f in self.files:
                newfiles.append(f)
        self.files.extend(newfiles)
        # TODO: sort by ctime, then filename, since safecat uses a rather
        # fine-grained timestamp in the filename
        for n in newfiles:
            # TODO: consider catching exceptions in messageReceived
            self.messageReceived(n)
        if not have_dnotify:
            self.startTimeout()

    def messageReceived(self, filename):
        """Called when a new file is noticed. Override it in subclasses.
        Will receive path relative to maildir/new."""
        print filename


def test1():
    m = Maildir("ddir")
    m.start()
    import signal
    while 1:
        signal.pause()
    
if __name__ == '__main__':
    test1()
    
