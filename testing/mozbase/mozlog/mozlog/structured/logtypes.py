# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

convertor_registry = {}
missing = object()
no_default = object()

class log_action(object):
    def __init__(self, *args):
        self.args = {}

        self.args_no_default = []
        self.args_with_default = []

        # These are the required fields in a log message that usually aren't
        # supplied by the caller, but can be in the case of log_raw
        self.default_args = [
            Unicode("action"),
            Int("time"),
            Unicode("thread"),
            Int("pid", default=None),
            Unicode("source"),
            Unicode("component")]

        for arg in args:
            if arg.default is no_default:
                self.args_no_default.append(arg.name)
            else:
                self.args_with_default.append(arg.name)

            if arg.name in self.args:
                raise ValueError("Repeated argument name %s" % arg.name)

            self.args[arg.name] = arg

        for extra in self.default_args:
            self.args[extra.name] = extra


    def __call__(self, f):
        convertor_registry[f.__name__] = self
        converter = self

        def inner(self, *args, **kwargs):
            data = converter.convert(*args, **kwargs)
            return f(self, data)

        return inner

    def convert(self, *args, **kwargs):
        data = {}
        values = {}
        values.update(kwargs)

        j = -1
        for i, arg in enumerate(args):
            j += 1
            # Skip over non-defaulted args supplied as keyword arguments
            while self.args_no_default[j] in values:
                j += 1
                if j == len(self.args_no_default):
                    raise TypeError("Too many arguments")
            values[self.args_no_default[j]] = arg

        for k in range(j+1, len(self.args_no_default)):
            if self.args_no_default[k] not in values:
                raise TypeError("Missing required argument %s\n%r\n%r\n%r" %
                                (self.args_no_default[k], args, kwargs, values))

        # Fill in missing arguments
        for name in self.args_with_default:
            if name not in values:
                values[name] = self.args[name].default

        for key, value in values.iteritems():
            out_value = self.args[key](value)
            if out_value is not missing:
                data[key] = out_value

        return data

    def convert_known(self, **kwargs):
        known_kwargs = {name: value for name, value in kwargs.iteritems()
                        if name in self.args}
        return self.convert(**known_kwargs)

class DataType(object):
    def __init__(self, name, default=no_default, optional=False):
        self.name = name
        self.default = default

        if default is no_default and optional is not False:
            raise ValueError("optional arguments require a default value")

        self.optional = optional

    def __call__(self, value):
        if value == self.default:
            if self.optional:
                return missing
            return self.default

        try:
            return self.convert(value)
        except:
            raise ValueError("Failed to convert value %s of type %s for field %s to type %s" %
                             (value, type(value).__name__, self.name, self.__class__.__name__))

class Unicode(DataType):
    def convert(self, data):
        if isinstance(data, unicode):
            return data
        if isinstance(data, str):
            return data.decode("utf8", "replace")
        return unicode(data)

class TestId(DataType):
    def convert(self, data):
        if isinstance(data, unicode):
            return data
        elif isinstance(data, str):
            return data.decode("utf-8", "replace")
        elif isinstance(data, tuple):
            # This is really a bit of a hack; should really split out convertors from the
            # fields they operate on
            func = Unicode(None).convert
            return tuple(func(item) for item in data)
        else:
            raise ValueError

class Status(DataType):
    allowed = ["PASS", "FAIL", "OK", "ERROR", "TIMEOUT", "CRASH", "ASSERT", "SKIP"]
    def convert(self, data):
        value = data.upper()
        if value not in self.allowed:
            raise ValueError
        return value

class SubStatus(Status):
    allowed = ["PASS", "FAIL", "ERROR", "TIMEOUT", "CRASH", "ASSERT"]

class Dict(DataType):
    def convert(self, data):
        return dict(data)

class List(DataType):
    def __init__(self, name, item_type, default=no_default, optional=False):
        DataType.__init__(self, name, default, optional)
        self.item_type = item_type(None)

    def convert(self, data):
        return [self.item_type.convert(item) for item in data]

class Int(DataType):
    def convert(self, data):
        return int(data)
