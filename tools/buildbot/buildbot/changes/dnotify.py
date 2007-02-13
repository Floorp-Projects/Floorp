#! /usr/bin/python

import fcntl, signal, os

class DNotify_Handler:
    def __init__(self):
        self.watchers = {}
        self.installed = 0
    def install(self):
        if self.installed:
            return
        signal.signal(signal.SIGIO, self.fire)
        self.installed = 1
    def uninstall(self):
        if not self.installed:
            return
        signal.signal(signal.SIGIO, signal.SIG_DFL)
        self.installed = 0
    def add(self, watcher):
        self.watchers[watcher.fd] = watcher
        self.install()
    def remove(self, watcher):
        if self.watchers.has_key(watcher.fd):
            del(self.watchers[watcher.fd])
            if not self.watchers:
                self.uninstall()
    def fire(self, signum, frame):
        # this is the signal handler
        # without siginfo_t, we must fire them all
        for watcher in self.watchers.values():
            watcher.callback()
            
class DNotify:
    DN_ACCESS = fcntl.DN_ACCESS  # a file in the directory was read
    DN_MODIFY = fcntl.DN_MODIFY  # a file was modified (write,truncate)
    DN_CREATE = fcntl.DN_CREATE  # a file was created
    DN_DELETE = fcntl.DN_DELETE  # a file was unlinked
    DN_RENAME = fcntl.DN_RENAME  # a file was renamed
    DN_ATTRIB = fcntl.DN_ATTRIB  # a file had attributes changed (chmod,chown)

    handler = [None]
    
    def __init__(self, dirname, callback=None,
                 flags=[DN_MODIFY,DN_CREATE,DN_DELETE,DN_RENAME]):

        """This object watches a directory for changes. The .callback
        attribute should be set to a function to be run every time something
        happens to it. Be aware that it will be called more times than you
        expect."""

        if callback:
            self.callback = callback
        else:
            self.callback = self.fire
        self.dirname = dirname
        self.flags = reduce(lambda x, y: x | y, flags) | fcntl.DN_MULTISHOT
        self.fd = os.open(dirname, os.O_RDONLY)
        # ideally we would move the notification to something like SIGRTMIN,
        # (to free up SIGIO) and use sigaction to have the signal handler
        # receive a structure with the fd number. But python doesn't offer
        # either.
        if not self.handler[0]:
            self.handler[0] = DNotify_Handler()
        self.handler[0].add(self)
        fcntl.fcntl(self.fd, fcntl.F_NOTIFY, self.flags)
    def remove(self):
        self.handler[0].remove(self)
        os.close(self.fd)
    def fire(self):
        print self.dirname, "changed!"

def test_dnotify1():
    d = DNotify(".")
    while 1:
        signal.pause()

def test_dnotify2():
    # create ./foo/, create/delete files in ./ and ./foo/ while this is
    # running. Notice how both notifiers are fired when anything changes;
    # this is an unfortunate side-effect of the lack of extended sigaction
    # support in Python.
    count = [0]
    d1 = DNotify(".")
    def fire1(count=count, d1=d1):
        print "./ changed!", count[0]
        count[0] += 1
        if count[0] > 5:
            d1.remove()
            del(d1)
    # change the callback, since we can't define it until after we have the
    # dnotify object. Hmm, unless we give the dnotify to the callback.
    d1.callback = fire1
    def fire2(): print "foo/ changed!"
    d2 = DNotify("foo", fire2)
    while 1:
        signal.pause()
        
    
if __name__ == '__main__':
    test_dnotify2()
    
