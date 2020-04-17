#!/usr/bin/env python
import mozunit
from mozperftest.layers import Layer, Layers
from mock import MagicMock


class TestLayer(Layer):
    called = 0

    def setup(self):
        self.called += 1

    def teardown(self):
        self.called += 1


def test_layer():
    mach = MagicMock()
    env = MagicMock()

    with TestLayer(env, mach) as layer:
        layer.info("info")
        layer.debug("debug")

    assert layer.called == 2


def test_layers():
    mach = MagicMock()
    factories = [TestLayer, TestLayer, TestLayer]
    env = MagicMock()

    with Layers(env, mach, factories) as layers:
        layers.info("info")
        layers.debug("debug")

    for layer in layers:
        assert layer.called == 2


if __name__ == "__main__":
    mozunit.main()
