# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Classes for each of the high-level metric types.
"""

import enum
from typing import Any, Dict, List, Optional, Type, Union  # noqa


from . import pings
from . import util


# Important: if the values are ever changing here, make sure
# to also fix mozilla/glean. Otherwise language bindings may
# break there.
class Lifetime(enum.Enum):
    ping = 0
    application = 1
    user = 2


class DataSensitivity(enum.Enum):
    technical = 1
    interaction = 2
    web_activity = 3
    highly_sensitive = 4


class Metric:
    typename: str = "ERROR"
    glean_internal_metric_cat: str = "glean.internal.metrics"
    metric_types: Dict[str, Any] = {}
    default_store_names: List[str] = ["metrics"]

    def __init__(
        self,
        type: str,
        category: str,
        name: str,
        bugs: List[str],
        description: str,
        notification_emails: List[str],
        expires: Any,
        data_reviews: Optional[List[str]] = None,
        version: int = 0,
        disabled: bool = False,
        lifetime: str = "ping",
        send_in_pings: Optional[List[str]] = None,
        unit: Optional[str] = None,
        gecko_datapoint: str = "",
        no_lint: Optional[List[str]] = None,
        data_sensitivity: Optional[List[str]] = None,
        defined_in: Optional[Dict] = None,
        telemetry_mirror: Optional[str] = None,
        _config: Dict[str, Any] = None,
        _validated: bool = False,
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
        if unit is not None:
            self.unit = unit
        self.gecko_datapoint = gecko_datapoint
        if no_lint is None:
            no_lint = []
        self.no_lint = no_lint
        if data_sensitivity is not None:
            self.data_sensitivity = [
                getattr(DataSensitivity, x) for x in data_sensitivity
            ]
        self.defined_in = defined_in
        if telemetry_mirror is not None:
            self.telemetry_mirror = telemetry_mirror

        # _validated indicates whether this metric has already been jsonschema
        # validated (but not any of the Python-level validation).
        if not _validated:
            data = {
                "$schema": parser.METRICS_ID,
                self.category: {self.name: self._serialize_input()},
            }  # type: Dict[str, util.JSONType]
            for error in parser.validate(data):
                raise ValueError(error)

        # Store the config, but only after validation.
        if _config is None:
            _config = {}
        self._config = _config

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
    def make_metric(
        cls,
        category: str,
        name: str,
        metric_info: Dict[str, util.JSONType],
        config: Optional[Dict[str, Any]] = None,
        validated: bool = False,
    ):
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
        if config is None:
            config = {}

        metric_type = metric_info["type"]
        if not isinstance(metric_type, str):
            raise TypeError(f"Unknown metric type {metric_type}")
        return cls.metric_types[metric_type](
            category=category,
            name=name,
            defined_in=getattr(metric_info, "defined_in", None),
            _validated=validated,
            _config=config,
            **metric_info,
        )

    def serialize(self) -> Dict[str, util.JSONType]:
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
            if isinstance(val, list) and len(val) and isinstance(val[0], enum.Enum):
                d[key] = [x.name for x in val]
        del d["name"]
        del d["category"]
        return d

    def _serialize_input(self) -> Dict[str, util.JSONType]:
        d = self.serialize()
        modified_dict = util.remove_output_params(d, "defined_in")
        return modified_dict

    def identifier(self) -> str:
        """
        Create an identifier unique for this metric.
        Generally, category.name; however, Glean internal
        metrics only use name.
        """
        if not self.category:
            return self.name
        return ".".join((self.category, self.name))

    def is_disabled(self) -> bool:
        return self.disabled or self.is_expired()

    def is_expired(self) -> bool:
        return self._config.get("custom_is_expired", util.is_expired)(self.expires)

    def validate_expires(self):
        return self._config.get("custom_validate_expires", util.validate_expires)(
            self.expires
        )

    def is_internal_metric(self) -> bool:
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

    def __init__(self, *args, **kwargs):
        self.time_unit = getattr(TimeUnit, kwargs.pop("time_unit", "nanosecond"))
        Metric.__init__(self, *args, **kwargs)


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

    _generate_enums = [("allowed_extra_keys", "Keys")]

    def __init__(self, *args, **kwargs):
        self.extra_keys = kwargs.pop("extra_keys", {})
        self.validate_extra_keys(self.extra_keys, kwargs.get("_config", {}))
        if self.has_extra_types:
            self._generate_enums = [("allowed_extra_keys_with_types", "Extra")]
        super().__init__(*args, **kwargs)

    @property
    def allowed_extra_keys(self):
        # Sort keys so that output is deterministic
        return sorted(list(self.extra_keys.keys()))

    @property
    def allowed_extra_keys_with_types(self):
        # Sort keys so that output is deterministic
        return sorted(
            [(k, v["type"]) for (k, v) in self.extra_keys.items()], key=lambda x: x[0]
        )

    @property
    def has_extra_types(self):
        """
        If any extra key has a `type` specified,
        we generate the new struct/object-based API.
        """
        return any("type" in x for x in self.extra_keys.values())

    @staticmethod
    def validate_extra_keys(extra_keys: Dict[str, str], config: Dict[str, Any]) -> None:
        if not config.get("allow_reserved") and any(
            k.startswith("glean.") for k in extra_keys.keys()
        ):
            raise ValueError(
                "Extra keys beginning with 'glean.' are reserved for "
                "Glean internal use."
            )


class Uuid(Metric):
    typename = "uuid"


class Jwe(Metric):
    typename = "jwe"

    def __init__(self, *args, **kwargs):
        raise ValueError(
            "JWE support was removed. "
            "If you require this send an email to glean-team@mozilla.com."
        )


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

    def serialize(self) -> Dict[str, util.JSONType]:
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


class Rate(Metric):
    typename = "rate"

    def __init__(self, *args, **kwargs):
        self.denominator_metric = kwargs.pop("denominator_metric", None)
        super().__init__(*args, **kwargs)


ObjectTree = Dict[str, Dict[str, Union[Metric, pings.Ping]]]
