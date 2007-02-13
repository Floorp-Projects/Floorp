#! /usr/bin/python

# code to deliver build status through twisted.words (instant messaging
# protocols: irc, etc)

import re, shlex

from twisted.internet import protocol, reactor
try:
    # Twisted-2.0
    from twisted.words.protocols import irc
except ImportError:
    # Twisted-1.3
    from twisted.protocols import irc
from twisted.python import log, failure
from twisted.application import internet

from buildbot import interfaces, util
from buildbot import version
from buildbot.sourcestamp import SourceStamp
from buildbot.process.base import BuildRequest
from buildbot.status import base
from buildbot.status.builder import SUCCESS, WARNINGS, FAILURE, EXCEPTION
from buildbot.scripts.runner import ForceOptions

class UsageError(ValueError):
    def __init__(self, string = "Invalid usage", *more):
        ValueError.__init__(self, string, *more)

class IrcBuildRequest:
    hasStarted = False
    timer = None

    def __init__(self, parent, reply):
        self.parent = parent
        self.reply = reply
        self.timer = reactor.callLater(5, self.soon)

    def soon(self):
        del self.timer
        if not self.hasStarted:
            self.parent.reply(self.reply,
                              "The build has been queued, I'll give a shout"
                              " when it starts")

    def started(self, c):
        self.hasStarted = True
        if self.timer:
            self.timer.cancel()
            del self.timer
        s = c.getStatus()
        eta = s.getETA()
        response = "build #%d forced" % s.getNumber()
        if eta is not None:
            response = "build forced [ETA %s]" % self.parent.convertTime(eta)
        self.parent.reply(self.reply, response)
        self.parent.reply(self.reply,
                          "I'll give a shout when the build finishes")
        d = s.waitUntilFinished()
        d.addCallback(self.parent.buildFinished, self.reply)


class IrcStatusBot(irc.IRCClient):
    silly = {
        "What happen ?": "Somebody set up us the bomb.",
        "It's You !!": ["How are you gentlemen !!",
                        "All your base are belong to us.",
                        "You are on the way to destruction."],
        "What you say !!": ["You have no chance to survive make your time.",
                            "HA HA HA HA ...."],
        }
    def __init__(self, nickname, password, channels, status, categories):
        """
        @type  nickname: string
        @param nickname: the nickname by which this bot should be known
        @type  password: string
        @param password: the password to use for identifying with Nickserv
        @type  channels: list of strings
        @param channels: the bot will maintain a presence in these channels
        @type  status: L{buildbot.status.builder.Status}
        @param status: the build master's Status object, through which the
                       bot retrieves all status information
        """
        self.nickname = nickname
        self.channels = channels
        self.password = password
        self.status = status
        self.categories = categories
        self.counter = 0
        self.hasQuit = 0

    def signedOn(self):
        if self.password:
            self.msg("Nickserv", "IDENTIFY " + self.password)
        for c in self.channels:
            self.join(c)
    def joined(self, channel):
        log.msg("I have joined", channel)
    def left(self, channel):
        log.msg("I have left", channel)
    def kickedFrom(self, channel, kicker, message):
        log.msg("I have been kicked from %s by %s: %s" % (channel,
                                                          kicker,
                                                          message))

    # input
    def privmsg(self, user, channel, message):
        user = user.split('!', 1)[0] # rest is ~user@hostname
        # channel is '#twisted' or 'buildbot' (for private messages)
        channel = channel.lower()
        #print "privmsg:", user, channel, message
        if channel == self.nickname:
            # private message
            message = "%s: %s" % (self.nickname, message)
            reply = user
        else:
            reply = channel
        if message.startswith("%s:" % self.nickname):
            message = message[len("%s:" % self.nickname):]

            message = message.lstrip()
            if self.silly.has_key(message):
                return self.doSilly(user, reply, message)

            parts = message.split(' ', 1)
            if len(parts) == 1:
                parts = parts + ['']
            cmd, args = parts
            log.msg("irc command", cmd)

            meth = self.getCommandMethod(cmd)
            if not meth and message[-1] == '!':
                meth = self.command_EXCITED

            error = None
            try:
                if meth:
                    meth(user, reply, args.strip())
            except UsageError, e:
                self.reply(reply, str(e))
            except:
                f = failure.Failure()
                log.err(f)
                error = "Something bad happened (see logs): %s" % f.type

            if error:
                try:
                    self.reply(reply, error)
                except:
                    log.err()

            #self.say(channel, "count %d" % self.counter)
            self.counter += 1
    def reply(self, dest, message):
        # maybe self.notice(dest, message) instead?
        self.msg(dest, message)

    def getCommandMethod(self, command):
        meth = getattr(self, 'command_' + command.upper(), None)
        return meth

    def getBuilder(self, which):
        try:
            b = self.status.getBuilder(which)
        except KeyError:
            raise UsageError, "no such builder '%s'" % which
        return b

    def getControl(self, which):
        if not self.control:
            raise UsageError("builder control is not enabled")
        try:
            bc = self.control.getBuilder(which)
        except KeyError:
            raise UsageError("no such builder '%s'" % which)
        return bc

    def getAllBuilders(self):
        """
        @rtype: list of L{buildbot.process.builder.Builder}
        """
        names = self.status.getBuilderNames(categories=self.categories)
        names.sort()
        builders = [self.status.getBuilder(n) for n in names]
        return builders

    def convertTime(self, seconds):
        if seconds < 60:
            return "%d seconds" % seconds
        minutes = int(seconds / 60)
        seconds = seconds - 60*minutes
        if minutes < 60:
            return "%dm%02ds" % (minutes, seconds)
        hours = int(minutes / 60)
        minutes = minutes - 60*hours
        return "%dh%02dm%02ds" % (hours, minutes, seconds)

    def doSilly(self, user, reply, message):
        response = self.silly[message]
        if type(response) != type([]):
            response = [response]
        when = 0.5
        for r in response:
            reactor.callLater(when, self.reply, reply, r)
            when += 2.5

    def command_HELLO(self, user, reply, args):
        self.reply(reply, "yes?")

    def command_VERSION(self, user, reply, args):
        self.reply(reply, "buildbot-%s at your service" % version)

    def command_LIST(self, user, reply, args):
        args = args.split()
        if len(args) == 0:
            raise UsageError, "try 'list builders'"
        if args[0] == 'builders':
            builders = self.getAllBuilders()
            str = "Configured builders: "
            for b in builders:
                str += b.name
                state = b.getState()[0]
                if state == 'offline':
                    str += "[offline]"
                str += " "
            str.rstrip()
            self.reply(reply, str)
            return
    command_LIST.usage = "list builders - List configured builders"

    def command_STATUS(self, user, reply, args):
        args = args.split()
        if len(args) == 0:
            which = "all"
        elif len(args) == 1:
            which = args[0]
        else:
            raise UsageError, "try 'status <builder>'"
        if which == "all":
            builders = self.getAllBuilders()
            for b in builders:
                self.emit_status(reply, b.name)
            return
        self.emit_status(reply, which)
    command_STATUS.usage = "status [<which>] - List status of a builder (or all builders)"

    def command_WATCH(self, user, reply, args):
        args = args.split()
        if len(args) != 1:
            raise UsageError("try 'watch <builder>'")
        which = args[0]
        b = self.getBuilder(which)
        builds = b.getCurrentBuilds()
        if not builds:
            self.reply(reply, "there are no builds currently running")
            return
        for build in builds:
            assert not build.isFinished()
            d = build.waitUntilFinished()
            d.addCallback(self.buildFinished, reply)
            r = "watching build %s #%d until it finishes" \
                % (which, build.getNumber())
            eta = build.getETA()
            if eta is not None:
                r += " [%s]" % self.convertTime(eta)
            r += ".."
            self.reply(reply, r)
    command_WATCH.usage = "watch <which> - announce the completion of an active build"

    def buildFinished(self, b, reply):
        results = {SUCCESS: "Success",
                   WARNINGS: "Warnings",
                   FAILURE: "Failure",
                   EXCEPTION: "Exception",
                   }

        # only notify about builders we are interested in
        builder = b.getBuilder()
        log.msg('builder %r in category %s finished' % (builder,
                                                        builder.category))
        if (self.categories != None and
            builder.category not in self.categories):
            return

        r = "Hey! build %s #%d is complete: %s" % \
            (b.getBuilder().getName(),
             b.getNumber(),
             results.get(b.getResults(), "??"))
        r += " [%s]" % " ".join(b.getText())
        self.reply(reply, r)
        buildurl = self.status.getURLForThing(b)
        if buildurl:
            self.reply(reply, "Build details are at %s" % buildurl)

    def command_FORCE(self, user, reply, args):
        args = shlex.split(args) # TODO: this requires python2.3 or newer
        if args.pop(0) != "build":
            raise UsageError("try 'force build WHICH <REASON>'")
        opts = ForceOptions()
        opts.parseOptions(args)
        
        which = opts['builder']
        branch = opts['branch']
        revision = opts['revision']
        reason = opts['reason']

        # keep weird stuff out of the branch and revision strings. TODO:
        # centralize this somewhere.
        if branch and not re.match(r'^[\w\.\-\/]*$', branch):
            log.msg("bad branch '%s'" % branch)
            self.reply(reply, "sorry, bad branch '%s'" % branch)
            return
        if revision and not re.match(r'^[\w\.\-\/]*$', revision):
            log.msg("bad revision '%s'" % revision)
            self.reply(reply, "sorry, bad revision '%s'" % revision)
            return

        bc = self.getControl(which)

        who = None # TODO: if we can authenticate that a particular User
                   # asked for this, use User Name instead of None so they'll
                   # be informed of the results.
        # TODO: or, monitor this build and announce the results through the
        # 'reply' argument.
        r = "forced: by IRC user <%s>: %s" % (user, reason)
        # TODO: maybe give certain users the ability to request builds of
        # certain branches
        s = SourceStamp(branch=branch, revision=revision)
        req = BuildRequest(r, s, which)
        try:
            bc.requestBuildSoon(req)
        except interfaces.NoSlaveError:
            self.reply(reply,
                       "sorry, I can't force a build: all slaves are offline")
            return
        ireq = IrcBuildRequest(self, reply)
        req.subscribe(ireq.started)


    command_FORCE.usage = "force build <which> <reason> - Force a build"

    def command_STOP(self, user, reply, args):
        args = args.split(None, 2)
        if len(args) < 3 or args[0] != 'build':
            raise UsageError, "try 'stop build WHICH <REASON>'"
        which = args[1]
        reason = args[2]

        buildercontrol = self.getControl(which)

        who = None
        r = "stopped: by IRC user <%s>: %s" % (user, reason)

        # find an in-progress build
        builderstatus = self.getBuilder(which)
        builds = builderstatus.getCurrentBuilds()
        if not builds:
            self.reply(reply, "sorry, no build is currently running")
            return
        for build in builds:
            num = build.getNumber()

            # obtain the BuildControl object
            buildcontrol = buildercontrol.getBuild(num)

            # make it stop
            buildcontrol.stopBuild(r)

            self.reply(reply, "build %d interrupted" % num)

    command_STOP.usage = "stop build <which> <reason> - Stop a running build"

    def emit_status(self, reply, which):
        b = self.getBuilder(which)
        str = "%s: " % which
        state, builds = b.getState()
        str += state
        if state == "idle":
            last = b.getLastFinishedBuild()
            if last:
                start,finished = last.getTimes()
                str += ", last build %s secs ago: %s" % \
                       (int(util.now() - finished), " ".join(last.getText()))
        if state == "building":
            t = []
            for build in builds:
                step = build.getCurrentStep()
                s = "(%s)" % " ".join(step.getText())
                ETA = build.getETA()
                if ETA is not None:
                    s += " [ETA %s]" % self.convertTime(ETA)
                t.append(s)
            str += ", ".join(t)
        self.reply(reply, str)

    def emit_last(self, reply, which):
        last = self.getBuilder(which).getLastFinishedBuild()
        if not last:
            str = "(no builds run since last restart)"
        else:
            start,finish = last.getTimes()
            str = "%s secs ago: " % (int(util.now() - finish))
            str += " ".join(last.getText())
        self.reply(reply, "last build [%s]: %s" % (which, str))

    def command_LAST(self, user, reply, args):
        args = args.split()
        if len(args) == 0:
            which = "all"
        elif len(args) == 1:
            which = args[0]
        else:
            raise UsageError, "try 'last <builder>'"
        if which == "all":
            builders = self.getAllBuilders()
            for b in builders:
                self.emit_last(reply, b.name)
            return
        self.emit_last(reply, which)
    command_LAST.usage = "last <which> - list last build status for builder <which>"

    def build_commands(self):
        commands = []
        for k in self.__class__.__dict__.keys():
            if k.startswith('command_'):
                commands.append(k[8:].lower())
        commands.sort()
        return commands

    def command_HELP(self, user, reply, args):
        args = args.split()
        if len(args) == 0:
            self.reply(reply, "Get help on what? (try 'help <foo>', or 'commands' for a command list)")
            return
        command = args[0]
        meth = self.getCommandMethod(command)
        if not meth:
            raise UsageError, "no such command '%s'" % command
        usage = getattr(meth, 'usage', None)
        if usage:
            self.reply(reply, "Usage: %s" % usage)
        else:
            self.reply(reply, "No usage info for '%s'" % command)
    command_HELP.usage = "help <command> - Give help for <command>"

    def command_SOURCE(self, user, reply, args):
        banner = "My source can be found at http://buildbot.sourceforge.net/"
        self.reply(reply, banner)

    def command_COMMANDS(self, user, reply, args):
        commands = self.build_commands()
        str = "buildbot commands: " + ", ".join(commands)
        self.reply(reply, str)
    command_COMMANDS.usage = "commands - List available commands"

    def command_DESTROY(self, user, reply, args):
        self.me(reply, "readies phasers")

    def command_DANCE(self, user, reply, args):
        reactor.callLater(1.0, self.reply, reply, "0-<")
        reactor.callLater(3.0, self.reply, reply, "0-/")
        reactor.callLater(3.5, self.reply, reply, "0-\\")

    def command_EXCITED(self, user, reply, args):
        # like 'buildbot: destroy the sun!'
        self.reply(reply, "What you say!")

    def action(self, user, channel, data):
        #log.msg("action: %s,%s,%s" % (user, channel, data))
        user = user.split('!', 1)[0] # rest is ~user@hostname
        # somebody did an action (/me actions)
        if data.endswith("s buildbot"):
            words = data.split()
            verb = words[-2]
            timeout = 4
            if verb == "kicks":
                response = "%s back" % verb
                timeout = 1
            else:
                response = "%s %s too" % (verb, user)
            reactor.callLater(timeout, self.me, channel, response)
    # userJoined(self, user, channel)
    
    # output
    # self.say(channel, message) # broadcast
    # self.msg(user, message) # unicast
    # self.me(channel, action) # send action
    # self.away(message='')
    # self.quit(message='')
    
class ThrottledClientFactory(protocol.ClientFactory):
    lostDelay = 2
    failedDelay = 60
    def clientConnectionLost(self, connector, reason):
        reactor.callLater(self.lostDelay, connector.connect)
    def clientConnectionFailed(self, connector, reason):
        reactor.callLater(self.failedDelay, connector.connect)

class IrcStatusFactory(ThrottledClientFactory):
    protocol = IrcStatusBot

    status = None
    control = None
    shuttingDown = False
    p = None

    def __init__(self, nickname, password, channels, categories):
        #ThrottledClientFactory.__init__(self) # doesn't exist
        self.status = None
        self.nickname = nickname
        self.password = password
        self.channels = channels
        self.categories = categories

    def __getstate__(self):
        d = self.__dict__.copy()
        del d['p']
        return d

    def shutdown(self):
        self.shuttingDown = True
        if self.p:
            self.p.quit("buildmaster reconfigured: bot disconnecting")

    def buildProtocol(self, address):
        p = self.protocol(self.nickname, self.password,
                          self.channels, self.status,
                          self.categories)
        p.factory = self
        p.status = self.status
        p.control = self.control
        self.p = p
        return p

    # TODO: I think a shutdown that occurs while the connection is being
    # established will make this explode

    def clientConnectionLost(self, connector, reason):
        if self.shuttingDown:
            log.msg("not scheduling reconnection attempt")
            return
        ThrottledClientFactory.clientConnectionLost(self, connector, reason)

    def clientConnectionFailed(self, connector, reason):
        if self.shuttingDown:
            log.msg("not scheduling reconnection attempt")
            return
        ThrottledClientFactory.clientConnectionFailed(self, connector, reason)


class IRC(base.StatusReceiverMultiService):
    """I am an IRC bot which can be queried for status information. I
    connect to a single IRC server and am known by a single nickname on that
    server, however I can join multiple channels."""

    compare_attrs = ["host", "port", "nick", "password",
                     "channels", "allowForce",
                     "categories"]

    def __init__(self, host, nick, channels, port=6667, allowForce=True,
                 categories=None, password=None):
        base.StatusReceiverMultiService.__init__(self)

        assert allowForce in (True, False) # TODO: implement others

        # need to stash these so we can detect changes later
        self.host = host
        self.port = port
        self.nick = nick
        self.channels = channels
        self.password = password
        self.allowForce = allowForce
        self.categories = categories

        # need to stash the factory so we can give it the status object
        self.f = IrcStatusFactory(self.nick, self.password,
                                  self.channels, self.categories)

        c = internet.TCPClient(host, port, self.f)
        c.setServiceParent(self)

    def setServiceParent(self, parent):
        base.StatusReceiverMultiService.setServiceParent(self, parent)
        self.f.status = parent.getStatus()
        if self.allowForce:
            self.f.control = interfaces.IControl(parent)

    def stopService(self):
        # make sure the factory will stop reconnecting
        self.f.shutdown()
        return base.StatusReceiverMultiService.stopService(self)


def main():
    from twisted.internet import app
    a = app.Application("irctest")
    f = IrcStatusFactory()
    host = "localhost"
    port = 6667
    f.addNetwork((host, port), ["private", "other"])
    a.connectTCP(host, port, f)
    a.run(save=0)
    

if __name__ == '__main__':
    main()

## buildbot: list builders
# buildbot: watch quick
#  print notification when current build in 'quick' finishes
## buildbot: status
## buildbot: status full-2.3
##  building, not, % complete, ETA
## buildbot: force build full-2.3 "reason"
