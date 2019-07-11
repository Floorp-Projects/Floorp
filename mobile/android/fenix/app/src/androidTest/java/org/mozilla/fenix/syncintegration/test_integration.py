import os
import sys


def test_sync_history_from_desktop(tps, gradlewbuild):
    # Running tests
    tps.run('test_history.js')
    gradlewbuild.test('checkHistoryFromDesktopTest')
'''
# For the future, this way we change the test to run....
def test_sync_bookmark_from_device(tps, xcodebuild):
    gradlewbuild.test('checkBookmarkFromDeviceTest')
    tps.run('app/src/androidTest/java/org/mozilla/fenix/ui/SyncIntegrationTests/test_bookmark.js')
'''