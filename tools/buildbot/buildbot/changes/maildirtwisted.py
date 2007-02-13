#! /usr/bin/python

# This is a class which watches a maildir for new messages. It uses the
# linux dirwatcher API (if available) to look for new files. The
# .messageReceived method is invoked with the filename of the new message,
# relative to the top of the maildir (so it will look like "new/blahblah").

# This version is implemented as a Twisted Python "Service". It uses the
# twisted Reactor to handle polling and signal safety.

from twisted.application import service
from twisted.internet import reactor
from maildir import Maildir

class MaildirTwisted(Maildir, service.Service):
    timeout = None

    def startService(self):
        self.start()
        service.Service.startService(self)
    def stopService(self):
        self.stop()
        service.Service.stopService(self)

    def startTimeout(self):
        self.timeout = reactor.callLater(self.pollinterval, self.poll)
    def stopTimeout(self):
        if self.timeout:
            self.timeout.cancel()
            self.timeout = None

    def dnotify_callback(self):
        # make it safe
        #reactor.callFromThread(self.poll)
        reactor.callLater(1, self.poll)
        # give it a moment. I found that qmail had problems when the message
        # was removed from the maildir instantly. It shouldn't, that's what
        # maildirs are made for. I wasn't able to eyeball any reason for the
        # problem, and safecat didn't behave the same way, but qmail reports
        # "Temporary_error_on_maildir_delivery" (qmail-local.c:165,
        # maildir_child() process exited with rc not in 0,2,3,4). Not sure why,
        # would have to hack qmail to investigate further, easier to just
        # wait a second before yanking the message out of new/ .

##     def messageReceived(self, filename):
##         if self.callback:
##             self.callback(filename)

class MaildirService(MaildirTwisted):
    """I watch a maildir for new messages. I should be placed as the service
    child of some MultiService instance. When running, I use the linux
    dirwatcher API (if available) or poll for new files in the 'new'
    subdirectory of my maildir path. When I discover a new message, I invoke
    my parent's .messageReceived() method with the short filename of the new
    message, so the full name of the new file can be obtained with
    os.path.join(maildir, 'new', filename). I will not move or delete the
    file on my own: the parent should do this in messageReceived().
    """
    def messageReceived(self, filename):
        self.parent.messageReceived(filename)


def test1():
    class MaildirTest(MaildirTwisted):
        def messageReceived(self, filename):
            print "changed:", filename
    m = MaildirTest(basedir="ddir")
    print "watching ddir/new/"
    m.startService()
    reactor.run()
    print "done"
    
if __name__ == '__main__':
    test1()
    

