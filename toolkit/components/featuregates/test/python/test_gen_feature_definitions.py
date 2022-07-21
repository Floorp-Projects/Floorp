import json
import sys
import unittest
from os import path
from textwrap import dedent

import mozunit
import pytoml
import voluptuous
from io import StringIO


FEATURE_GATES_ROOT_PATH = path.abspath(
    path.join(path.dirname(__file__), path.pardir, path.pardir)
)
sys.path.append(FEATURE_GATES_ROOT_PATH)
from gen_feature_definitions import (
    ExceptionGroup,
    expand_feature,
    feature_schema,
    FeatureGateException,
    hyphens_to_camel_case,
    main,
    process_configured_value,
    process_files,
)


def make_test_file_path(name):
    return path.join(FEATURE_GATES_ROOT_PATH, "test", "python", "data", name + ".toml")


def minimal_definition(**kwargs):
    defaults = {
        "id": "test-feature",
        "title": "Test Feature",
        "description": "A feature for testing things",
        "bug-numbers": [1479127],
        "restart-required": False,
        "type": "boolean",
    }
    defaults.update(dict([(k.replace("_", "-"), v) for k, v in kwargs.items()]))
    return defaults


class TestHyphensToCamelCase(unittest.TestCase):
    simple_cases = [
        ("", ""),
        ("singleword", "singleword"),
        ("more-than-one-word", "moreThanOneWord"),
    ]

    def test_simple_cases(self):
        for in_string, out_string in self.simple_cases:
            assert hyphens_to_camel_case(in_string) == out_string


class TestExceptionGroup(unittest.TestCase):
    def test_str_indentation_of_grouped_lines(self):
        errors = [
            Exception("single line error 1"),
            Exception("single line error 2"),
            Exception("multiline\nerror 1"),
            Exception("multiline\nerror 2"),
        ]

        assert str(ExceptionGroup(errors)) == dedent(
            """\
        There were errors while processing feature definitions:
          * single line error 1
          * single line error 2
          * multiline
            error 1
          * multiline
            error 2"""
        )


class TestFeatureGateException(unittest.TestCase):
    def test_str_no_file(self):
        error = FeatureGateException("oops")
        assert str(error) == "In unknown file: oops"

    def test_str_with_file(self):
        error = FeatureGateException("oops", filename="some/bad/file.txt")
        assert str(error) == 'In file "some/bad/file.txt": oops'

    def test_repr_no_file(self):
        error = FeatureGateException("oops")
        assert repr(error) == "FeatureGateException('oops', filename=None)"

    def test_repr_with_file(self):
        error = FeatureGateException("oops", filename="some/bad/file.txt")
        assert (
            repr(error) == "FeatureGateException('oops', filename='some/bad/file.txt')"
        )


class TestProcessFiles(unittest.TestCase):
    def test_valid_file(self):
        filename = make_test_file_path("good")
        result = process_files([filename])
        assert result == {
            "demo-feature": {
                "id": "demo-feature",
                "title": "Demo Feature",
                "description": "A no-op feature to demo the feature gate system.",
                "restartRequired": False,
                "preference": "foo.bar.baz",
                "type": "boolean",
                "bugNumbers": [1479127],
                "isPublic": {"default": True},
                "defaultValue": {"default": False},
            },
            "minimal-feature": {
                "id": "minimal-feature",
                "title": "Minimal Feature",
                "description": "The smallest feature that is valid",
                "restartRequired": True,
                "preference": "features.minimal-feature.enabled",
                "type": "boolean",
                "bugNumbers": [1479127],
                "isPublic": {"default": False},
                "defaultValue": {"default": None},
            },
        }

    def test_invalid_toml(self):
        filename = make_test_file_path("invalid_toml")
        with self.assertRaises(ExceptionGroup) as context:
            process_files([filename])
        error_group = context.exception
        assert len(error_group.errors) == 1
        assert type(error_group.errors[0]) == pytoml.TomlError

    def test_empty_feature(self):
        filename = make_test_file_path("empty_feature")
        with self.assertRaises(ExceptionGroup) as context:
            process_files([filename])
        error_group = context.exception
        assert len(error_group.errors) == 1
        assert type(error_group.errors[0]) == FeatureGateException
        assert "required key not provided" in str(error_group.errors[0])

    def test_missing_file(self):
        filename = make_test_file_path("file_does_not_exist")
        with self.assertRaises(ExceptionGroup) as context:
            process_files([filename])
        error_group = context.exception
        assert len(error_group.errors) == 1
        assert type(error_group.errors[0]) == FeatureGateException
        assert "No such file or directory" in str(error_group.errors[0])


class TestFeatureSchema(unittest.TestCase):
    def make_test_features(self, *overrides):
        if len(overrides) == 0:
            overrides = [{}]
        features = {}
        for override in overrides:
            feature = minimal_definition(**override)
            feature_id = feature.pop("id")
            features[feature_id] = feature
        return features

    def test_minimal_valid(self):
        definition = self.make_test_features()
        # should not raise an exception
        feature_schema(definition)

    def test_extra_keys_not_allowed(self):
        definition = self.make_test_features({"unexpected_key": "oh no!"})
        with self.assertRaises(voluptuous.Error) as context:
            feature_schema(definition)
        assert "extra keys not allowed" in str(context.exception)

    def test_required_fields(self):
        required_keys = [
            "title",
            "description",
            "bug-numbers",
            "restart-required",
            "type",
        ]
        for key in required_keys:
            definition = self.make_test_features({"id": "test-feature"})
            del definition["test-feature"][key]
            with self.assertRaises(voluptuous.Error) as context:
                feature_schema(definition)
            assert "required key not provided" in str(context.exception)
            assert key in str(context.exception)

    def test_nonempty_keys(self):
        test_parameters = [("title", ""), ("description", ""), ("bug-numbers", [])]
        for key, empty in test_parameters:
            definition = self.make_test_features({key: empty})
            with self.assertRaises(voluptuous.Error) as context:
                feature_schema(definition)
            assert "length of value must be at least" in str(context.exception)
            assert "['{}']".format(key) in str(context.exception)


class ExpandFeatureTests(unittest.TestCase):
    def test_hyphenation_to_snake_case(self):
        feature = minimal_definition()
        assert "bug-numbers" in feature
        assert "bugNumbers" in expand_feature(feature)

    def test_default_value_default(self):
        feature = minimal_definition(type="boolean")
        assert "default-value" not in feature
        assert "defaultValue" not in feature
        assert expand_feature(feature)["defaultValue"] == {"default": None}

    def test_default_value_override_constant(self):
        feature = minimal_definition(type="boolean", default_value=True)
        assert expand_feature(feature)["defaultValue"] == {"default": True}

    def test_default_value_override_configured_value(self):
        feature = minimal_definition(
            type="boolean", default_value={"default": False, "nightly": True}
        )
        assert expand_feature(feature)["defaultValue"] == {
            "default": False,
            "nightly": True,
        }

    def test_preference_default(self):
        feature = minimal_definition(type="boolean")
        assert "preference" not in feature
        assert expand_feature(feature)["preference"] == "features.test-feature.enabled"

    def test_preference_override(self):
        feature = minimal_definition(preference="test.feature.a")
        assert expand_feature(feature)["preference"] == "test.feature.a"


class ProcessConfiguredValueTests(unittest.TestCase):
    def test_expands_single_values(self):
        for value in [True, False, 2, "features"]:
            assert process_configured_value("test", value) == {"default": value}

    def test_default_key_is_required(self):
        with self.assertRaises(FeatureGateException) as context:
            assert process_configured_value("test", {"nightly": True})
        assert "has no default" in str(context.exception)

    def test_invalid_keys_rejected(self):
        with self.assertRaises(FeatureGateException) as context:
            assert process_configured_value("test", {"default": True, "bogus": True})
        assert "Unexpected target bogus" in str(context.exception)

    def test_simple_key(self):
        value = {"nightly": True, "default": False}
        assert process_configured_value("test", value) == value

    def test_compound_keys(self):
        value = {"win,nightly": True, "default": False}
        assert process_configured_value("test", value) == value

    def test_multiple_keys(self):
        value = {"win": True, "mac": True, "default": False}
        assert process_configured_value("test", value) == value


class MainTests(unittest.TestCase):
    def test_it_outputs_json(self):
        output = StringIO()
        filename = make_test_file_path("good")
        main(output, filename)
        output.seek(0)
        results = json.load(output)
        assert results == {
            u"demo-feature": {
                u"id": u"demo-feature",
                u"title": u"Demo Feature",
                u"description": u"A no-op feature to demo the feature gate system.",
                u"restartRequired": False,
                u"preference": u"foo.bar.baz",
                u"type": u"boolean",
                u"bugNumbers": [1479127],
                u"isPublic": {u"default": True},
                u"defaultValue": {u"default": False},
            },
            u"minimal-feature": {
                u"id": u"minimal-feature",
                u"title": u"Minimal Feature",
                u"description": u"The smallest feature that is valid",
                u"restartRequired": True,
                u"preference": u"features.minimal-feature.enabled",
                u"type": u"boolean",
                u"bugNumbers": [1479127],
                u"isPublic": {u"default": False},
                u"defaultValue": {u"default": None},
            },
        }

    def test_it_returns_1_for_errors(self):
        output = StringIO()
        filename = make_test_file_path("invalid_toml")
        assert main(output, filename) == 1
        assert output.getvalue() == ""


if __name__ == "__main__":
    mozunit.main(*sys.argv[1:])
