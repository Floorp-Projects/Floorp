# -*- test-case-name: buildbot.test.test_web -*-

from __future__ import generators

from twisted.python import log, components
import urllib, re

from twisted.internet import defer, reactor
from twisted.web.resource import Resource
from twisted.web import static, html, server, distrib
from twisted.web.error import NoResource
from twisted.web.util import Redirect, DeferredResource
from twisted.application import strports
from twisted.spread import pb

from buildbot.twcompat import implements, Interface

import sys, string, types, time, os.path

from buildbot import interfaces, util
from buildbot import version
from buildbot.sourcestamp import SourceStamp
from buildbot.status import builder, base
from buildbot.changes import changes
from buildbot.process.base import BuildRequest

class ITopBox(Interface):
    """I represent a box in the top row of the waterfall display: the one
    which shows the status of the last build for each builder."""
    pass

class ICurrentBox(Interface):
    """I represent the 'current activity' box, just above the builder name."""
    pass

class IBox(Interface):
    """I represent a box in the waterfall display."""
    pass

class IHTMLLog(Interface):
    pass

ROW_TEMPLATE = '''
<div class="row">
  <span class="label">%(label)s</span>
  <span class="field">%(field)s</span>
</div>'''

def make_row(label, field):
    """Create a name/value row for the HTML.

    `label` is plain text; it will be HTML-encoded.

    `field` is a bit of HTML structure; it will not be encoded in
    any way.
    """
    label = html.escape(label)
    return ROW_TEMPLATE % {"label": label, "field": field}

colormap = {
    'green': '#72ff75',
    }
def td(text="", parms={}, **props):
    data = ""
    data += "  "
    #if not props.has_key("border"):
    #    props["border"] = 1
    props.update(parms)
    if props.has_key("bgcolor"):
        props["bgcolor"] = colormap.get(props["bgcolor"], props["bgcolor"])
    comment = props.get("comment", None)
    if comment:
        data += "<!-- %s -->" % comment
    data += "<td"
    class_ = props.get('class_', None)
    if class_:
        props["class"] = class_
    for prop in ("align", "bgcolor", "colspan", "rowspan", "border",
                 "valign", "halign", "class"):
        p = props.get(prop, None)
        if p != None:
            data += " %s=\"%s\"" % (prop, p)
    data += ">"
    if not text:
        text = "&nbsp;"
    if type(text) == types.ListType:
        data += string.join(text, "<br />")
    else:
        data += text
    data += "</td>\n"
    return data

def build_get_class(b):
    """
    Return the class to use for a finished build or buildstep,
    based on the result.
    """
    # FIXME: this getResults duplicity might need to be fixed
    result = b.getResults()
    #print "THOMAS: result for b %r: %r" % (b, result)
    if isinstance(b, builder.BuildStatus):
        result = b.getResults()
    elif isinstance(b, builder.BuildStepStatus):
        result = b.getResults()[0]
        # after forcing a build, b.getResults() returns ((None, []), []), ugh
        if isinstance(result, tuple):
            result = result[0]
    else:
        raise TypeError, "%r is not a BuildStatus or BuildStepStatus" % b

    if result == None:
        # FIXME: this happens when a buildstep is running ?
        return "running"
    return builder.Results[result]

class Box:
    # a Box wraps an Event. The Box has HTML <td> parameters that Events
    # lack, and it has a base URL to which each File's name is relative.
    # Events don't know about HTML.
    spacer = False
    def __init__(self, text=[], color=None, class_=None, urlbase=None,
                 **parms):
        self.text = text
        self.color = color
        self.class_ = class_
        self.urlbase = urlbase
        self.show_idle = 0
        if parms.has_key('show_idle'):
            del parms['show_idle']
            self.show_idle = 1
            
        self.parms = parms
        # parms is a dict of HTML parameters for the <td> element that will
        # represent this Event in the waterfall display.

    def td(self, **props):
        props.update(self.parms)
        text = self.text
        if not text and self.show_idle:
            text = ["[idle]"]
        return td(text, props, bgcolor=self.color, class_=self.class_)


class HtmlResource(Resource):
    css = None
    contentType = "text/html; charset=UTF-8"
    title = "Dummy"

    def render(self, request):
        data = self.content(request)
        if isinstance(data, unicode):
            data = data.encode("utf-8")
        request.setHeader("content-type", self.contentType)
        if request.method == "HEAD":
            request.setHeader("content-length", len(data))
            return ''
        return data

    def content(self, request):
        data = ('<!DOCTYPE html PUBLIC'
                ' "-//W3C//DTD XHTML 1.0 Transitional//EN"\n'
                '"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">\n'
                '<html'
                ' xmlns="http://www.w3.org/1999/xhtml"'
                ' lang="en"'
                ' xml:lang="en">\n')
        data += "<head>\n"
        data += "  <title>" + self.title + "</title>\n"
        if self.css:
            # TODO: use some sort of relative link up to the root page, so
            # this css can be used from child pages too
            data += ('  <link href="%s" rel="stylesheet" type="text/css"/>\n'
                     % "buildbot.css")
        data += "</head>\n"
        data += '<body vlink="#800080">\n'
        data += self.body(request)
        data += "</body></html>\n"
        return data

    def body(self, request):
        return "Dummy\n"

class StaticHTML(HtmlResource):
    def __init__(self, body, title):
        HtmlResource.__init__(self)
        self.bodyHTML = body
        self.title = title
    def body(self, request):
        return self.bodyHTML

# $builder/builds/NN/stepname
class StatusResourceBuildStep(HtmlResource):
    title = "Build Step"

    def __init__(self, status, step):
        HtmlResource.__init__(self)
        self.status = status
        self.step = step

    def body(self, request):
        s = self.step
        b = s.getBuild()
        data = "<h1>BuildStep %s:#%d:%s</h1>\n" % \
               (b.getBuilder().getName(), b.getNumber(), s.getName())

        if s.isFinished():
            data += ("<h2>Finished</h2>\n"
                     "<p>%s</p>\n" % html.escape("%s" % s.getText()))
        else:
            data += ("<h2>Not Finished</h2>\n"
                     "<p>ETA %s seconds</p>\n" % s.getETA())

        exp = s.getExpectations()
        if exp:
            data += ("<h2>Expectations</h2>\n"
                     "<ul>\n")
            for e in exp:
                data += "<li>%s: current=%s, target=%s</li>\n" % \
                        (html.escape(e[0]), e[1], e[2])
            data += "</ul>\n"
        logs = s.getLogs()
        if logs:
            data += ("<h2>Logs</h2>\n"
                     "<ul>\n")
            for num in range(len(logs)):
                if logs[num].hasContents():
                    # FIXME: If the step name has a / in it, this is broken
                    # either way.  If we quote it but say '/'s are safe,
                    # it chops up the step name.  If we quote it and '/'s
                    # are not safe, it escapes the / that separates the
                    # step name from the log number.
                    data += '<li><a href="%s">%s</a></li>\n' % \
                            (urllib.quote(request.childLink("%d" % num)),
                             html.escape(logs[num].getName()))
                else:
                    data += ('<li>%s</li>\n' %
                             html.escape(logs[num].getName()))
            data += "</ul>\n"

        return data

    def getChild(self, path, request):
        logname = path
        try:
            log = self.step.getLogs()[int(logname)]
            if log.hasContents():
                return IHTMLLog(interfaces.IStatusLog(log))
            return NoResource("Empty Log '%s'" % logname)
        except (IndexError, ValueError):
            return NoResource("No such Log '%s'" % logname)

# $builder/builds/NN/tests/TESTNAME
class StatusResourceTestResult(HtmlResource):
    title = "Test Logs"

    def __init__(self, status, name, result):
        HtmlResource.__init__(self)
        self.status = status
        self.name = name
        self.result = result

    def body(self, request):
        dotname = ".".join(self.name)
        logs = self.result.getLogs()
        lognames = logs.keys()
        lognames.sort()
        data = "<h1>%s</h1>\n" % html.escape(dotname)
        for name in lognames:
            data += "<h2>%s</h2>\n" % html.escape(name)
            data += "<pre>" + logs[name] + "</pre>\n\n"

        return data


# $builder/builds/NN/tests
class StatusResourceTestResults(HtmlResource):
    title = "Test Results"

    def __init__(self, status, results):
        HtmlResource.__init__(self)
        self.status = status
        self.results = results

    def body(self, request):
        r = self.results
        data = "<h1>Test Results</h1>\n"
        data += "<ul>\n"
        testnames = r.keys()
        testnames.sort()
        for name in testnames:
            res = r[name]
            dotname = ".".join(name)
            data += " <li>%s: " % dotname
            # TODO: this could break on weird test names. At the moment,
            # test names only come from Trial tests, where the name
            # components must be legal python names, but that won't always
            # be a restriction.
            url = request.childLink(dotname)
            data += "<a href=\"%s\">%s</a>" % (url, " ".join(res.getText()))
            data += "</li>\n"
        data += "</ul>\n"
        return data

    def getChild(self, path, request):
        try:
            name = tuple(path.split("."))
            result = self.results[name]
            return StatusResourceTestResult(self.status, name, result)
        except KeyError:
            return NoResource("No such test name '%s'" % path)


# $builder/builds/NN
class StatusResourceBuild(HtmlResource):
    title = "Build"

    def __init__(self, status, build, builderControl, buildControl):
        HtmlResource.__init__(self)
        self.status = status
        self.build = build
        self.builderControl = builderControl
        self.control = buildControl

    def body(self, request):
        b = self.build
        buildbotURL = self.status.getBuildbotURL()
        projectName = self.status.getProjectName()
        data = '<div class="title"><a href="%s">%s</a></div>\n'%(buildbotURL,
                                                                 projectName)
        # the color in the following line gives python-mode trouble
        data += ("<h1>Build <a href=\"%s\">%s</a>:#%d</h1>\n"
                 % (self.status.getURLForThing(b.getBuilder()),
                    b.getBuilder().getName(), b.getNumber()))
        data += "<h2>Buildslave:</h2>\n %s\n" % html.escape(b.getSlavename())
        data += "<h2>Reason:</h2>\n%s\n" % html.escape(b.getReason())

        branch, revision, patch = b.getSourceStamp()
        data += "<h2>SourceStamp:</h2>\n"
        data += " <ul>\n"
        if branch:
            data += "  <li>Branch: %s</li>\n" % html.escape(branch)
        if revision:
            data += "  <li>Revision: %s</li>\n" % html.escape(str(revision))
        if patch:
            data += "  <li>Patch: YES</li>\n" # TODO: provide link to .diff
        if b.getChanges():
            data += "  <li>Changes: see below</li>\n"
        if (branch is None and revision is None and patch is None
            and not b.getChanges()):
            data += "  <li>build of most recent revision</li>\n"
        data += " </ul>\n"
        if b.isFinished():
            data += "<h2>Results:</h2>\n"
            data += " ".join(b.getText()) + "\n"
            if b.getTestResults():
                url = request.childLink("tests")
                data += "<h3><a href=\"%s\">test results</a></h3>\n" % url
        else:
            data += "<h2>Build In Progress</h2>"
            if self.control is not None:
                stopURL = urllib.quote(request.childLink("stop"))
                data += """
                <form action="%s" class='command stopbuild'>
                <p>To stop this build, fill out the following fields and
                push the 'Stop' button</p>\n""" % stopURL
                data += make_row("Your name:",
                                 "<input type='text' name='username' />")
                data += make_row("Reason for stopping build:",
                                 "<input type='text' name='comments' />")
                data += """<input type="submit" value="Stop Builder" />
                </form>
                """

        if b.isFinished() and self.builderControl is not None:
            data += "<h3>Resubmit Build:</h3>\n"
            # can we rebuild it exactly?
            exactly = (revision is not None) or b.getChanges()
            if exactly:
                data += ("<p>This tree was built from a specific set of \n"
                         "source files, and can be rebuilt exactly</p>\n")
            else:
                data += ("<p>This tree was built from the most recent "
                         "revision")
                if branch:
                    data += " (along some branch)"
                data += (" and thus it might not be possible to rebuild it \n"
                         "exactly. Any changes that have been committed \n"
                         "after this build was started <b>will</b> be \n"
                         "included in a rebuild.</p>\n")
            rebuildURL = urllib.quote(request.childLink("rebuild"))
            data += ('<form action="%s" class="command rebuild">\n'
                     % rebuildURL)
            data += make_row("Your name:",
                             "<input type='text' name='username' />")
            data += make_row("Reason for re-running build:",
                             "<input type='text' name='comments' />")
            data += '<input type="submit" value="Rebuild" />\n'

        data += "<h2>Steps and Logfiles:</h2>\n"
        if b.getLogs():
            data += "<ol>\n"
            for s in b.getSteps():
                data += (" <li><a href=\"%s\">%s</a> [%s]\n"
                         % (self.status.getURLForThing(s), s.getName(),
                            " ".join(s.getText())))
                if s.getLogs():
                    data += "  <ol>\n"
                    for logfile in s.getLogs():
                        data += ("   <li><a href=\"%s\">%s</a></li>\n" %
                                 (self.status.getURLForThing(logfile),
                                  logfile.getName()))
                    data += "  </ol>\n"
                data += " </li>\n"
            data += "</ol>\n"

        data += ("<h2>Blamelist:</h2>\n"
                 " <ol>\n")
        for who in b.getResponsibleUsers():
            data += "  <li>%s</li>\n" % html.escape(who)
        data += (" </ol>\n"
                 "<h2>All Changes</h2>\n")
        changes = b.getChanges()
        if changes:
            data += "<ol>\n"
            for c in changes:
                data += "<li>" + c.asHTML() + "</li>\n"
            data += "</ol>\n"
        #data += html.PRE(b.changesText()) # TODO
        return data

    def stop(self, request):
        log.msg("web stopBuild of build %s:%s" % \
                (self.build.getBuilder().getName(),
                 self.build.getNumber()))
        name = request.args.get("username", ["<unknown>"])[0]
        comments = request.args.get("comments", ["<no reason specified>"])[0]
        reason = ("The web-page 'stop build' button was pressed by "
                  "'%s': %s\n" % (name, comments))
        self.control.stopBuild(reason)
        # we're at http://localhost:8080/svn-hello/builds/5/stop?[args] and
        # we want to go to: http://localhost:8080/svn-hello/builds/5 or
        # http://localhost:8080/
        #
        #return Redirect("../%d" % self.build.getNumber())
        r = Redirect("../../..")
        d = defer.Deferred()
        reactor.callLater(1, d.callback, r)
        return DeferredResource(d)

    def rebuild(self, request):
        log.msg("web rebuild of build %s:%s" % \
                (self.build.getBuilder().getName(),
                 self.build.getNumber()))
        name = request.args.get("username", ["<unknown>"])[0]
        comments = request.args.get("comments", ["<no reason specified>"])[0]
        reason = ("The web-page 'rebuild' button was pressed by "
                  "'%s': %s\n" % (name, comments))
        if not self.builderControl or not self.build.isFinished():
            log.msg("could not rebuild: bc=%s, isFinished=%s"
                    % (self.builderControl, self.build.isFinished()))
            # TODO: indicate an error
        else:
            self.builderControl.resubmitBuild(self.build, reason)
        # we're at http://localhost:8080/svn-hello/builds/5/rebuild?[args] and
        # we want to go to the top, at http://localhost:8080/
        r = Redirect("../../..")
        d = defer.Deferred()
        reactor.callLater(1, d.callback, r)
        return DeferredResource(d)

    def getChild(self, path, request):
        if path == "tests":
            return StatusResourceTestResults(self.status,
                                             self.build.getTestResults())
        if path == "stop":
            return self.stop(request)
        if path == "rebuild":
            return self.rebuild(request)
        if path.startswith("step-"):
            stepname = path[len("step-"):]
            steps = self.build.getSteps()
            for s in steps:
                if s.getName() == stepname:
                    return StatusResourceBuildStep(self.status, s)
            return NoResource("No such BuildStep '%s'" % stepname)
        return NoResource("No such resource '%s'" % path)

# $builder
class StatusResourceBuilder(HtmlResource):

    def __init__(self, status, builder, control):
        HtmlResource.__init__(self)
        self.status = status
        self.title = builder.getName() + " Builder"
        self.builder = builder
        self.control = control

    def body(self, request):
        b = self.builder
        slaves = b.getSlaves()
        connected_slaves = [s for s in slaves if s.isConnected()]

        buildbotURL = self.status.getBuildbotURL()
        projectName = self.status.getProjectName()
        data = "<a href=\"%s\">%s</a>\n" % (buildbotURL, projectName)
        data += make_row("Builder:", html.escape(b.getName()))
        b1 = b.getBuild(-1)
        if b1 is not None:
            data += make_row("Current/last build:", str(b1.getNumber()))
        data += "\n<br />BUILDSLAVES<br />\n"
        data += "<ol>\n"
        for slave in slaves:
            data += "<li><b>%s</b>: " % html.escape(slave.getName())
            if slave.isConnected():
                data += "CONNECTED\n"
                if slave.getAdmin():
                    data += make_row("Admin:", html.escape(slave.getAdmin()))
                if slave.getHost():
                    data += "<span class='label'>Host info:</span>\n"
                    data += html.PRE(slave.getHost())
            else:
                data += ("NOT CONNECTED\n")
            data += "</li>\n"
        data += "</ol>\n"

        if self.control is not None and connected_slaves:
            forceURL = urllib.quote(request.childLink("force"))
            data += (
                """
                <form action='%(forceURL)s' class='command forcebuild'>
                <p>To force a build, fill out the following fields and
                push the 'Force Build' button</p>"""
                + make_row("Your name:",
                           "<input type='text' name='username' />")
                + make_row("Reason for build:",
                           "<input type='text' name='comments' />")
                + make_row("Branch to build:",
                           "<input type='text' name='branch' />")
                + make_row("Revision to build:",
                           "<input type='text' name='revision' />")
                + """
                <input type='submit' value='Force Build' />
                </form>
                """) % {"forceURL": forceURL}
        elif self.control is not None:
            data += """
            <p>All buildslaves appear to be offline, so it's not possible
            to force this build to execute at this time.</p>
            """

        if self.control is not None:
            pingURL = urllib.quote(request.childLink("ping"))
            data += """
            <form action="%s" class='command pingbuilder'>
            <p>To ping the buildslave(s), push the 'Ping' button</p>

            <input type="submit" value="Ping Builder" />
            </form>
            """ % pingURL

        return data

    def force(self, request):
        name = request.args.get("username", ["<unknown>"])[0]
        reason = request.args.get("comments", ["<no reason specified>"])[0]
        branch = request.args.get("branch", [""])[0]
        revision = request.args.get("revision", [""])[0]

        r = "The web-page 'force build' button was pressed by '%s': %s\n" \
            % (name, reason)
        log.msg("web forcebuild of builder '%s', branch='%s', revision='%s'"
                % (self.builder.name, branch, revision))

        if not self.control:
            # TODO: tell the web user that their request was denied
            log.msg("but builder control is disabled")
            return Redirect("..")

        # keep weird stuff out of the branch and revision strings. TODO:
        # centralize this somewhere.
        if not re.match(r'^[\w\.\-\/]*$', branch):
            log.msg("bad branch '%s'" % branch)
            return Redirect("..")
        if not re.match(r'^[\w\.\-\/]*$', revision):
            log.msg("bad revision '%s'" % revision)
            return Redirect("..")
        if branch == "":
            branch = None
        if revision == "":
            revision = None

        # TODO: if we can authenticate that a particular User pushed the
        # button, use their name instead of None, so they'll be informed of
        # the results.
        s = SourceStamp(branch=branch, revision=revision)
        req = BuildRequest(r, s, self.builder.getName())
        try:
            self.control.requestBuildSoon(req)
        except interfaces.NoSlaveError:
            # TODO: tell the web user that their request could not be
            # honored
            pass
        return Redirect("..")

    def ping(self, request):
        log.msg("web ping of builder '%s'" % self.builder.name)
        self.control.ping() # TODO: there ought to be an ISlaveControl
        return Redirect("..")

    def getChild(self, path, request):
        if path == "force":
            return self.force(request)
        if path == "ping":
            return self.ping(request)
        if not path in ("events", "builds"):
            return NoResource("Bad URL '%s'" % path)
        num = request.postpath.pop(0)
        request.prepath.append(num)
        num = int(num)
        if path == "events":
            # TODO: is this dead code? .statusbag doesn't exist,right?
            log.msg("getChild['path']: %s" % request.uri)
            return NoResource("events are unavailable until code gets fixed")
            filename = request.postpath.pop(0)
            request.prepath.append(filename)
            e = self.builder.statusbag.getEventNumbered(num)
            if not e:
                return NoResource("No such event '%d'" % num)
            file = e.files.get(filename, None)
            if file == None:
                return NoResource("No such file '%s'" % filename)
            if type(file) == type(""):
                if file[:6] in ("<HTML>", "<html>"):
                    return static.Data(file, "text/html")
                return static.Data(file, "text/plain")
            return file
        if path == "builds":
            build = self.builder.getBuild(num)
            if build:
                control = None
                if self.control:
                    control = self.control.getBuild(num)
                return StatusResourceBuild(self.status, build,
                                           self.control, control)
            else:
                return NoResource("No such build '%d'" % num)
        return NoResource("really weird URL %s" % path)

# $changes/NN
class StatusResourceChanges(HtmlResource):
    def __init__(self, status, changemaster):
        HtmlResource.__init__(self)
        self.status = status
        self.changemaster = changemaster
    def body(self, request):
        data = ""
        data += "Change sources:\n"
        sources = list(self.changemaster)
        if sources:
            data += "<ol>\n"
            for s in sources:
                data += "<li>%s</li>\n" % s.describe()
            data += "</ol>\n"
        else:
            data += "none (push only)\n"
        return data
    def getChild(self, path, request):
        num = int(path)
        c = self.changemaster.getChangeNumbered(num)
        if not c:
            return NoResource("No change number '%d'" % num)
        return StaticHTML(c.asHTML(), "Change #%d" % num)

textlog_stylesheet = """
<style type="text/css">
 div.data {
  font-family: "Courier New", courier, monotype;
 }
 span.stdout {
  font-family: "Courier New", courier, monotype;
 }
 span.stderr {
  font-family: "Courier New", courier, monotype;
  color: red;
 }
 span.header {
  font-family: "Courier New", courier, monotype;
  color: blue;
 }
</style>
"""

class ChunkConsumer:
    if implements:
        implements(interfaces.IStatusLogConsumer)
    else:
        __implements__ = interfaces.IStatusLogConsumer,

    def __init__(self, original, textlog):
        self.original = original
        self.textlog = textlog
    def registerProducer(self, producer, streaming):
        self.producer = producer
        self.original.registerProducer(producer, streaming)
    def unregisterProducer(self):
        self.original.unregisterProducer()
    def writeChunk(self, chunk):
        formatted = self.textlog.content([chunk])
        try:
            self.original.write(formatted)
        except pb.DeadReferenceError:
            self.producing.stopProducing()
    def finish(self):
        self.textlog.finished()

class TextLog(Resource):
    # a new instance of this Resource is created for each client who views
    # it, so we can afford to track the request in the Resource.
    if implements:
        implements(IHTMLLog)
    else:
        __implements__ = IHTMLLog,

    asText = False
    subscribed = False

    def __init__(self, original):
        Resource.__init__(self)
        self.original = original

    def getChild(self, path, request):
        if path == "text":
            self.asText = True
            return self
        return NoResource("bad pathname")

    def htmlHeader(self, request):
        title = "Log File contents"
        data = "<html>\n<head><title>" + title + "</title>\n"
        data += textlog_stylesheet
        data += "</head>\n"
        data += "<body vlink=\"#800080\">\n"
        texturl = request.childLink("text")
        data += '<a href="%s">(view as text)</a><br />\n' % texturl
        data += "<pre>\n"
        return data

    def content(self, entries):
        spanfmt = '<span class="%s">%s</span>'
        data = ""
        for type, entry in entries:
            if self.asText:
                if type != builder.HEADER:
                    data += entry
            else:
                data += spanfmt % (builder.ChunkTypes[type],
                                   html.escape(entry))
        return data

    def htmlFooter(self):
        data = "</pre>\n"
        data += "</body></html>\n"
        return data

    def render_HEAD(self, request):
        if self.asText:
            request.setHeader("content-type", "text/plain")
        else:
            request.setHeader("content-type", "text/html")

        # vague approximation, ignores markup
        request.setHeader("content-length", self.original.length)
        return ''

    def render_GET(self, req):
        self.req = req

        if self.asText:
            req.setHeader("content-type", "text/plain")
        else:
            req.setHeader("content-type", "text/html")

        if not self.asText:
            req.write(self.htmlHeader(req))

        self.original.subscribeConsumer(ChunkConsumer(req, self))
        return server.NOT_DONE_YET

    def finished(self):
        if not self.req:
            return
        try:
            if not self.asText:
                self.req.write(self.htmlFooter())
            self.req.finish()
        except pb.DeadReferenceError:
            pass
        # break the cycle, the Request's .notifications list includes the
        # Deferred (from req.notifyFinish) that's pointing at us.
        self.req = None

components.registerAdapter(TextLog, interfaces.IStatusLog, IHTMLLog)


class HTMLLog(Resource):
    if implements:
        implements(IHTMLLog)
    else:
        __implements__ = IHTMLLog,


    def __init__(self, original):
        Resource.__init__(self)
        self.original = original

    def render(self, request):
        request.setHeader("content-type", "text/html")
        return self.original.html

components.registerAdapter(HTMLLog, builder.HTMLLogFile, IHTMLLog)


class CurrentBox(components.Adapter):
    # this provides the "current activity" box, just above the builder name
    if implements:
        implements(ICurrentBox)
    else:
        __implements__ = ICurrentBox,

    def formatETA(self, eta):
        if eta is None:
            return []
        if eta < 0:
            return ["Soon"]
        abstime = time.strftime("%H:%M:%S", time.localtime(util.now()+eta))
        return ["ETA in", "%d secs" % eta, "at %s" % abstime]

    def getBox(self, status):
        # getState() returns offline, idle, or building
        state, builds = self.original.getState()

        # look for upcoming builds. We say the state is "waiting" if the
        # builder is otherwise idle and there is a scheduler which tells us a
        # build will be performed some time in the near future. TODO: this
        # functionality used to be in BuilderStatus.. maybe this code should
        # be merged back into it.
        upcoming = []
        builderName = self.original.getName()
        for s in status.getSchedulers():
            if builderName in s.listBuilderNames():
                upcoming.extend(s.getPendingBuildTimes())
        if state == "idle" and upcoming:
            state = "waiting"

        if state == "building":
            color = "yellow"
            text = ["building"]
            if builds:
                for b in builds:
                    eta = b.getETA()
                    if eta:
                        text.extend(self.formatETA(eta))
        elif state == "offline":
            color = "red"
            text = ["offline"]
        elif state == "idle":
            color = "white"
            text = ["idle"]
        elif state == "waiting":
            color = "yellow"
            text = ["waiting"]
        else:
            # just in case I add a state and forget to update this
            color = "white"
            text = [state]

        # TODO: for now, this pending/upcoming stuff is in the "current
        # activity" box, but really it should go into a "next activity" row
        # instead. The only times it should show up in "current activity" is
        # when the builder is otherwise idle.

        # are any builds pending? (waiting for a slave to be free)
        pbs = self.original.getPendingBuilds()
        if pbs:
            text.append("%d pending" % len(pbs))
        for t in upcoming:
            text.extend(["next at", 
                         time.strftime("%H:%M:%S", time.localtime(t)),
                         "[%d secs]" % (t - util.now()),
                         ])
            # TODO: the upcoming-builds box looks like:
            #  ['waiting', 'next at', '22:14:15', '[86 secs]']
            # while the currently-building box is reversed:
            #  ['building', 'ETA in', '2 secs', 'at 22:12:50']
            # consider swapping one of these to make them look the same. also
            # consider leaving them reversed to make them look different.
        return Box(text, color=color, class_="Activity " + state)

components.registerAdapter(CurrentBox, builder.BuilderStatus, ICurrentBox)

class ChangeBox(components.Adapter):
    if implements:
        implements(IBox)
    else:
        __implements__ = IBox,

    def getBox(self):
        url = "changes/%d" % self.original.number
        text = '<a href="%s">%s</a>' % (url, html.escape(self.original.who))
        return Box([text], color="white", class_="Change")
components.registerAdapter(ChangeBox, changes.Change, IBox)

class BuildBox(components.Adapter):
    # this provides the yellow "starting line" box for each build
    if implements:
        implements(IBox)
    else:
        __implements__ = IBox,

    def getBox(self):
        b = self.original
        name = b.getBuilder().getName()
        number = b.getNumber()
        url = "%s/builds/%d" % (urllib.quote(name, safe=''), number)
        reason = b.getReason()
        text = ('<a title="Reason: %s" href="%s">Build %d</a>'
                % (html.escape(reason), url, number))
        color = "yellow"
        class_ = "start"
        if b.isFinished() and not b.getSteps():
            # the steps have been pruned, so there won't be any indication
            # of whether it succeeded or failed. Color the box red or green
            # to show its status
            color = b.getColor()
            class_ = build_get_class(b)
        return Box([text], color=color, class_="BuildStep " + class_)
components.registerAdapter(BuildBox, builder.BuildStatus, IBox)

class StepBox(components.Adapter):
    if implements:
        implements(IBox)
    else:
        __implements__ = IBox,

    def getBox(self):
        b = self.original.getBuild()
        urlbase = "%s/builds/%d/step-%s" % (
            urllib.quote(b.getBuilder().getName(), safe=''),
            b.getNumber(),
            urllib.quote(self.original.getName(), safe=''))
        text = self.original.getText()
        if text is None:
            log.msg("getText() gave None", urlbase)
            text = []
        text = text[:]
        logs = self.original.getLogs()
        for num in range(len(logs)):
            name = logs[num].getName()
            if logs[num].hasContents():
                url = "%s/%d" % (urlbase, num)
                text.append("<a href=\"%s\">%s</a>" % (url, html.escape(name)))
            else:
                text.append(html.escape(name))
        urls = self.original.getURLs()
        ex_url_class = "BuildStep external"
        for name, target in urls.items():
            text.append('[<a href="%s" class="%s">%s</a>]' %
                        (target, ex_url_class, html.escape(name)))
        color = self.original.getColor()
        class_ = "BuildStep " + build_get_class(self.original)
        return Box(text, color, class_=class_)
components.registerAdapter(StepBox, builder.BuildStepStatus, IBox)

class EventBox(components.Adapter):
    if implements:
        implements(IBox)
    else:
        __implements__ = IBox,

    def getBox(self):
        text = self.original.getText()
        color = self.original.getColor()
        class_ = "Event"
        if color:
            class_ += " " + color
        return Box(text, color, class_=class_)
components.registerAdapter(EventBox, builder.Event, IBox)
        

class BuildTopBox(components.Adapter):
    # this provides a per-builder box at the very top of the display,
    # showing the results of the most recent build
    if implements:
        implements(IBox)
    else:
        __implements__ = IBox,

    def getBox(self):
        assert interfaces.IBuilderStatus(self.original)
        b = self.original.getLastFinishedBuild()
        if not b:
            return Box(["none"], "white", class_="LastBuild")
        name = b.getBuilder().getName()
        number = b.getNumber()
        url = "%s/builds/%d" % (name, number)
        text = b.getText()
        # TODO: add logs?
        # TODO: add link to the per-build page at 'url'
        c = b.getColor()
        class_ = build_get_class(b)
        return Box(text, c, class_="LastBuild %s" % class_)
components.registerAdapter(BuildTopBox, builder.BuilderStatus, ITopBox)

class Spacer(builder.Event):
    def __init__(self, start, finish):
        self.started = start
        self.finished = finish

class SpacerBox(components.Adapter):
    if implements:
        implements(IBox)
    else:
        __implements__ = IBox,

    def getBox(self):
        #b = Box(["spacer"], "white")
        b = Box([])
        b.spacer = True
        return b
components.registerAdapter(SpacerBox, Spacer, IBox)
    
def insertGaps(g, lastEventTime, idleGap=2):
    debug = False

    e = g.next()
    starts, finishes = e.getTimes()
    if debug: log.msg("E0", starts, finishes)
    if finishes == 0:
        finishes = starts
    if debug: log.msg("E1 finishes=%s, gap=%s, lET=%s" % \
                      (finishes, idleGap, lastEventTime))
    if finishes is not None and finishes + idleGap < lastEventTime:
        if debug: log.msg(" spacer0")
        yield Spacer(finishes, lastEventTime)

    followingEventStarts = starts
    if debug: log.msg(" fES0", starts)
    yield e

    while 1:
        e = g.next()
        starts, finishes = e.getTimes()
        if debug: log.msg("E2", starts, finishes)
        if finishes == 0:
            finishes = starts
        if finishes is not None and finishes + idleGap < followingEventStarts:
            # there is a gap between the end of this event and the beginning
            # of the next one. Insert an idle event so the waterfall display
            # shows a gap here.
            if debug:
                log.msg(" finishes=%s, gap=%s, fES=%s" % \
                        (finishes, idleGap, followingEventStarts))
            yield Spacer(finishes, followingEventStarts)
        yield e
        followingEventStarts = starts
        if debug: log.msg(" fES1", starts)


class WaterfallStatusResource(HtmlResource):
    """This builds the main status page, with the waterfall display, and
    all child pages."""
    title = "BuildBot"
    def __init__(self, status, changemaster, categories, css=None):
        HtmlResource.__init__(self)
        self.status = status
        self.changemaster = changemaster
        self.categories = categories
        p = self.status.getProjectName()
        if p:
            self.title = "BuildBot: %s" % p
        self.css = css

    def body(self, request):
        "This method builds the main waterfall display."

        data = ''

        projectName = self.status.getProjectName()
        projectURL = self.status.getProjectURL()

        phase = request.args.get("phase",["2"])
        phase = int(phase[0])

        showBuilders = request.args.get("show", None)
        allBuilders = self.status.getBuilderNames(categories=self.categories)
        if showBuilders:
            builderNames = []
            for b in showBuilders:
                if b not in allBuilders:
                    continue
                if b in builderNames:
                    continue
                builderNames.append(b)
        else:
            builderNames = allBuilders
        builders = map(lambda name: self.status.getBuilder(name),
                       builderNames)

        if phase == -1:
            return self.body0(request, builders)
        (changeNames, builderNames, timestamps, eventGrid, sourceEvents) = \
                      self.buildGrid(request, builders)
        if phase == 0:
            return self.phase0(request, (changeNames + builderNames),
                               timestamps, eventGrid)
        # start the table: top-header material
        data += '<table border="0" cellspacing="0">\n'

        if projectName and projectURL:
            # TODO: this is going to look really ugly
            topleft = "<a href=\"%s\">%s</a><br />last build" % \
                      (projectURL, projectName)
        else:
            topleft = "last build"
        data += ' <tr class="LastBuild">\n'
        data += td(topleft, align="right", colspan=2, class_="Project")
        for b in builders:
            box = ITopBox(b).getBox()
            data += box.td(align="center")
        data += " </tr>\n"

        data += ' <tr class="Activity">\n'
        data += td('current activity', align='right', colspan=2)
        for b in builders:
            box = ICurrentBox(b).getBox(self.status)
            data += box.td(align="center")
        data += " </tr>\n"
        
        data += " <tr>\n"
        TZ = time.tzname[time.daylight]
        data += td("time (%s)" % TZ, align="center", class_="Time")
        name = changeNames[0]
        data += td(
                "<a href=\"%s\">%s</a>" % (urllib.quote(name, safe=''), name),
                align="center", class_="Change")
        for name in builderNames:
            data += td(
                #"<a href=\"%s\">%s</a>" % (request.childLink(name), name),
                "<a href=\"%s\">%s</a>" % (urllib.quote(name, safe=''), name),
                align="center", class_="Builder")
        data += " </tr>\n"

        if phase == 1:
            f = self.phase1
        else:
            f = self.phase2
        data += f(request, changeNames + builderNames, timestamps, eventGrid,
                  sourceEvents)

        data += "</table>\n"

        data += "<hr />\n"

        data += "<a href=\"http://buildbot.sourceforge.net/\">Buildbot</a>"
        data += "-%s " % version
        if projectName:
            data += "working for the "
            if projectURL:
                data += "<a href=\"%s\">%s</a> project." % (projectURL,
                                                            projectName)
            else:
                data += "%s project." % projectName
        data += "<br />\n"
        # TODO: push this to the right edge, if possible
        data += ("Page built: " +
                 time.strftime("%a %d %b %Y %H:%M:%S",
                               time.localtime(util.now()))
                 + "\n")
        return data

    def body0(self, request, builders):
        # build the waterfall display
        data = ""
        data += "<h2>Basic display</h2>\n"
        data += "<p>See <a href=\"%s\">here</a>" % \
                urllib.quote(request.childLink("waterfall"))
        data += " for the waterfall display</p>\n"
                
        data += '<table border="0" cellspacing="0">\n'
        names = map(lambda builder: builder.name, builders)

        # the top row is two blank spaces, then the top-level status boxes
        data += " <tr>\n"
        data += td("", colspan=2)
        for b in builders:
            text = ""
            color = "#ca88f7"
            state, builds = b.getState()
            if state != "offline":
                text += "%s<br />\n" % state #b.getCurrentBig().text[0]
            else:
                text += "OFFLINE<br />\n"
                color = "#ffe0e0"
            data += td(text, align="center", bgcolor=color)

        # the next row has the column headers: time, changes, builder names
        data += " <tr>\n"
        data += td("Time", align="center")
        data += td("Changes", align="center")
        for name in names:
            data += td(
                "<a href=\"%s\">%s</a>" % (urllib.quote(request.childLink(name)), name),
                align="center")
        data += " </tr>\n"

        # all further rows involve timestamps, commit events, and build events
        data += " <tr>\n"
        data += td("04:00", align="bottom")
        data += td("fred", align="center")
        for name in names:
            data += td("stuff", align="center", bgcolor="red")
        data += " </tr>\n"

        data += "</table>\n"
        return data
    
    def buildGrid(self, request, builders):
        debug = False

        # XXX: see if we can use a cached copy

        # first step is to walk backwards in time, asking each column
        # (commit, all builders) if they have any events there. Build up the
        # array of events, and stop when we have a reasonable number.
            
        commit_source = self.changemaster

        lastEventTime = util.now()
        sources = [commit_source] + builders
        changeNames = ["changes"]
        builderNames = map(lambda builder: builder.getName(), builders)
        sourceNames = changeNames + builderNames
        sourceEvents = []
        sourceGenerators = []
        for s in sources:
            gen = insertGaps(s.eventGenerator(), lastEventTime)
            sourceGenerators.append(gen)
            # get the first event
            try:
                e = gen.next()
                event = interfaces.IStatusEvent(e)
                if debug:
                    log.msg("gen %s gave1 %s" % (gen, event.getText()))
            except StopIteration:
                event = None
            sourceEvents.append(event)
        eventGrid = []
        timestamps = []
        spanLength = 10  # ten-second chunks
        tooOld = util.now() - 12*60*60 # never show more than 12 hours
        maxPageLen = 200

        lastEventTime = 0
        for e in sourceEvents:
            if e and e.getTimes()[0] > lastEventTime:
                lastEventTime = e.getTimes()[0]
        if lastEventTime == 0:
            lastEventTime = util.now()

        spanStart = lastEventTime - spanLength
        debugGather = 0

        while 1:
            if debugGather: log.msg("checking (%s,]" % spanStart)
            # the tableau of potential events is in sourceEvents[]. The
            # window crawls backwards, and we examine one source at a time.
            # If the source's top-most event is in the window, is it pushed
            # onto the events[] array and the tableau is refilled. This
            # continues until the tableau event is not in the window (or is
            # missing).

            spanEvents = [] # for all sources, in this span. row of eventGrid
            firstTimestamp = None # timestamp of first event in the span
            lastTimestamp = None # last pre-span event, for next span

            for c in range(len(sourceGenerators)):
                events = [] # for this source, in this span. cell of eventGrid
                event = sourceEvents[c]
                while event and spanStart < event.getTimes()[0]:
                    # to look at windows that don't end with the present,
                    # condition the .append on event.time <= spanFinish
                    if not IBox(event, None):
                        log.msg("BAD EVENT", event, event.getText())
                        assert 0
                    if debug:
                        log.msg("pushing", event.getText(), event)
                    events.append(event)
                    starts, finishes = event.getTimes()
                    firstTimestamp = util.earlier(firstTimestamp, starts)
                    try:
                        event = sourceGenerators[c].next()
                        #event = interfaces.IStatusEvent(event)
                        if debug:
                            log.msg("gen[%s] gave2 %s" % (sourceNames[c],
                                                          event.getText()))
                    except StopIteration:
                        event = None
                if debug:
                    log.msg("finished span")

                if event:
                    # this is the last pre-span event for this source
                    lastTimestamp = util.later(lastTimestamp,
                                               event.getTimes()[0])
                if debugGather:
                    log.msg(" got %s from %s" % (events, sourceNames[c]))
                sourceEvents[c] = event # refill the tableau
                spanEvents.append(events)

            if firstTimestamp is not None:
                eventGrid.append(spanEvents)
                timestamps.append(firstTimestamp)
            

            if lastTimestamp:
                spanStart = lastTimestamp - spanLength
            else:
                # no more events
                break
            if lastTimestamp < tooOld:
                pass
                #break
            if len(timestamps) > maxPageLen:
                break
            
            
            # now loop
            
        # loop is finished. now we have eventGrid[] and timestamps[]
        if debugGather: log.msg("finished loop")
        assert(len(timestamps) == len(eventGrid))
        return (changeNames, builderNames, timestamps, eventGrid, sourceEvents)
    
    def phase0(self, request, sourceNames, timestamps, eventGrid):
        # phase0 rendering
        if not timestamps:
            return "no events"
        data = ""
        for r in range(0, len(timestamps)):
            data += "<p>\n"
            data += "[%s]<br />" % timestamps[r]
            row = eventGrid[r]
            assert(len(row) == len(sourceNames))
            for c in range(0, len(row)):
                if row[c]:
                    data += "<b>%s</b><br />\n" % sourceNames[c]
                    for e in row[c]:
                        log.msg("Event", r, c, sourceNames[c], e.getText())
                        lognames = [loog.getName() for loog in e.getLogs()]
                        data += "%s: %s: %s %s<br />" % (e.getText(),
                                                         e.getTimes()[0],
                                                         e.getColor(),
                                                         lognames)
                else:
                    data += "<b>%s</b> [none]<br />\n" % sourceNames[c]
        return data
    
    def phase1(self, request, sourceNames, timestamps, eventGrid,
               sourceEvents):
        # phase1 rendering: table, but boxes do not overlap
        data = ""
        if not timestamps:
            return data
        lastDate = None
        for r in range(0, len(timestamps)):
            chunkstrip = eventGrid[r]
            # chunkstrip is a horizontal strip of event blocks. Each block
            # is a vertical list of events, all for the same source.
            assert(len(chunkstrip) == len(sourceNames))
            maxRows = reduce(lambda x,y: max(x,y),
                             map(lambda x: len(x), chunkstrip))
            for i in range(maxRows):
                data += " <tr>\n";
                if i == 0:
                    stuff = []
                    # add the date at the beginning, and each time it changes
                    today = time.strftime("<b>%d %b %Y</b>",
                                          time.localtime(timestamps[r]))
                    todayday = time.strftime("<b>%a</b>",
                                             time.localtime(timestamps[r]))
                    if today != lastDate:
                        stuff.append(todayday)
                        stuff.append(today)
                        lastDate = today
                    stuff.append(
                        time.strftime("%H:%M:%S",
                                      time.localtime(timestamps[r])))
                    data += td(stuff, valign="bottom", align="center",
                               rowspan=maxRows, class_="Time")
                for c in range(0, len(chunkstrip)):
                    block = chunkstrip[c]
                    assert(block != None) # should be [] instead
                    # bottom-justify
                    offset = maxRows - len(block)
                    if i < offset:
                        data += td("")
                    else:
                        e = block[i-offset]
                        box = IBox(e).getBox()
                        box.parms["show_idle"] = 1
                        data += box.td(valign="top", align="center")
                data += " </tr>\n"
        
        return data
    
    def phase2(self, request, sourceNames, timestamps, eventGrid,
               sourceEvents):
        data = ""
        if not timestamps:
            return data
        # first pass: figure out the height of the chunks, populate grid
        grid = []
        for i in range(1+len(sourceNames)):
            grid.append([])
        # grid is a list of columns, one for the timestamps, and one per
        # event source. Each column is exactly the same height. Each element
        # of the list is a single <td> box.
        lastDate = time.strftime("<b>%d %b %Y</b>",
                                 time.localtime(util.now()))
        for r in range(0, len(timestamps)):
            chunkstrip = eventGrid[r]
            # chunkstrip is a horizontal strip of event blocks. Each block
            # is a vertical list of events, all for the same source.
            assert(len(chunkstrip) == len(sourceNames))
            maxRows = reduce(lambda x,y: max(x,y),
                             map(lambda x: len(x), chunkstrip))
            for i in range(maxRows):
                if i != maxRows-1:
                    grid[0].append(None)
                else:
                    # timestamp goes at the bottom of the chunk
                    stuff = []
                    # add the date at the beginning (if it is not the same as
                    # today's date), and each time it changes
                    todayday = time.strftime("<b>%a</b>",
                                             time.localtime(timestamps[r]))
                    today = time.strftime("<b>%d %b %Y</b>",
                                          time.localtime(timestamps[r]))
                    if today != lastDate:
                        stuff.append(todayday)
                        stuff.append(today)
                        lastDate = today
                    stuff.append(
                        time.strftime("%H:%M:%S",
                                      time.localtime(timestamps[r])))
                    grid[0].append(Box(text=stuff, class_="Time",
                                       valign="bottom", align="center"))

            # at this point the timestamp column has been populated with
            # maxRows boxes, most None but the last one has the time string
            for c in range(0, len(chunkstrip)):
                block = chunkstrip[c]
                assert(block != None) # should be [] instead
                for i in range(maxRows - len(block)):
                    # fill top of chunk with blank space
                    grid[c+1].append(None)
                for i in range(len(block)):
                    # so the events are bottom-justified
                    b = IBox(block[i]).getBox()
                    b.parms['valign'] = "top"
                    b.parms['align'] = "center"
                    grid[c+1].append(b)
            # now all the other columns have maxRows new boxes too
        # populate the last row, if empty
        gridlen = len(grid[0])
        for i in range(len(grid)):
            strip = grid[i]
            assert(len(strip) == gridlen)
            if strip[-1] == None:
                if sourceEvents[i-1]:
                    filler = IBox(sourceEvents[i-1]).getBox()
                else:
                    # this can happen if you delete part of the build history
                    filler = Box(text=["?"], align="center")
                strip[-1] = filler
            strip[-1].parms['rowspan'] = 1
        # second pass: bubble the events upwards to un-occupied locations
        # Every square of the grid that has a None in it needs to have
        # something else take its place.
        noBubble = request.args.get("nobubble",['0'])
        noBubble = int(noBubble[0])
        if not noBubble:
            for col in range(len(grid)):
                strip = grid[col]
                if col == 1: # changes are handled differently
                    for i in range(2, len(strip)+1):
                        # only merge empty boxes. Don't bubble commit boxes.
                        if strip[-i] == None:
                            next = strip[-i+1]
                            assert(next)
                            if next:
                                #if not next.event:
                                if next.spacer:
                                    # bubble the empty box up
                                    strip[-i] = next
                                    strip[-i].parms['rowspan'] += 1
                                    strip[-i+1] = None
                                else:
                                    # we are above a commit box. Leave it
                                    # be, and turn the current box into an
                                    # empty one
                                    strip[-i] = Box([], rowspan=1,
                                                    comment="commit bubble")
                                    strip[-i].spacer = True
                            else:
                                # we are above another empty box, which
                                # somehow wasn't already converted.
                                # Shouldn't happen
                                pass
                else:
                    for i in range(2, len(strip)+1):
                        # strip[-i] will go from next-to-last back to first
                        if strip[-i] == None:
                            # bubble previous item up
                            assert(strip[-i+1] != None)
                            strip[-i] = strip[-i+1]
                            strip[-i].parms['rowspan'] += 1
                            strip[-i+1] = None
                        else:
                            strip[-i].parms['rowspan'] = 1
        # third pass: render the HTML table
        for i in range(gridlen):
            data += " <tr>\n";
            for strip in grid:
                b = strip[i]
                if b:
                    data += b.td()
                else:
                    if noBubble:
                        data += td([])
                # Nones are left empty, rowspan should make it all fit
            data += " </tr>\n"
        return data


class StatusResource(Resource):
    status = None
    control = None
    favicon = None
    robots_txt = None

    def __init__(self, status, control, changemaster, categories, css):
        """
        @type  status:       L{buildbot.status.builder.Status}
        @type  control:      L{buildbot.master.Control}
        @type  changemaster: L{buildbot.changes.changes.ChangeMaster}
        """
        Resource.__init__(self)
        self.status = status
        self.control = control
        self.changemaster = changemaster
        self.categories = categories
        self.css = css
        waterfall = WaterfallStatusResource(self.status, changemaster,
                                            categories, css)
        self.putChild("", waterfall)

    def render(self, request):
        request.redirect(request.prePathURL() + '/')
        request.finish()

    def getChild(self, path, request):
        if path == "robots.txt" and self.robots_txt:
            return static.File(self.robots_txt)
        if path == "buildbot.css" and self.css:
            return static.File(self.css)
        if path == "changes":
            return StatusResourceChanges(self.status, self.changemaster)
        if path == "favicon.ico":
            if self.favicon:
                return static.File(self.favicon)
            return NoResource("No favicon.ico registered")

        if path in self.status.getBuilderNames():
            builder = self.status.getBuilder(path)
            control = None
            if self.control:
                control = self.control.getBuilder(path)
            return StatusResourceBuilder(self.status, builder, control)

        return NoResource("No such Builder '%s'" % path)

if hasattr(sys, "frozen"):
    # all 'data' files are in the directory of our executable
    here = os.path.dirname(sys.executable)
    buildbot_icon = os.path.abspath(os.path.join(here, "buildbot.png"))
    buildbot_css = os.path.abspath(os.path.join(here, "classic.css"))
else:
    # running from source
    # the icon is sibpath(__file__, "../buildbot.png") . This is for
    # portability.
    up = os.path.dirname
    buildbot_icon = os.path.abspath(os.path.join(up(up(__file__)),
                                                 "buildbot.png"))
    buildbot_css = os.path.abspath(os.path.join(up(__file__), "classic.css"))

class Waterfall(base.StatusReceiverMultiService):
    """I implement the primary web-page status interface, called a 'Waterfall
    Display' because builds and steps are presented in a grid of boxes which
    move downwards over time. The top edge is always the present. Each column
    represents a single builder. Each box describes a single Step, which may
    have logfiles or other status information.

    All these pages are served via a web server of some sort. The simplest
    approach is to let the buildmaster run its own webserver, on a given TCP
    port, but it can also publish its pages to a L{twisted.web.distrib}
    distributed web server (which lets the buildbot pages be a subset of some
    other web server).

    Since 0.6.3, BuildBot defines class attributes on elements so they can be
    styled with CSS stylesheets. Buildbot uses some generic classes to
    identify the type of object, and some more specific classes for the
    various kinds of those types. It does this by specifying both in the
    class attributes where applicable, separated by a space. It is important
    that in your CSS you declare the more generic class styles above the more
    specific ones. For example, first define a style for .Event, and below
    that for .SUCCESS

    The following CSS class names are used:
        - Activity, Event, BuildStep, LastBuild: general classes
        - waiting, interlocked, building, offline, idle: Activity states
        - start, running, success, failure, warnings, skipped, exception:
          LastBuild and BuildStep states
        - Change: box with change
        - Builder: box for builder name (at top)
        - Project
        - Time

    @type parent: L{buildbot.master.BuildMaster}
    @ivar parent: like all status plugins, this object is a child of the
                  BuildMaster, so C{.parent} points to a
                  L{buildbot.master.BuildMaster} instance, through which
                  the status-reporting object is acquired.
    """

    compare_attrs = ["http_port", "distrib_port", "allowForce",
                     "categories", "css", "favicon", "robots_txt"]

    def __init__(self, http_port=None, distrib_port=None, allowForce=True,
                 categories=None, css=buildbot_css, favicon=buildbot_icon,
                 robots_txt=None):
        """To have the buildbot run its own web server, pass a port number to
        C{http_port}. To have it run a web.distrib server

        @type  http_port: int or L{twisted.application.strports} string
        @param http_port: a strports specification describing which port the
                          buildbot should use for its web server, with the
                          Waterfall display as the root page. For backwards
                          compatibility this can also be an int. Use
                          'tcp:8000' to listen on that port, or
                          'tcp:12345:interface=127.0.0.1' if you only want
                          local processes to connect to it (perhaps because
                          you are using an HTTP reverse proxy to make the
                          buildbot available to the outside world, and do not
                          want to make the raw port visible).

        @type  distrib_port: int or L{twisted.application.strports} string
        @param distrib_port: Use this if you want to publish the Waterfall
                             page using web.distrib instead. The most common
                             case is to provide a string that is an absolute
                             pathname to the unix socket on which the
                             publisher should listen
                             (C{os.path.expanduser(~/.twistd-web-pb)} will
                             match the default settings of a standard
                             twisted.web 'personal web server'). Another
                             possibility is to pass an integer, which means
                             the publisher should listen on a TCP socket,
                             allowing the web server to be on a different
                             machine entirely. Both forms are provided for
                             backwards compatibility; the preferred form is a
                             strports specification like
                             'unix:/home/buildbot/.twistd-web-pb'. Providing
                             a non-absolute pathname will probably confuse
                             the strports parser.

        @type  allowForce: bool
        @param allowForce: if True, present a 'Force Build' button on the
                           per-Builder page that allows visitors to the web
                           site to initiate a build. If False, don't provide
                           this button.

        @type  favicon: string
        @param favicon: if set, provide the pathname of an image file that
                        will be used for the 'favicon.ico' resource. Many
                        browsers automatically request this file and use it
                        as an icon in any bookmark generated from this site.
                        Defaults to the buildbot/buildbot.png image provided
                        in the distribution. Can be set to None to avoid
                        using a favicon at all.

        @type  robots_txt: string
        @param robots_txt: if set, provide the pathname of a robots.txt file.
                           Many search engines request this file and obey the
                           rules in it. E.g. to disallow them to crawl the
                           status page, put the following two lines in
                           robots.txt::
                              User-agent: *
                              Disallow: /
        """

        base.StatusReceiverMultiService.__init__(self)
        assert allowForce in (True, False) # TODO: implement others
        if type(http_port) is int:
            http_port = "tcp:%d" % http_port
        self.http_port = http_port
        if distrib_port is not None:
            if type(distrib_port) is int:
                distrib_port = "tcp:%d" % distrib_port
            if distrib_port[0] in "/~.": # pathnames
                distrib_port = "unix:%s" % distrib_port
        self.distrib_port = distrib_port
        self.allowForce = allowForce
        self.categories = categories
        self.css = css
        self.favicon = favicon
        self.robots_txt = robots_txt

    def __repr__(self):
        if self.http_port is None:
            return "<Waterfall on path %s>" % self.distrib_port
        if self.distrib_port is None:
            return "<Waterfall on port %s>" % self.http_port
        return "<Waterfall on port %s and path %s>" % (self.http_port,
                                                       self.distrib_port)

    def setServiceParent(self, parent):
        """
        @type  parent: L{buildbot.master.BuildMaster}
        """
        base.StatusReceiverMultiService.setServiceParent(self, parent)
        self.setup()

    def setup(self):
        status = self.parent.getStatus()
        if self.allowForce:
            control = interfaces.IControl(self.parent)
        else:
            control = None
        change_svc = self.parent.change_svc
        sr = StatusResource(status, control, change_svc, self.categories,
                            self.css)
        sr.favicon = self.favicon
        sr.robots_txt = self.robots_txt
        self.site = server.Site(sr)

        if self.http_port is not None:
            s = strports.service(self.http_port, self.site)
            s.setServiceParent(self)
        if self.distrib_port is not None:
            f = pb.PBServerFactory(distrib.ResourcePublisher(self.site))
            s = strports.service(self.distrib_port, f)
            s.setServiceParent(self)
