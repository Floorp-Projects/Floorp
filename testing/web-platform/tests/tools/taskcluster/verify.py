import argparse
import json
import os

import jsone
import yaml

here = os.path.dirname(__file__)
root = os.path.abspath(os.path.join(here, "..", ".."))


def create_parser():
    return argparse.ArgumentParser()


def run(venv, **kwargs):
    with open(os.path.join(root, ".taskcluster.yml")) as f:
        template = yaml.safe_load(f)
    with open(os.path.join(here, "testdata/pr_event.json")) as f:
        event = json.load(f)

    context = {"tasks_for": "github-pull-request",
               "event": event,
               "as_slugid": lambda x: x}

    data = jsone.render(template, context)
    print("Got %s tasks for pull request synchonize" % len(data["tasks"]))
    for item in data["tasks"]:
        print(json.dumps(item, indent=2))
