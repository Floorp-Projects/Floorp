# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import multiprocessing
from abc import ABCMeta, abstractproperty
from collections import defaultdict

from flask import Flask, render_template, request

SECTIONS = []
SUPPORTED_KINDS = set()


def register_section(cls):
    assert issubclass(cls, Section)
    instance = cls()
    SECTIONS.append(instance)
    SUPPORTED_KINDS.update(instance.kind.split(","))


class Section(object):
    __metaclass__ = ABCMeta

    @abstractproperty
    def name(self):
        pass

    @abstractproperty
    def kind(self):
        pass

    @abstractproperty
    def title(self):
        pass

    @abstractproperty
    def attrs(self):
        pass

    def contains(self, task):
        return task.kind in self.kind.split(",")

    def get_context(self, tasks):
        labels = defaultdict(lambda: {"max_chunk": 0, "attrs": defaultdict(list)})

        for task in tasks.values():
            if not self.contains(task):
                continue

            task = task.attributes
            label = labels[self.labelfn(task)]
            for attr in self.attrs:
                if attr in task and task[attr] not in label["attrs"][attr]:
                    label["attrs"][attr].append(task[attr])

                if "test_chunk" in task:
                    label["max_chunk"] = max(
                        label["max_chunk"], int(task["test_chunk"])
                    )

        return {
            "name": self.name,
            "kind": self.kind,
            "title": self.title,
            "labels": labels,
        }


@register_section
class Platform(Section):
    name = "platform"
    kind = "build"
    title = "Platforms"
    attrs = ["build_platform"]

    def labelfn(self, task):
        return task["build_platform"]

    def contains(self, task):
        if not Section.contains(self, task):
            return False

        # android-stuff tasks aren't actual platforms
        return task.task["tags"].get("android-stuff", False) != "true"


@register_section
class Test(Section):
    name = "test"
    kind = "test"
    title = "Test Suites"
    attrs = ["unittest_suite"]

    def labelfn(self, task):
        suite = task["unittest_suite"].replace(" ", "-")

        if suite.endswith("-chunked"):
            suite = suite[: -len("-chunked")]

        return suite

    def contains(self, task):
        if not Section.contains(self, task):
            return False
        return task.attributes["unittest_suite"] not in ("raptor", "talos")


@register_section
class Perf(Section):
    name = "perf"
    kind = "test"
    title = "Performance"
    attrs = ["unittest_suite", "raptor_try_name", "talos_try_name"]

    def labelfn(self, task):
        suite = task["unittest_suite"]
        label = task["{}_try_name".format(suite)]

        if not label.startswith(suite):
            label = "{}-{}".format(suite, label)

        if label.endswith("-e10s"):
            label = label[: -len("-e10s")]

        return label

    def contains(self, task):
        if not Section.contains(self, task):
            return False
        return task.attributes["unittest_suite"] in ("raptor", "talos")


@register_section
class Analysis(Section):
    name = "analysis"
    kind = "build,static-analysis-autotest,hazard"
    title = "Analysis"
    attrs = ["build_platform"]

    def labelfn(self, task):
        return task["build_platform"]

    def contains(self, task):
        if not Section.contains(self, task):
            return False
        if task.kind == "build":
            return task.task["tags"].get("android-stuff", False) == "true"
        return True


def create_application(tg, queue: multiprocessing.Queue):
    tasks = {l: t for l, t in tg.tasks.items() if t.kind in SUPPORTED_KINDS}
    sections = [s.get_context(tasks) for s in SECTIONS]
    context = {
        "tasks": {l: t.attributes for l, t in tasks.items()},
        "sections": sections,
    }

    app = Flask(__name__)
    app.env = "development"
    app.tasks = []

    @app.route("/", methods=["GET", "POST"])
    def chooser():
        if request.method == "GET":
            return render_template("chooser.html", **context)

        if request.form["action"] == "Push":
            labels = request.form["selected-tasks"].splitlines()
            app.tasks.extend(labels)

        queue.put(app.tasks)
        return render_template("close.html")

    return app
