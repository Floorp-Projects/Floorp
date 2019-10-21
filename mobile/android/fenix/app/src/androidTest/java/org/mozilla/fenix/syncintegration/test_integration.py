import os
import sys


def test_sync_history_from_desktop(tps, gradlewbuild):
    tps.run('test_history.js')
    gradlewbuild.test('checkHistoryFromDesktopTest')

def test_sync_bookmark_from_device(tps, gradlewbuild):
    os.chdir('app/src/androidTest/java/org/mozilla/fenix/syncintegration/')
    tps.run('test_bookmark.js')
    gradlewbuild.test('checkBookmarkFromDesktopTest')
