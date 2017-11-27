from __future__ import absolute_import

import mozunit
import pytest

from talos.test import Test, TsBase, ts_paint
from talos.test import register_test
from talos.test import test_dict


class BasicTestA(Test):
    pass


class BasicTestB(Test):
    pass


class BasicTestC(Test):
    """basic description"""
    keys = [
        'nonnull_attrib',
        'null_attrib'
    ]

    nonnull_attrib = 'value'
    null_attrib = None


class NotATest(object):
    pass


class Test_register_test(object):

    def test_same_instance_returned(self):
        decorator = register_test()
        NewBasicTest = decorator(BasicTestA)

        assert BasicTestA is NewBasicTest

    def test_class_registered(self):
        _TESTS = test_dict()
        decorator = register_test()

        # class registered
        decorator(BasicTestB)
        assert 'BasicTestB' in _TESTS
        assert BasicTestB in _TESTS.values()

        # cannot register same class
        with pytest.raises(AssertionError):
            decorator(BasicTestB)

        # # cannot register other class type
        with pytest.raises(AssertionError):
            decorator(NotATest)


class TestTest(object):

    def test_same_class_name(self):
        assert BasicTestA.name() == 'BasicTestA'

    def test_class_doc(self):
        assert BasicTestA.description() is not None
        assert BasicTestC.description() == 'basic description'

    def test_init(self):
        basic_test = BasicTestA(new_attrib_a='value_a', new_attrib_b='value_b')
        assert basic_test.new_attrib_a == 'value_a'
        assert basic_test.new_attrib_b == 'value_b'

    def test_update(self):
        basic_test = BasicTestA()
        basic_test.update(new_attrib_a='value_a', new_attrib_b='value_b')

        assert basic_test.new_attrib_a == 'value_a'
        assert basic_test.new_attrib_b == 'value_b'

        basic_test.update(new_attrib_c='value_c')
        assert basic_test.new_attrib_c == 'value_c'

    def test_items(self):
        basic_test = BasicTestC()

        # returns iterable
        try:
            iter(basic_test.items())
        except TypeError:
            pytest.fail('Test.items() did not return iterator')

        tuple_result = basic_test.items()[0]
        assert len(tuple_result) == 2

        # returns not nones
        assert ('nonnull_attrib', 'value') in basic_test.items()
        assert ('null_attrib', None) not in basic_test.items()

        # not overriden Test instance
        test_instance = Test()
        assert test_instance.items() == [('name', 'Test')]

        # overriden Test instance
        test_instance = Test(unregistered_attr='value')
        assert ('unregistered_attr', 'value') not in test_instance.items()

        test_instance = Test()
        test_instance.update(keys=['cycles', 'desktop', 'lower_is_better'])
        assert dict(test_instance.items()) == {
            'name': 'Test', 'desktop': True, 'lower_is_better': True}

        test_instance = Test()
        test_instance.update(new_attrib='some')
        assert ('new_attrib', 'some') not in test_instance.items()

        test_instance = Test()
        test_instance.update(keys=['new_attrib'], new_attrib='value')
        assert dict(test_instance.items()) == {'name': 'Test', 'new_attrib': 'value'}

        test_instance = Test(cycles=20, desktop=False)
        assert test_instance.cycles == 20
        assert test_instance.desktop is False

        test_instance = Test()
        test_instance.update(cycles=20, desktop=False)
        assert test_instance.cycles == 20
        assert test_instance.desktop is False


class TestTsBase(object):
    ts_base_registered_keys = {
        'url',
        'url_timestamp',
        'timeout',
        'cycles',
        'profile_path',
        'gecko_profile',
        'gecko_profile_interval',
        'gecko_profile_entries',
        'gecko_profile_startup',
        'preferences',
        'xperf_counters',
        'xperf_providers',
        'xperf_user_providers',
        'xperf_stackwalk',
        'tpmozafterpaint',
        'fnbpaint',
        'profile',
        'firstpaint',
        'userready',
        'testeventmap',
        'base_vs_ref',
        'extensions',
        'filters',
        'setup',
        'cleanup',
        'webextensions',
        'reinstall',
    }

    def setup_method(self):
        self.test_instance = TsBase()

    def test_no_unknown_keys_are_somehow_added_alongside_registered_ones(self):
        assert set(self.test_instance.keys) == self.ts_base_registered_keys

        self.test_instance.update(attribute_one='value', attribute_two='value')
        assert set(self.test_instance.keys) == self.ts_base_registered_keys

    def test_nonnull_keys_show_up(self):
        assert dict(self.test_instance.items()) == {
            'name': 'TsBase',
            'filters': self.test_instance.filters
        }

        self.test_instance.update(timeout=500)
        assert dict(self.test_instance.items()) == {
            'name': 'TsBase',
            'filters': self.test_instance.filters,
            'timeout': 500
        }


class Test_ts_paint(object):
    def test_test_nonnull_keys_show_up(self):
        test_instance = ts_paint()
        keys = {key for key, _ in test_instance.items()}
        assert keys == {
            'name',
            'cycles',
            'timeout',
            'gecko_profile_startup',
            'gecko_profile_entries',
            'url',
            'xperf_counters',
            'filters',
            'tpmozafterpaint'
        }


if __name__ == '__main__':
    mozunit.main()
