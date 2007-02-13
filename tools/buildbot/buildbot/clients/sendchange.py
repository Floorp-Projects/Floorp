
from twisted.spread import pb
from twisted.cred import credentials
from twisted.internet import reactor

class Sender:
    def __init__(self, master, user):
        self.user = user
        self.host, self.port = master.split(":")
        self.port = int(self.port)

    def send(self, branch, revision, comments, files):
        change = {'who': self.user, 'files': files, 'comments': comments,
                  'branch': branch, 'revision': revision}

        f = pb.PBClientFactory()
        d = f.login(credentials.UsernamePassword("change", "changepw"))
        reactor.connectTCP(self.host, self.port, f)
        d.addCallback(self.addChange, change)
        return d

    def addChange(self, remote, change):
        d = remote.callRemote('addChange', change)
        d.addCallback(lambda res: remote.broker.transport.loseConnection())
        return d

    def printSuccess(self, res):
        print "change sent successfully"
    def printFailure(self, why):
        print "change NOT sent"
        print why

    def stop(self, res):
        reactor.stop()
        return res

    def run(self):
        reactor.run()
