import os
import sys

def test_sync_account_settings(tps, gradlewbuild):
    gradlewbuild.test('checkAccountSettings')

def test_sync_history_from_desktop(tps, gradlewbuild):
    os.chdir('app/src/androidTest/java/org/mozilla/fenix/syncintegration/')
    tps.run('test_history.js')
    gradlewbuild.test('checkHistoryFromDesktopTest')
'''
def test_sync_bookmark_from_desktop(tps, gradlewbuild):
    os.chdir('app/src/androidTest/java/org/mozilla/fenix/syncintegration/')
    tps.run('test_bookmark.js')
    gradlewbuild.test('checkBookmarkFromDesktopTest')

def test_sync_logins_from_desktop(tps, gradlewbuild):
    os.chdir('app/src/androidTest/java/org/mozilla/fenix/syncintegration/')
    tps.run('test_logins.js')
    gradlewbuild.test('checkLoginsFromDesktopTest')

def test_sync_bookmark_from_device(tps, gradlewbuild):
    os.chdir('app/src/androidTest/java/org/mozilla/fenix/syncintegration/')
    gradlewbuild.test('checkBookmarkFromDeviceTest')
    tps.run('app/src/androidTest/java/org/mozilla/fenix/syncintegration/test_bookmark_desktop.js')

def test_sync_history_from_device(tps, gradlewbuild):
    os.chdir('app/src/androidTest/java/org/mozilla/fenix/syncintegration/')
    gradlewbuild.test('checkHistoryFromDeviceTest')
    tps.run('app/src/androidTest/java/org/mozilla/fenix/syncintegration/test_history_desktop.js')
'''