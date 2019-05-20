# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# originally taken from /testing/talos/talos/filter.py

from __future__ import absolute_import

import math

"""
data filters:
takes a series of run data and applies statistical transforms to it

Each filter is a simple function, but it also have attached a special
`prepare` method that create a tuple with one instance of a
:class:`Filter`; this allow to write stuff like::

  from raptor import filters
  filter_list = filters.ignore_first.prepare(1) + filters.median.prepare()

  for filter in filter_list:
      data = filter(data)
  # data is filtered
"""

_FILTERS = {}


class Filter(object):
    def __init__(self, func, *args, **kwargs):
        """
        Takes a filter function, and save args and kwargs that
        should be used when the filter is used.
        """
        self.func = func
        self.args = args
        self.kwargs = kwargs

    def apply(self, data):
        """
        Apply the filter on the data, and return the new data
        """
        return self.func(data, *self.args, **self.kwargs)


def define_filter(func):
    """
    decorator to attach the prepare method.
    """
    def prepare(*args, **kwargs):
        return (Filter(func, *args, **kwargs),)
    func.prepare = prepare
    return func


def register_filter(func):
    """
    all filters defined in this module
    should be registered
    """
    global _FILTERS

    _FILTERS[func.__name__] = func
    return func


def filters(*args):
    global _FILTERS

    filters_ = [_FILTERS[filter] for filter in args]
    return filters_


def apply(data, filters):
    for filter in filters:
        data = filter(data)

    return data


def parse(string_):

    def to_number(string_number):
        try:
            return int(string_number)
        except ValueError:
            return float(string_number)

    tokens = string_.split(":")

    func = tokens[0]
    digits = []
    if len(tokens) > 1:
        digits.extend(tokens[1].split(","))
        digits = [to_number(digit) for digit in digits]

    return [func, digits]


# filters that return a scalar

@register_filter
@define_filter
def mean(series):
    """
    mean of data; needs at least one data point
    """
    return sum(series)/float(len(series))


@register_filter
@define_filter
def median(series):
    """
    median of data; needs at least one data point
    """
    series = sorted(series)
    if len(series) % 2:
        # odd
        return series[len(series)/2]
    else:
        # even
        middle = len(series)/2  # the higher of the middle 2, actually
        return 0.5*(series[middle-1] + series[middle])


@register_filter
@define_filter
def variance(series):
    """
    variance: http://en.wikipedia.org/wiki/Variance
    """

    _mean = mean(series)
    variance = sum([(i-_mean)**2 for i in series])/float(len(series))
    return variance


@register_filter
@define_filter
def stddev(series):
    """
    standard deviation: http://en.wikipedia.org/wiki/Standard_deviation
    """
    return variance(series)**0.5


@register_filter
@define_filter
def dromaeo(series):
    """
    dromaeo: https://wiki.mozilla.org/Dromaeo, pull the internal calculation
    out
      * This is for 'runs/s' based tests, not 'ms' tests.
      * chunksize: defined in dromaeo: tests/dromaeo/webrunner.js#l8
    """
    means = []
    chunksize = 5
    series = list(dromaeo_chunks(series, chunksize))
    for i in series:
        means.append(mean(i))
    return geometric_mean(means)


@register_filter
@define_filter
def dromaeo_chunks(series, size):
    for i in range(0, len(series), size):
        yield series[i:i+size]


@register_filter
@define_filter
def geometric_mean(series):
    """
    geometric_mean: http://en.wikipedia.org/wiki/Geometric_mean
    """
    total = 0
    for i in series:
        total += math.log(i+1)
    return math.exp(total / len(series)) - 1

# filters that return a list


@register_filter
@define_filter
def ignore_first(series, number=1):
    """
    ignore first datapoint
    """
    if len(series) <= number:
        # don't modify short series
        return series
    return series[number:]


@register_filter
@define_filter
def ignore(series, function):
    """
    ignore the first value of a list given by function
    """
    if len(series) <= 1:
        # don't modify short series
        return series
    series = series[:]  # do not mutate the original series
    value = function(series)
    series.remove(value)
    return series


@register_filter
@define_filter
def ignore_max(series):
    """
    ignore maximum data point
    """
    return ignore(series, max)


@register_filter
@define_filter
def ignore_min(series):
    """
    ignore minimum data point
    """
    return ignore(series, min)


@register_filter
@define_filter
def ignore_negative(series):
    """
    ignore data points that have a negative value
    caution: if all data values are < 0, this will return an empty list
    """
    if len(series) <= 1:
        # don't modify short series
        return series
    series = series[:]  # do not mutate the original series
    return list(filter(lambda x: x >= 0, series))


@register_filter
@define_filter
def v8_subtest(series, name):
    """
       v8 benchmark score - modified for no sub benchmarks.
       * removed Crypto and kept Encrypt/Decrypt standalone
       * removed EarlyBoyer and kept Earley/Boyer standalone

       this is not 100% in parity but within .3%
    """
    reference = {'Encrypt': 266181.,
                 'Decrypt': 266181.,
                 'DeltaBlue': 66118.,
                 'Earley': 666463.,
                 'Boyer': 666463.,
                 'NavierStokes': 1484000.,
                 'RayTrace': 739989.,
                 'RegExp': 910985.,
                 'Richards': 35302.,
                 'Splay': 81491.
                 }

    return reference[name] / geometric_mean(series)


@register_filter
@define_filter
def responsiveness_Metric(val_list):
    return sum([float(x)*float(x) / 1000000.0 for x in val_list])
