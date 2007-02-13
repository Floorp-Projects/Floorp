#! /usr/bin/python

# This is a class which watches a maildir for new messages. It uses the
# linux dirwatcher API (if available) to look for new files. The
# .messageReceived method is invoked with the filename of the new message,
# relative to the top of the maildir (so it will look like "new/blahblah").

# This form uses the Gtk event loop to handle polling and signal safety

if __name__ == '__main__':
    import pygtk
    pygtk.require("2.0")

import gtk
from maildir import Maildir

class MaildirGtk(Maildir):
    def __init__(self, basedir):
        Maildir.__init__(self, basedir)
        self.idler = None
    def startTimeout(self):
        self.timeout = gtk.timeout_add(self.pollinterval*1000, self.doTimeout)
    def doTimeout(self):
        self.poll()
        return gtk.TRUE # keep going
    def stopTimeout(self):
        if self.timeout:
            gtk.timeout_remove(self.timeout)
            self.timeout = None
    def dnotify_callback(self):
        # make it safe
        self.idler = gtk.idle_add(self.idlePoll)
    def idlePoll(self):
        gtk.idle_remove(self.idler)
        self.idler = None
        self.poll()
        return gtk.FALSE

def test1():
    class MaildirTest(MaildirGtk):
        def messageReceived(self, filename):
            print "changed:", filename
    m = MaildirTest("ddir")
    print "watching ddir/new/"
    m.start()
    #gtk.main()
    # to allow the python-side signal handler to run, we must surface from
    # gtk (which blocks on the C-side) every once in a while.
    while 1:
        gtk.mainiteration() # this will block until there is something to do
    m.stop()
    print "done"
    
if __name__ == '__main__':
    test1()
