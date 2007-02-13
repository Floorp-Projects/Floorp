#! /usr/bin/python

from twisted.web import resource
from twisted.web.error import NoResource
from twisted.web.html import PRE

# these are our test result types. Steps are responsible for mapping results
# into these values.
SKIP, EXPECTED_FAILURE, FAILURE, ERROR, UNEXPECTED_SUCCESS, SUCCESS = \
      "skip", "expected failure", "failure", "error", "unexpected success", \
      "success"
UNKNOWN = "unknown" # catch-all


class OneTest(resource.Resource):
    isLeaf = 1
    def __init__(self, parent, testName, results):
        self.parent = parent
        self.testName = testName
        self.resultType, self.results = results

    def render(self, request):
        request.setHeader("content-type", "text/html")
        if request.method == "HEAD":
            request.setHeader("content-length", len(self.html(request)))
            return ''
        return self.html(request)

    def html(self, request):
        # turn ourselves into HTML
        raise NotImplementedError

class TestResults(resource.Resource):
    oneTestClass = OneTest
    def __init__(self):
        resource.Resource.__init__(self)
        self.tests = {}
    def addTest(self, testName, resultType, results=None):
        self.tests[testName] = (resultType, results)
    # TODO: .setName and .delete should be used on our Swappable
    def countTests(self):
        return len(self.tests)
    def countFailures(self):
        failures = 0
        for t in self.tests.values():
            if t[0] in (FAILURE, ERROR):
                failures += 1
        return failures
    def summary(self):
        """Return a short list of text strings as a summary, suitable for
        inclusion in an Event"""
        return ["some", "tests"]
    def describeOneTest(self, testname):
        return "%s: %s\n" % (testname, self.tests[testname][0])
    def html(self):
        data = "<html>\n<head><title>Test Results</title></head>\n"
        data += "<body>\n"
        data += "<pre>\n"
        tests = self.tests.keys()
        tests.sort()
        for testname in tests:
            data += self.describeOneTest(testname)
        data += "</pre>\n"
        data += "</body></html>\n"
        return data
    def render(self, request):
        request.setHeader("content-type", "text/html")
        if request.method == "HEAD":
            request.setHeader("content-length", len(self.html()))
            return ''
        return self.html()
    def getChild(self, path, request):
        if self.tests.has_key(path):
            return self.oneTestClass(self, path, self.tests[path])
        return NoResource("No such test '%s'" % path)
