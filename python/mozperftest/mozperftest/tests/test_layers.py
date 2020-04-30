#!/usr/bin/env python
import pytest
import mozunit
from mock import MagicMock
from mozperftest.layers import Layer, Layers
from mozperftest.environment import MachEnvironment


class _TestLayer(Layer):
    name = "test"
    activated = True
    called = 0
    arguments = {"--arg1": {"type": str, "default": "xxx", "help": "arg1"}}

    def setup(self):
        self.called += 1

    def teardown(self):
        self.called += 1


class _TestLayer2(_TestLayer):
    name = "test2"
    activated = True
    arguments = {"arg2": {"type": str, "default": "xxx", "help": "arg2"}}


class _TestLayer3(_TestLayer):
    name = "test3"
    activated = True


def test_layer():
    mach = MagicMock()
    env = MachEnvironment(mach, test=True, test_arg1="ok")

    with _TestLayer(env, mach) as layer:
        layer.info("info")
        layer.debug("debug")
        assert layer.get_arg("test")
        assert layer.get_arg("arg1") == "ok"
        assert layer.get_arg("test-arg1") == "ok"
        layer.set_arg("arg1", "two")
        assert layer.get_arg("test-arg1") == "two"
        layer.set_arg("test-arg1", 1)
        assert layer.get_arg("test-arg1") == 1
        with pytest.raises(KeyError):
            layer.set_arg("another", 1)

        layer(object())

    assert layer.called == 2


def test_layers():
    mach = MagicMock()
    factories = [_TestLayer, _TestLayer2, _TestLayer3]
    env = MachEnvironment(
        mach, no_test3=True, test_arg1="ok", test2=True, test2_arg2="2"
    )

    with Layers(env, mach, factories) as layers:
        # layer3 was deactivated with test3=False
        assert len(layers.layers) == 2
        layers.info("info")
        layers.debug("debug")
        assert layers.get_arg("--test2")
        assert layers.get_arg("test-arg1") == "ok"
        layers.set_arg("test-arg1", "two")
        assert layers.get_arg("test-arg1") == "two"
        layers.set_arg("--test-arg1", 1)
        assert layers.get_arg("test-arg1") == 1
        assert layers.get_layer("test2").name == "test2"
        assert layers.get_layer("test3") is None
        assert layers.name == "test + test2"
        with pytest.raises(KeyError):
            layers.set_arg("another", 1)

    for layer in layers:
        assert layer.called == 2


if __name__ == "__main__":
    mozunit.main()
