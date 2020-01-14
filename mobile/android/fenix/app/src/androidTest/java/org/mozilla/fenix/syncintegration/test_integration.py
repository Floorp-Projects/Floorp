import os
import sys


def test_sync_account_settings(tps, gradlewbuild):
    gradlewbuild.test('checkAccountSettings')

def test_sync_history_from_desktop(tps, gradlewbuild):
    os.chdir('app/src/androidTest/java/org/mozilla/fenix/syncintegration/')
    tps.run('test_history.js')
    gradlewbuild.test('checkHistoryFromDesktopTest')

def test_sync_bookmark_from_device(tps, gradlewbuild):
    os.chdir('app/src/androidTest/java/org/mozilla/fenix/syncintegration/')
    tps.run('test_bookmark.js')
    gradlewbuild.test('checkBookmarkFromDesktopTest')

def test_sync_logins_from_device(tps, gradlewbuild):
    os.chdir('app/src/androidTest/java/org/mozilla/fenix/syncintegration/')
    tps.run('test_logins.js')
    gradlewbuild.test('checkLoginsFromDesktopTest')
