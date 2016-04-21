from macholib import framework

import sys
if sys.version_info[:2] <= (2,6):
    import unittest2 as unittest
else:
    import unittest


class TestFramework (unittest.TestCase):
    def test_framework(self):
        self.assertEqual(
                framework.framework_info('Location/Name.framework/Versions/SomeVersion/Name_Suffix'),
                dict(
                    location='Location',
                    name='Name.framework/Versions/SomeVersion/Name_Suffix',
                    shortname='Name',
                    version='SomeVersion',
                    suffix='Suffix',
                ))
        self.assertEqual(
                framework.framework_info('Location/Name.framework/Versions/SomeVersion/Name'),
                dict(
                    location='Location',
                    name='Name.framework/Versions/SomeVersion/Name',
                    shortname='Name',
                    version='SomeVersion',
                    suffix=None,
                ))
        self.assertEqual(
                framework.framework_info('Location/Name.framework/Name_Suffix'),
                dict(
                    location='Location',
                    name='Name.framework/Name_Suffix',
                    shortname='Name',
                    version=None,
                    suffix='Suffix',
                ))
        self.assertEqual(
                framework.framework_info('Location/Name.framework/Name'),
                dict(
                    location='Location',
                    name='Name.framework/Name',
                    shortname='Name',
                    version=None,
                    suffix=None
                ))
        self.assertEqual(
                framework.framework_info('Location/Name.framework.disabled/Name'),
                None
                )
        self.assertEqual(
                framework.framework_info('Location/Name.framework/Versions/A/B/Name'),
                None
                )
        self.assertEqual(
                framework.framework_info('Location/Name.framework/Versions/A'),
                None
                )
        self.assertEqual(
                framework.framework_info('Location/Name.framework/Versions/A/Name/_debug'),
                None
                )

    def test_interal_tests(self):
        # Ported over from the source file
        def d(location=None, name=None, shortname=None, version=None, suffix=None):
            return dict(
                location=location,
                name=name,
                shortname=shortname,
                version=version,
                suffix=suffix
            )
        self.assertEqual(framework.framework_info('completely/invalid'), None)
        self.assertEqual(framework.framework_info('completely/invalid/_debug'), None)
        self.assertEqual(framework.framework_info('P/F.framework'), None)
        self.assertEqual(framework.framework_info('P/F.framework/_debug'), None)
        self.assertEqual(framework.framework_info('P/F.framework/F'), d('P', 'F.framework/F', 'F'))
        self.assertEqual(framework.framework_info('P/F.framework/F_debug'), d('P', 'F.framework/F_debug', 'F', suffix='debug'))
        self.assertEqual(framework.framework_info('P/F.framework/Versions'), None)
        self.assertEqual(framework.framework_info('P/F.framework/Versions/A'), None)
        self.assertEqual(framework.framework_info('P/F.framework/Versions/A/F'), d('P', 'F.framework/Versions/A/F', 'F', 'A'))
        self.assertEqual(framework.framework_info('P/F.framework/Versions/A/F_debug'), d('P', 'F.framework/Versions/A/F_debug', 'F', 'A', 'debug'))


if __name__ == "__main__":
    unittest.main()
