import types
from xml.etree import ElementTree

import base
from .. import handlers

def format_test_id(test_id):
    """Take a test id and return something that looks a bit like
    a class path"""
    if type(test_id) not in types.StringTypes:
        #Not sure how to deal with reftests yet
        raise NotImplementedError

    #Turn a path into something like a class heirachy
    return test_id.replace('.', '_').replace('/', ".")


class XUnitFormatter(base.BaseFormatter):
    """The data model here isn't a great match. This implementation creates
    one <testcase> element for each subtest and one more, with no @name
    for each test"""

    def __init__(self):
        self.tree = ElementTree.ElementTree()
        self.root = None
        self.suite_start_time = None
        self.test_start_time = None

        self.tests_run = 0
        self.errors = 0
        self.failures = 0
        self.skips = 0

    def suite_start(self, data):
        self.root = ElementTree.Element("testsuite")
        self.tree.root = self.root
        self.suite_start_time = data["time"]

    def test_start(self, data):
        self.tests_run += 1
        self.test_start_time = data["time"]

    def _create_result(self, data):
        test = ElementTree.SubElement(self.root, "testcase")
        name = format_test_id(data["test"])
        test.attrib["classname"] = name

        if "subtest" in data:
            test.attrib["name"] = data["subtest"]
            # We generally don't know how long subtests take
            test.attrib["time"] = "0"
        else:
            if "." in name:
                test_name = name.rsplit(".", 1)[1]
            else:
                test_name = name
            test.attrib["name"] = test_name
            test.attrib["time"] = "%.2f" % ((data["time"] - self.test_start_time) / 1000)

        if ("expected" in data and data["expected"] != data["status"]):
            if data["status"] in ("NOTRUN", "ASSERT", "ERROR"):
                result = ElementTree.SubElement(test, "error")
                self.errors += 1
            else:
                result = ElementTree.SubElement(test, "failure")
                self.failures += 1

            result.attrib["message"] = "Expected %s, got %s" % (data["status"], data["message"])
            result.text = data["message"]

        elif data["status"] == "SKIP":
            result = ElementTree.SubElement(test, "skipped")
            self.skips += 1

    def test_status(self, data):
        self._create_result(data)

    def test_end(self, data):
        self._create_result(data)

    def suite_end(self, data):
        self.root.attrib.update({"tests": str(self.tests_run),
                                 "errors": str(self.errors),
                                 "failures": str(self.failures),
                                 "skiped": str(self.skips),
                                 "time":   "%.2f" % (
                                     (data["time"] - self.suite_start_time) / 1000)})
        return ElementTree.tostring(self.root, encoding="utf8")

if __name__ == "__main__":
    base.format_file(sys.stdin,
                     handlers.StreamHandler(stream=sys.stdout,
                                            formatter=XUnitFormatter()))
