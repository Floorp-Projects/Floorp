import unittest
import mozunit
from datetime import datetime
from taskcluster_graph.from_now import (
    InvalidString,
    UnknownTimeMeasurement,
    value_of,
    json_time_from_now
)

class FromNowTest(unittest.TestCase):

    def test_invalid_str(self):
        with self.assertRaises(InvalidString):
            value_of('wtfs')

    def test_missing_unit(self):
        with self.assertRaises(InvalidString):
            value_of('1')

    def test_missing_unknown_unit(self):
        with self.assertRaises(UnknownTimeMeasurement):
            value_of('1z')

    def test_value_of(self):
        self.assertEqual(value_of('1s').total_seconds(), 1)
        self.assertEqual(value_of('1 second').total_seconds(), 1)
        self.assertEqual(value_of('1m').total_seconds(), 60)
        self.assertEqual(value_of('1h').total_seconds(), 3600)
        self.assertEqual(value_of('1d').total_seconds(), 86400)
        self.assertEqual(value_of('1y').total_seconds(), 31536000)

    def test_json_from_now_utc_now(self):
        # Just here to ensure we don't raise.
        time = json_time_from_now('1 years')

    def test_json_from_now(self):
        now = datetime(2014, 1, 1)
        self.assertEqual(json_time_from_now('1 years', now),
                '2015-01-01T00:00:00Z')
        self.assertEqual(json_time_from_now('6 days', now),
                '2014-01-07T00:00:00Z')

if __name__ == '__main__':
    mozunit.main()


