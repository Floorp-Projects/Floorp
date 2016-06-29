import math

"""
data filters:
takes a series of run data and applies statistical transforms to it

Each filter is a simple function, but it also have attached a special
`prepare` method that create a tuple with one instance of a
:class:`Filter`; this allow to write stuff like::

  from talos import filter
  filters = filter.ignore_first.prepare(1) + filter.median.prepare()

  for filter in filters:
      data = filter(data)
  # data is filtered
"""


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

# filters that return a scalar


@define_filter
def mean(series):
    """
    mean of data; needs at least one data point
    """
    return sum(series)/float(len(series))


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


@define_filter
def variance(series):
    """
    variance: http://en.wikipedia.org/wiki/Variance
    """

    _mean = mean(series)
    variance = sum([(i-_mean)**2 for i in series])/float(len(series))
    return variance


@define_filter
def stddev(series):
    """
    standard deviation: http://en.wikipedia.org/wiki/Standard_deviation
    """
    return variance(series)**0.5


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


@define_filter
def dromaeo_chunks(series, size):
    for i in range(0, len(series), size):
        yield series[i:i+size]


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


@define_filter
def ignore_first(series, number=1):
    """
    ignore first datapoint
    """
    if len(series) <= number:
        # don't modify short series
        return series
    return series[number:]


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


@define_filter
def ignore_max(series):
    """
    ignore maximum data point
    """
    return ignore(series, max)


@define_filter
def ignore_min(series):
    """
    ignore minimum data point
    """
    return ignore(series, min)


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


@define_filter
def responsiveness_Metric(val_list):
    return sum([float(x)*float(x) / 1000000.0 for x in val_list])
