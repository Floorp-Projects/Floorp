#!/usr/bin/python

import sys
import json

sys.path.append('./')
import codecs
from fluent.syntax import ast, serialize


def read_json(path):
    with codecs.open(path, 'r', encoding='utf-8') as file:
        return json.load(file)


def pretty_print(fileType, data):
    resource = ast.from_json(data)
    print(serialize(resource))

if __name__ == "__main__":
    file_type = 'ftl'
    f = read_json(sys.argv[1])
    pretty_print(file_type, f)
