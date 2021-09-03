#!/usr/bin/env python
# coding: utf-8

"""
A rudimentary backward- and forward-compatible script to benchmark pystache.

Usage:

tests/benchmark.py 10000

"""

import sys
from timeit import Timer

import pystache

# TODO: make the example realistic.

examples = [
    # Test case: 1
    ("""{{#person}}Hi {{name}}{{/person}}""",
    {"person": {"name": "Jon"}},
    "Hi Jon"),

    # Test case: 2
    ("""\
<div class="comments">
<h3>{{header}}</h3>
<ul>
{{#comments}}<li class="comment">
<h5>{{name}}</h5><p>{{body}}</p>
</li>{{/comments}}
</ul>
</div>""",
    {'header': "My Post Comments",
     'comments': [
         {'name': "Joe", 'body': "Thanks for this post!"},
         {'name': "Sam", 'body': "Thanks for this post!"},
         {'name': "Heather", 'body': "Thanks for this post!"},
         {'name': "Kathy", 'body': "Thanks for this post!"},
         {'name': "George", 'body': "Thanks for this post!"}]},
    """\
<div class="comments">
<h3>My Post Comments</h3>
<ul>
<li class="comment">
<h5>Joe</h5><p>Thanks for this post!</p>
</li><li class="comment">
<h5>Sam</h5><p>Thanks for this post!</p>
</li><li class="comment">
<h5>Heather</h5><p>Thanks for this post!</p>
</li><li class="comment">
<h5>Kathy</h5><p>Thanks for this post!</p>
</li><li class="comment">
<h5>George</h5><p>Thanks for this post!</p>
</li>
</ul>
</div>"""),
]


def make_test_function(example):

    template, context, expected = example

    def test():
        actual = pystache.render(template, context)
        if actual != expected:
            raise Exception("Benchmark mismatch: \n%s\n*** != ***\n%s" % (expected, actual))

    return test


def main(sys_argv):
    args = sys_argv[1:]
    count = int(args[0])

    print "Benchmarking: %sx" % count
    print

    for example in examples:

        test = make_test_function(example)

        t = Timer(test,)
        print min(t.repeat(repeat=3, number=count))

    print "Done"


if __name__ == '__main__':
    main(sys.argv)

