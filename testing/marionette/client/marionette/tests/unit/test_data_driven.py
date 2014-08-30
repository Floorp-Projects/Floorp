from marionette_test import parameterized, with_parameters, MetaParameterized, \
                            MarionetteTestCase

class Parameterizable(object):
    __metaclass__ = MetaParameterized

class TestDataDriven(MarionetteTestCase):
    def test_parameterized(self):
        class Test(Parameterizable):
            def __init__(self):
                self.parameters = []

            @parameterized('1', 'thing', named=43)
            @parameterized('2', 'thing2')
            def test(self, thing, named=None):
                self.parameters.append((thing, named))

        self.assertFalse(hasattr(Test, 'test'))
        self.assertTrue(hasattr(Test, 'test_1'))
        self.assertTrue(hasattr(Test, 'test_2'))

        test = Test()
        test.test_1()
        test.test_2()

        self.assertEquals(test.parameters, [('thing', 43), ('thing2', None)])

    def test_with_parameters(self):
        DATA = [('1', ('thing',), {'named': 43}),
                ('2', ('thing2',), {'named': None})]

        class Test(Parameterizable):
            def __init__(self):
                self.parameters = []

            @with_parameters(DATA)
            def test(self, thing, named=None):
                self.parameters.append((thing, named))

        self.assertFalse(hasattr(Test, 'test'))
        self.assertTrue(hasattr(Test, 'test_1'))
        self.assertTrue(hasattr(Test, 'test_2'))

        test = Test()
        test.test_1()
        test.test_2()

        self.assertEquals(test.parameters, [('thing', 43), ('thing2', None)])

    def test_parameterized_same_name_raises_error(self):
        with self.assertRaises(KeyError):
            class Test(Parameterizable):
                @parameterized('1', 'thing', named=43)
                @parameterized('1', 'thing2')
                def test(self, thing, named=None):
                    pass

    def test_marionette_test_case_is_parameterizable(self):
        self.assertTrue(issubclass(MarionetteTestCase.__metaclass__, MetaParameterized))
