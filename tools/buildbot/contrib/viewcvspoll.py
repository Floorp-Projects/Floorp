#! /usr/bin/python

"""Based on the fakechanges.py contrib script"""

import sys
from twisted.spread import pb
from twisted.cred import credentials
from twisted.internet import reactor, task
from twisted.python import log
import commands, random, os.path, time, MySQLdb

class ViewCvsPoller:

    def __init__(self):
        def _load_rc():
            import user
            ret = {}
            for line in open(os.path.join(user.home,".cvsblamerc")).readlines():
                if line.find("=") != -1:
                    key, val = line.split("=")
                    ret[key.strip()] = val.strip()
            return ret
        # maybe add your own keys here db=xxx, user=xxx, passwd=xxx
        self.cvsdb = MySQLdb.connect("cvs", **_load_rc())
        #self.last_checkin = "2005-05-11" # for testing
        self.last_checkin = time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime())

    def get_changes(self):
        changes = []

        def empty_change():
            return {'who': None, 'files': [], 'comments': None }
        change = empty_change()

        cursor = self.cvsdb.cursor()
        cursor.execute("""SELECT whoid, descid, fileid, dirid, branchid, ci_when
            FROM checkins WHERE ci_when>='%s'""" % self.last_checkin)
        last_checkin = None
        for whoid, descid, fileid, dirid, branchid, ci_when in cursor.fetchall():
            if branchid != 1: # only head
                continue
            cursor.execute("""SELECT who from people where id=%s""" % whoid)
            who = cursor.fetchone()[0]
            cursor.execute("""SELECT description from descs where id=%s""" % descid)
            desc = cursor.fetchone()[0]
            cursor.execute("""SELECT file from files where id=%s""" % fileid)
            filename = cursor.fetchone()[0]
            cursor.execute("""SELECT dir from dirs where id=%s""" % dirid)
            dirname = cursor.fetchone()[0]
            if who == change["who"] and desc == change["comments"]:
                change["files"].append( "%s/%s" % (dirname, filename) )
            elif change["who"]:
                changes.append(change)
                change = empty_change()
            else:
                change["who"] = who
                change["files"].append( "%s/%s" % (dirname, filename) )
                change["comments"] = desc
            if last_checkin == None or ci_when > last_checkin:
                last_checkin = ci_when
        if last_checkin:
            self.last_checkin = last_checkin
        return changes

poller = ViewCvsPoller()

def error(*args):
    log.err()
    reactor.stop()

def poll_changes(remote):
    print "GET CHANGES SINCE", poller.last_checkin,
    changes = poller.get_changes()
    for change in changes:
        print change["who"], "\n *", "\n * ".join(change["files"])
        remote.callRemote('addChange', change).addErrback(error)
    print
    reactor.callLater(60, poll_changes, remote)

factory = pb.PBClientFactory()
reactor.connectTCP("localhost", 9999, factory )
deferred = factory.login(credentials.UsernamePassword("change", "changepw"))
deferred.addCallback(poll_changes).addErrback(error)

reactor.run()
