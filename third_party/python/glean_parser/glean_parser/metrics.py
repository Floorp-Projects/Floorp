# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Classes for each of the high-level metric types.
"""

import enum
import sys

from . import util


# Import a backport of PEP487 to support __init_subclass__
if sys.version_info < (3, 6):
    import pep487

    base_object = pep487.PEP487Object
else:
    base_object = object


class Lifetime(enum.Enum):
    ping = 0
    user = 1
    application = 2


class Metric(base_object):
    glean_internal_metric_cat = "glean.internal.metrics"
    metric_types = {}
    default_store_names = ["metrics"]

    def __init__(
        self,
        type,
        category,
        name,
        bugs,
        description,
        notification_emails,
        expires,
        data_reviews=None,
        version=0,
        disabled=False,
        lifetime="ping",
        send_in_pings=None,
        unit="",
        gecko_datapoint="",
        no_lint=None,
        _config=None,
        _validated=False,
    ):
        # Avoid cyclical import
        from . import parser

        self.type = type
        self.category = category
        self.name = name
        self.bugs = bugs
        self.description = description
        self.notification_emails = notification_emails
        self.expires = expires
        if data_reviews is None:
            data_reviews = []
        self.data_reviews = data_reviews
        self.version = version
        self.disabled = disabled
        self.lifetime = getattr(Lifetime, lifetime)
        if send_in_pings is None:
            send_in_pings = ["default"]
        self.send_in_pings = send_in_pings
        self.unit = unit
        self.gecko_datapoint = gecko_datapoint
        if no_lint is None:
            no_lint = []
        self.no_lint = no_lint

        # _validated indicates whether this metric has already been jsonschema
        # validated (but not any of the Python-level validation).
        if not _validated:
            data = {
                "$schema": parser.METRICS_ID,
                self.category: {self.name: self.serialize()},
            }
            for error in parser.validate(data):
                raise ValueError(error)

        # Metrics in the special category "glean.internal.metrics" need to have
        # an empty category string when identifying the metrics in the ping.
        if self.category == Metric.glean_internal_metric_cat:
            self.category = ""

    def __init_subclass__(cls, **kwargs):
        # Create a mapping of all of the subclasses of this class
        if cls not in Metric.metric_types and hasattr(cls, "typename"):
            Metric.metric_types[cls.typename] = cls
        super().__init_subclass__(**kwargs)

    @classmethod
    def make_metric(cls, category, name, metric_info, config={}, validated=False):
        """
        Given a metric_info dictionary from metrics.yaml, return a metric
        instance.

        :param: category The category the metric lives in
        :param: name The name of the metric
        :param: metric_info A dictionary of the remaining metric parameters
        :param: config A dictionary containing commandline configuration
            parameters
        :param: validated True if the metric has already gone through
            jsonschema validation
        :return: A new Metric instance.
        """
        metric_type = metric_info["type"]
        return cls.metric_types[metric_type](
            category=category,
            name=name,
            _validated=validated,
            _config=config,
            **metric_info
        )

    def serialize(self):
        """
        Serialize the metric back to JSON object model.
        """
        d = self.__dict__.copy()
        # Convert enum fields back to strings
        for key, val in d.items():
            if isinstance(val, enum.Enum):
                d[key] = d[key].name
            if isinstance(val, set):
                d[key] = sorted(list(val))
        del d["name"]
        del d["category"]
        return d

    def identifier(self):
        """
        Create an identifier unique for this metric.
        Generally, category.name; however, Glean internal
        metrics only use name.
        """
        if not self.category:
            return self.name
        return ".".join((self.category, self.name))

    def is_disabled(self):
        return self.disabled or self.is_expired()

    def is_expired(self):
        return util.is_expired(self.expires)

    @staticmethod
    def validate_expires(expires):
        return util.validate_expires(expires)

    def is_internal_metric(self):
        return self.category in (Metric.glean_internal_metric_cat, "")


class Boolean(Metric):
    typename = "boolean"


class String(Metric):
    typename = "string"


class StringList(Metric):
    typename = "string_list"


class Counter(Metric):
    typename = "counter"


class Quantity(Metric):
    typename = "quantity"


class TimeUnit(enum.Enum):
    nanosecond = 0
    microsecond = 1
    millisecond = 2
    second = 3
    minute = 4
    hour = 5
    day = 6


class TimeBase(Metric):
    def __init__(self, *args, **kwargs):
        self.time_unit = getattr(TimeUnit, kwargs.pop("time_unit", "millisecond"))
        super().__init__(*args, **kwargs)


class Timespan(TimeBase):
    typename = "timespan"


class TimingDistribution(TimeBase):
    typename = "timing_distribution"


class MemoryUnit(enum.Enum):
    byte = 0
    kilobyte = 1
    megabyte = 2
    gigabyte = 3


class MemoryDistribution(Metric):
    typename = "memory_distribution"

    def __init__(self, *args, **kwargs):
        self.memory_unit = getattr(MemoryUnit, kwargs.pop("memory_unit", "byte"))
        super().__init__(*args, **kwargs)


class HistogramType(enum.Enum):
    linear = 0
    exponential = 1


class CustomDistribution(Metric):
    typename = "custom_distribution"

    def __init__(self, *args, **kwargs):
        self.range_min = kwargs.pop("range_min", 1)
        self.range_max = kwargs.pop("range_max")
        self.bucket_count = kwargs.pop("bucket_count")
        self.histogram_type = getattr(
            HistogramType, kwargs.pop("histogram_type", "exponential")
        )
        super().__init__(*args, **kwargs)


class Datetime(TimeBase):
    typename = "datetime"


class Event(Metric):
    typename = "event"

    default_store_names = ["events"]

    _generate_enums = [("extra_keys", "Keys")]

    def __init__(self, *args, **kwargs):
        self.extra_keys = kwargs.pop("extra_keys", {})
        self.validate_extra_keys(self.extra_keys, kwargs.get("_config", {}))
        super().__init__(*args, **kwargs)

    @property
    def allowed_extra_keys(self):
        # Sort keys so that output is deterministic
        return sorted(list(self.extra_keys.keys()))

    @staticmethod
    def validate_extra_keys(extra_keys, config):
        if not config.get("allow_reserved") and any(
            k.startswith("glean.") for k in extra_keys.keys()
        ):
            raise ValueError(
                "Extra keys beginning with 'glean.' are reserved for "
                "Glean internal use."
            )


class Uuid(Metric):
    typename = "uuid"


class Labeled(Metric):
    labeled = True

    def __init__(self, *args, **kwargs):
        labels = kwargs.pop("labels", None)
        if labels is not None:
            self.ordered_labels = labels
            self.labels = set(labels)
        else:
            self.ordered_labels = None
            self.labels = None
        super().__init__(*args, **kwargs)

    def serialize(self):
        """
        Serialize the metric back to JSON object model.
        """
        d = super().serialize()
        d["labels"] = self.ordered_labels
        del d["ordered_labels"]
        return d


class LabeledBoolean(Labeled, Boolean):
    typename = "labeled_boolean"


class LabeledString(Labeled, String):
    typename = "labeled_string"


class LabeledCounter(Labeled, Counter):
    typename = "labeled_counter"
