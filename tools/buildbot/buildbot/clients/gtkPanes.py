#! /usr/bin/python

from twisted.internet import gtk2reactor
gtk2reactor.install()

from twisted.internet import reactor

import sys, time

import pygtk
pygtk.require("2.0")
import gobject, gtk
assert(gtk.Window) # in gtk1 it's gtk.GtkWindow

from twisted.spread import pb

#from buildbot.clients.base import Builder, Client
from buildbot.clients.base import TextClient
from buildbot.util import now

'''
class Pane:
    def __init__(self):
        pass

class OneRow(Pane):
    """This is a one-row status bar. It has one square per Builder, and that
    square is either red, yellow, or green. """

    def __init__(self):
        Pane.__init__(self)
        self.widget = gtk.VBox(gtk.FALSE, 2)
        self.nameBox = gtk.HBox(gtk.TRUE)
        self.statusBox = gtk.HBox(gtk.TRUE)
        self.widget.add(self.nameBox)
        self.widget.add(self.statusBox)
        self.widget.show_all()
        self.builders = []
        
    def getWidget(self):
        return self.widget
    def addBuilder(self, builder):
        print "OneRow.addBuilder"
        # todo: ordering. Should follow the order in which they were added
        # to the original BotMaster
        self.builders.append(builder)
        # add the name to the left column, and a label (with background) to
        # the right
        name = gtk.Label(builder.name)
        status = gtk.Label('??')
        status.set_size_request(64,64)
        box = gtk.EventBox()
        box.add(status)
        name.show()
        box.show_all()
        self.nameBox.add(name)
        self.statusBox.add(box)
        builder.haveSomeWidgets([name, status, box])
    
class R2Builder(Builder):
    def start(self):
        self.nameSquare.set_text(self.name)
        self.statusSquare.set_text("???")
        self.subscribe()
    def haveSomeWidgets(self, widgets):
        self.nameSquare, self.statusSquare, self.statusBox = widgets

    def remote_newLastBuildStatus(self, event):
        color = None
        if event:
            text = "\n".join(event.text)
            color = event.color
        else:
            text = "none"
        self.statusSquare.set_text(text)
        if color:
            print "color", color
            self.statusBox.modify_bg(gtk.STATE_NORMAL,
                                     gtk.gdk.color_parse(color))

    def remote_currentlyOffline(self):
        self.statusSquare.set_text("offline")
    def remote_currentlyIdle(self):
        self.statusSquare.set_text("idle")
    def remote_currentlyWaiting(self, seconds):
        self.statusSquare.set_text("waiting")
    def remote_currentlyInterlocked(self):
        self.statusSquare.set_text("interlocked")
    def remote_currentlyBuilding(self, eta):
        self.statusSquare.set_text("building")


class CompactRow(Pane):
    def __init__(self):
        Pane.__init__(self)
        self.widget = gtk.VBox(gtk.FALSE, 3)
        self.nameBox = gtk.HBox(gtk.TRUE, 2)
        self.lastBuildBox = gtk.HBox(gtk.TRUE, 2)
        self.statusBox = gtk.HBox(gtk.TRUE, 2)
        self.widget.add(self.nameBox)
        self.widget.add(self.lastBuildBox)
        self.widget.add(self.statusBox)
        self.widget.show_all()
        self.builders = []
        
    def getWidget(self):
        return self.widget
        
    def addBuilder(self, builder):
        self.builders.append(builder)

        name = gtk.Label(builder.name)
        name.show()
        self.nameBox.add(name)

        last = gtk.Label('??')
        last.set_size_request(64,64)
        lastbox = gtk.EventBox()
        lastbox.add(last)
        lastbox.show_all()
        self.lastBuildBox.add(lastbox)

        status = gtk.Label('??')
        status.set_size_request(64,64)
        statusbox = gtk.EventBox()
        statusbox.add(status)
        statusbox.show_all()
        self.statusBox.add(statusbox)

        builder.haveSomeWidgets([name, last, lastbox, status, statusbox])

    def removeBuilder(self, name, builder):
        self.nameBox.remove(builder.nameSquare)
        self.lastBuildBox.remove(builder.lastBuildBox)
        self.statusBox.remove(builder.statusBox)
        self.builders.remove(builder)
    
class CompactBuilder(Builder):
    def setup(self):
        self.timer = None
        self.text = []
        self.eta = None
    def start(self):
        self.nameSquare.set_text(self.name)
        self.statusSquare.set_text("???")
        self.subscribe()
    def haveSomeWidgets(self, widgets):
        (self.nameSquare,
         self.lastBuildSquare, self.lastBuildBox,
         self.statusSquare, self.statusBox) = widgets
        
    def remote_currentlyOffline(self):
        self.eta = None
        self.stopTimer()
        self.statusSquare.set_text("offline")
        self.statusBox.modify_bg(gtk.STATE_NORMAL,
                                 gtk.gdk.color_parse("red"))
    def remote_currentlyIdle(self):
        self.eta = None
        self.stopTimer()
        self.statusSquare.set_text("idle")
    def remote_currentlyWaiting(self, seconds):
        self.nextBuild = now() + seconds
        self.startTimer(self.updateWaiting)
    def remote_currentlyInterlocked(self):
        self.stopTimer()
        self.statusSquare.set_text("interlocked")
    def startTimer(self, func):
        # the func must clear self.timer and return gtk.FALSE when the event
        # has arrived
        self.stopTimer()
        self.timer = gtk.timeout_add(1000, func)
        func()
    def stopTimer(self):
        if self.timer:
            gtk.timeout_remove(self.timer)
            self.timer = None
    def updateWaiting(self):
        when = self.nextBuild
        if now() < when:
            next = time.strftime("%H:%M:%S", time.localtime(when))
            secs = "[%d seconds]" % (when - now())
            self.statusSquare.set_text("waiting\n%s\n%s" % (next, secs))
            return gtk.TRUE # restart timer
        else:
            # done
            self.statusSquare.set_text("waiting\n[RSN]")
            self.timer = None
            return gtk.FALSE

    def remote_currentlyBuilding(self, eta):
        self.stopTimer()
        self.statusSquare.set_text("building")
        if eta:
            d = eta.callRemote("subscribe", self, 5)

    def remote_newLastBuildStatus(self, event):
        color = None
        if event:
            text = "\n".join(event.text)
            color = event.color
        else:
            text = "none"
        if not color: color = "gray"
        self.lastBuildSquare.set_text(text)
        self.lastBuildBox.modify_bg(gtk.STATE_NORMAL,
                                    gtk.gdk.color_parse(color))

    def remote_newEvent(self, event):
        assert(event.__class__ == GtkUpdatingEvent)
        self.current = event
        event.builder = self
        self.text = event.text
        if not self.text: self.text = ["idle"]
        self.eta = None
        self.stopTimer()
        self.updateText()
        color = event.color
        if not color: color = "gray"
        self.statusBox.modify_bg(gtk.STATE_NORMAL,
                                 gtk.gdk.color_parse(color))

    def updateCurrent(self):
        text = self.current.text
        if text:
            self.text = text
            self.updateText()
        color = self.current.color
        if color:
            self.statusBox.modify_bg(gtk.STATE_NORMAL,
                                     gtk.gdk.color_parse(color))
    def updateText(self):
        etatext = []
        if self.eta:
            etatext = [time.strftime("%H:%M:%S", time.localtime(self.eta))]
            if now() > self.eta:
                etatext += ["RSN"]
            else:
                seconds = self.eta - now()
                etatext += ["[%d secs]" % seconds]
        text = "\n".join(self.text + etatext)
        self.statusSquare.set_text(text)
    def updateTextTimer(self):
        self.updateText()
        return gtk.TRUE # restart timer
    
    def remote_progress(self, seconds):
        if seconds == None:
            self.eta = None
        else:
            self.eta = now() + seconds
        self.startTimer(self.updateTextTimer)
        self.updateText()
    def remote_finished(self, eta):
        self.eta = None
        self.stopTimer()
        self.updateText()
        eta.callRemote("unsubscribe", self)
'''

class Box:
    def __init__(self, text="?"):
        self.text = text
        self.box = gtk.EventBox()
        self.label = gtk.Label(text)
        self.box.add(self.label)
        self.box.set_size_request(64,64)
        self.timer = None

    def getBox(self):
        return self.box

    def setText(self, text):
        self.text = text
        self.label.set_text(text)

    def setColor(self, color):
        if not color:
            return
        self.box.modify_bg(gtk.STATE_NORMAL, gtk.gdk.color_parse(color))

    def setETA(self, eta):
        if eta:
            self.when = now() + eta
            self.startTimer()
        else:
            self.stopTimer()

    def startTimer(self):
        self.stopTimer()
        self.timer = gobject.timeout_add(1000, self.update)
        self.update()

    def stopTimer(self):
        if self.timer:
            gobject.source_remove(self.timer)
            self.timer = None
        self.label.set_text(self.text)

    def update(self):
        if now() < self.when:
            next = time.strftime("%H:%M:%S", time.localtime(self.when))
            secs = "[%d secs]" % (self.when - now())
            self.label.set_text("%s\n%s\n%s" % (self.text, next, secs))
            return True # restart timer
        else:
            # done
            self.label.set_text("%s\n[soon]\n[overdue]" % (self.text,))
            self.timer = None
            return False



class ThreeRowBuilder:
    def __init__(self, name, ref):
        self.name = name

        self.last = Box()
        self.current = Box()
        self.step = Box("idle")
        self.step.setColor("white")

        self.ref = ref

    def getBoxes(self):
        return self.last.getBox(), self.current.getBox(), self.step.getBox()

    def getLastBuild(self):
        d = self.ref.callRemote("getLastFinishedBuild")
        d.addCallback(self.gotLastBuild)
    def gotLastBuild(self, build):
        if build:
            build.callRemote("getText").addCallback(self.gotLastText)
            build.callRemote("getColor").addCallback(self.gotLastColor)

    def gotLastText(self, text):
        self.last.setText("\n".join(text))
    def gotLastColor(self, color):
        self.last.setColor(color)

    def getState(self):
        self.ref.callRemote("getState").addCallback(self.gotState)
    def gotState(self, res):
        state, ETA, builds = res
        # state is one of: offline, idle, waiting, interlocked, building
        # TODO: ETA is going away, you have to look inside the builds to get
        # that value
        currentmap = {"offline": "red",
                      "idle": "white",
                      "waiting": "yellow",
                      "interlocked": "yellow",
                      "building": "yellow",}
        text = state
        self.current.setColor(currentmap[state])
        if ETA is not None:
            text += "\nETA=%s secs" % ETA
        self.current.setText(state)

    def buildStarted(self, build):
        print "[%s] buildStarted" % (self.name,)
        self.current.setColor("yellow")

    def buildFinished(self, build, results):
        print "[%s] buildFinished: %s" % (self.name, results)
        self.gotLastBuild(build)
        self.current.setColor("white")
        self.current.stopTimer()

    def buildETAUpdate(self, eta):
        print "[%s] buildETAUpdate: %s" % (self.name, eta)
        self.current.setETA(eta)


    def stepStarted(self, stepname, step):
        print "[%s] stepStarted: %s" % (self.name, stepname)
        self.step.setText(stepname)
        self.step.setColor("yellow")
    def stepFinished(self, stepname, step, results):
        print "[%s] stepFinished: %s %s" % (self.name, stepname, results)
        self.step.setText("idle")
        self.step.setColor("white")
        self.step.stopTimer()
    def stepETAUpdate(self, stepname, eta):
        print "[%s] stepETAUpdate: %s %s" % (self.name, stepname, eta)
        self.step.setETA(eta)


class ThreeRowClient(pb.Referenceable):
    def __init__(self, window):
        self.window = window
        self.buildernames = []
        self.builders = {}

    def connected(self, ref):
        print "connected"
        self.ref = ref
        self.pane = gtk.VBox(False, 2)
        self.table = gtk.Table(1+3, 1)
        self.pane.add(self.table)
        self.window.vb.add(self.pane)
        self.pane.show_all()
        ref.callRemote("subscribe", "logs", 5, self)

    def removeTable(self):
        for child in self.table.get_children():
            self.table.remove(child)
        self.pane.remove(self.table)

    def makeTable(self):
        columns = len(self.builders)
        self.table = gtk.Table(2, columns)
        self.pane.add(self.table)
        for i in range(len(self.buildernames)):
            name = self.buildernames[i]
            b = self.builders[name]
            last,current,step = b.getBoxes()
            self.table.attach(gtk.Label(name), i, i+1, 0, 1)
            self.table.attach(last, i, i+1, 1, 2,
                              xpadding=1, ypadding=1)
            self.table.attach(current, i, i+1, 2, 3,
                              xpadding=1, ypadding=1)
            self.table.attach(step, i, i+1, 3, 4,
                              xpadding=1, ypadding=1)
        self.table.show_all()

    def rebuildTable(self):
        self.removeTable()
        self.makeTable()

    def remote_builderAdded(self, buildername, builder):
        print "builderAdded", buildername
        assert buildername not in self.buildernames
        self.buildernames.append(buildername)

        b = ThreeRowBuilder(buildername, builder)
        self.builders[buildername] = b
        self.rebuildTable()
        b.getLastBuild()
        b.getState()

    def remote_builderRemoved(self, buildername):
        del self.builders[buildername]
        self.buildernames.remove(buildername)
        self.rebuildTable()

    def remote_builderChangedState(self, name, state, eta):
        self.builders[name].gotState((state, eta, None))
    def remote_buildStarted(self, name, build):
        self.builders[name].buildStarted(build)
    def remote_buildFinished(self, name, build, results):
        self.builders[name].buildFinished(build, results)

    def remote_buildETAUpdate(self, name, build, eta):
        self.builders[name].buildETAUpdate(eta)
    def remote_stepStarted(self, name, build, stepname, step):
        self.builders[name].stepStarted(stepname, step)
    def remote_stepFinished(self, name, build, stepname, step, results):
        self.builders[name].stepFinished(stepname, step, results)

    def remote_stepETAUpdate(self, name, build, stepname, step,
                             eta, expectations):
        # expectations is a list of (metricname, current_value,
        # expected_value) tuples, so that we could show individual progress
        # meters for each metric
        self.builders[name].stepETAUpdate(stepname, eta)

    def remote_logStarted(self, buildername, build, stepname, step,
                          logname, log):
        pass

    def remote_logFinished(self, buildername, build, stepname, step,
                           logname, log):
        pass


class GtkClient(TextClient):
    ClientClass = ThreeRowClient

    def __init__(self, master):
        self.master = master

        w = gtk.Window()
        self.w = w
        #w.set_size_request(64,64)
        w.connect('destroy', lambda win: gtk.main_quit())
        self.vb = gtk.VBox(False, 2)
        self.status = gtk.Label("unconnected")
        self.vb.add(self.status)
        self.listener = self.ClientClass(self)
        w.add(self.vb)
        w.show_all()

    def connected(self, ref):
        self.status.set_text("connected")
        TextClient.connected(self, ref)

"""
    def addBuilder(self, name, builder):
        Client.addBuilder(self, name, builder)
        self.pane.addBuilder(builder)
    def removeBuilder(self, name):
        self.pane.removeBuilder(name, self.builders[name])
        Client.removeBuilder(self, name)
        
    def startConnecting(self, master):
        self.master = master
        Client.startConnecting(self, master)
        self.status.set_text("connecting to %s.." % master)
    def connected(self, remote):
        Client.connected(self, remote)
        self.status.set_text(self.master)
        remote.notifyOnDisconnect(self.disconnected)
    def disconnected(self, remote):
        self.status.set_text("disconnected, will retry")
"""

def main():
    master = "localhost:8007"
    if len(sys.argv) > 1:
        master = sys.argv[1]
    c = GtkClient(master)
    c.run()

if __name__ == '__main__':
    main()
    
