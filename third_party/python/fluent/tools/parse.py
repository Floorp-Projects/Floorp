#!/usr/bin/python

import sys

sys.path.append('./')
import codecs
from fluent.syntax import parse
import json


def read_file(path):
    with codecs.open(path, 'r', encoding='utf-8') as file:
        text = file.read()
    return text


def print_ast(fileType, data):
    ast = parse(data)
    print(json.dumps(ast.to_json(), indent=2, ensure_ascii=False))


if __name__ == "__main__":
    file_type = 'ftl'
    f = read_file(sys.argv[1])
    print_ast(file_type, f)
