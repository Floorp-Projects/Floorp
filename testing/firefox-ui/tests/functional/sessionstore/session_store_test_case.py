# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_puppeteer import PuppeteerMixin
from marionette_harness import MarionetteTestCase


class SessionStoreTestCase(PuppeteerMixin, MarionetteTestCase):

    def setUp(self, startup_page=1, include_private=True, no_auto_updates=True):
        super(SessionStoreTestCase, self).setUp()
        # Each list element represents a window of tabs loaded at
        # some testing URL
        self.test_windows = set([
            # Window 1. Note the comma after the absolute_url call -
            # this is Python's way of declaring a 1 item tuple.
            (self.marionette.absolute_url('layout/mozilla.html'), ),

            # Window 2
            (self.marionette.absolute_url('layout/mozilla_organizations.html'),
             self.marionette.absolute_url('layout/mozilla_community.html')),

            # Window 3
            (self.marionette.absolute_url('layout/mozilla_governance.html'),
             self.marionette.absolute_url('layout/mozilla_grants.html')),
        ])

        self.private_windows = set([
            (self.marionette.absolute_url('layout/mozilla_mission.html'),
             self.marionette.absolute_url('layout/mozilla_organizations.html')),

            (self.marionette.absolute_url('layout/mozilla_projects.html'),
             self.marionette.absolute_url('layout/mozilla_mission.html')),
        ])

        self.marionette.enforce_gecko_prefs({
            # Set browser restore previous session pref,
            # depending on what the test requires.
            'browser.startup.page': startup_page,
            # Make the content load right away instead of waiting for
            # the user to click on the background tabs
            'browser.sessionstore.restore_on_demand': False,
            # Avoid race conditions by having the content process never
            # send us session updates unless the parent has explicitly asked
            # for them via the TabStateFlusher.
            'browser.sessionstore.debug.no_auto_updates': no_auto_updates,
        })

        self.all_windows = self.test_windows.copy()
        self.open_windows(self.test_windows)

        if include_private:
            self.all_windows.update(self.private_windows)
            self.open_windows(self.private_windows, is_private=True)

    def tearDown(self):
        try:
            # Create a fresh profile for subsequent tests.
            self.restart(clean=True)
        finally:
            super(SessionStoreTestCase, self).tearDown()

    def open_windows(self, window_sets, is_private=False):
        """ Opens a set of windows with tabs pointing at some
        URLs.

        @param window_sets (list)
               A set of URL tuples. Each tuple within window_sets
               represents a window, and each URL in the URL
               tuples represents what will be loaded in a tab.

               Note that if is_private is False, then the first
               URL tuple will be opened in the current window, and
               subequent tuples will be opened in new windows.

               Example:

               set(
                   (self.marionette.absolute_url('layout/mozilla_1.html'),
                    self.marionette.absolute_url('layout/mozilla_2.html')),

                   (self.marionette.absolute_url('layout/mozilla_3.html'),
                    self.marionette.absolute_url('layout/mozilla_4.html')),
               )

               This would take the currently open window, and load
               mozilla_1.html and mozilla_2.html in new tabs. It would
               then open a new, second window, and load tabs at
               mozilla_3.html and mozilla_4.html.
        @param is_private (boolean, optional)
               Whether or not any new windows should be a private browsing
               windows.
        """

        if (is_private):
            win = self.browser.open_browser(is_private=True)
            win.switch_to()
        else:
            win = self.browser

        for index, urls in enumerate(window_sets):
            if index > 0:
                win = self.browser.open_browser(is_private=is_private)
            win.switch_to()
            self.open_tabs(win, urls)

    def open_tabs(self, win, urls):
        """ Opens a set of URLs inside a window in new tabs.

        @param win (browser window)
               The browser window to load the tabs in.
        @param urls (tuple)
               A tuple of URLs to load in this window. The
               first URL will be loaded in the currently selected
               browser tab. Subsequent URLs will be loaded in
               new tabs.
        """
        # If there are any remaining URLs for this window,
        # open some new tabs and navigate to them.
        with self.marionette.using_context('content'):
            if isinstance(urls, str):
                self.marionette.navigate(urls)
            else:
                for index, url in enumerate(urls):
                    if index > 0:
                        with self.marionette.using_context('chrome'):
                            win.tabbar.open_tab()
                    self.marionette.navigate(url)

    def convert_open_windows_to_set(self):
        windows = self.puppeteer.windows.all

        # There's no guarantee that Marionette will return us an
        # iterator for the opened windows that will match the
        # order within our window list. Instead, we'll convert
        # the list of URLs within each open window to a set of
        # tuples that will allow us to do a direct comparison
        # while allowing the windows to be in any order.

        opened_windows = set()
        for win in windows:
            urls = tuple()
            for tab in win.tabbar.tabs:
                urls = urls + tuple([tab.location])
            opened_windows.add(urls)
        return opened_windows

    def simulate_os_shutdown(self):
        """ Simulate an OS shutdown.

        :raises: Exception: if not supported on the current platform
        :raises: WindowsError: if a Windows API call failed
        """

        if self.marionette.session_capabilities['platformName'] != 'windows_nt':
            raise Exception('Unsupported platform for simulate_os_shutdown')

        self._shutdown_with_windows_restart_manager(self.marionette.process_id)

    def _shutdown_with_windows_restart_manager(self, pid):
        """ Shut down a process using the Windows Restart Manager.

        When Windows shuts down, it uses a protocol including the
        WM_QUERYENDSESSION and WM_ENDSESSION messages to give
        applications a chance to shut down safely. The best way to
        simulate this is via the Restart Manager, which allows a process
        (such as an installer) to use the same mechanism to shut down
        any other processes which are using registered resources.

        This function starts a Restart Manager session, registers the
        process as a resource, and shuts down the process.

        :param pid: The process id (int) of the process to shutdown

        :raises: WindowsError: if a Windows API call fails
        """

        import ctypes
        from ctypes import Structure, POINTER, WINFUNCTYPE, windll, pointer, WinError
        from ctypes.wintypes import HANDLE, DWORD, BOOL, WCHAR, UINT, ULONG, LPCWSTR

        # set up Windows SDK types
        OpenProcess = windll.kernel32.OpenProcess
        OpenProcess.restype = HANDLE
        OpenProcess.argtypes = [DWORD,  # dwDesiredAccess
                                BOOL,   # bInheritHandle
                                DWORD]  # dwProcessId
        PROCESS_QUERY_INFORMATION = 0x0400

        class FILETIME(Structure):
            _fields_ = [('dwLowDateTime', DWORD),
                        ('dwHighDateTime', DWORD)]
        LPFILETIME = POINTER(FILETIME)

        GetProcessTimes = windll.kernel32.GetProcessTimes
        GetProcessTimes.restype = BOOL
        GetProcessTimes.argtypes = [HANDLE,      # hProcess
                                    LPFILETIME,  # lpCreationTime
                                    LPFILETIME,  # lpExitTime
                                    LPFILETIME,  # lpKernelTime
                                    LPFILETIME]  # lpUserTime

        ERROR_SUCCESS = 0

        class RM_UNIQUE_PROCESS(Structure):
            _fields_ = [('dwProcessId', DWORD),
                        ('ProcessStartTime', FILETIME)]

        RmStartSession = windll.rstrtmgr.RmStartSession
        RmStartSession.restype = DWORD
        RmStartSession.argtypes = [POINTER(DWORD),  # pSessionHandle
                                   DWORD,           # dwSessionFlags
                                   POINTER(WCHAR)]  # strSessionKey

        class GUID(ctypes.Structure):
            _fields_ = [('Data1', ctypes.c_ulong),
                        ('Data2', ctypes.c_ushort),
                        ('Data3', ctypes.c_ushort),
                        ('Data4', ctypes.c_ubyte * 8)]
        CCH_RM_SESSION_KEY = ctypes.sizeof(GUID) * 2

        RmRegisterResources = windll.rstrtmgr.RmRegisterResources
        RmRegisterResources.restype = DWORD
        RmRegisterResources.argtypes = [DWORD,             # dwSessionHandle
                                        UINT,              # nFiles
                                        POINTER(LPCWSTR),  # rgsFilenames
                                        UINT,              # nApplications
                                        POINTER(RM_UNIQUE_PROCESS),  # rgApplications
                                        UINT,              # nServices
                                        POINTER(LPCWSTR)]  # rgsServiceNames

        RM_WRITE_STATUS_CALLBACK = WINFUNCTYPE(None, UINT)
        RmShutdown = windll.rstrtmgr.RmShutdown
        RmShutdown.restype = DWORD
        RmShutdown.argtypes = [DWORD,  # dwSessionHandle
                               ULONG,  # lActionFlags
                               RM_WRITE_STATUS_CALLBACK]  # fnStatus

        RmEndSession = windll.rstrtmgr.RmEndSession
        RmEndSession.restype = DWORD
        RmEndSession.argtypes = [DWORD]  # dwSessionHandle

        # Get the info needed to uniquely identify the process
        hProc = OpenProcess(PROCESS_QUERY_INFORMATION, False, pid)
        if not hProc:
            raise WinError()

        creationTime = FILETIME()
        exitTime = FILETIME()
        kernelTime = FILETIME()
        userTime = FILETIME()
        if not GetProcessTimes(hProc,
                               pointer(creationTime),
                               pointer(exitTime),
                               pointer(kernelTime),
                               pointer(userTime)):
            raise WinError()

        # Start the Restart Manager Session
        dwSessionHandle = DWORD()
        sessionKeyType = WCHAR * (CCH_RM_SESSION_KEY + 1)
        sessionKey = sessionKeyType()
        if RmStartSession(pointer(dwSessionHandle), 0, sessionKey) != ERROR_SUCCESS:
            raise WinError()

        try:
            UProcs_count = 1
            UProcsArrayType = RM_UNIQUE_PROCESS * UProcs_count
            UProcs = UProcsArrayType(RM_UNIQUE_PROCESS(pid, creationTime))

            # Register the process as a resource
            if RmRegisterResources(dwSessionHandle,
                                   0, None,
                                   UProcs_count, UProcs,
                                   0, None) != ERROR_SUCCESS:
                raise WinError()

            # Shut down all processes using registered resources
            if RmShutdown(dwSessionHandle, 0,
                          ctypes.cast(None, RM_WRITE_STATUS_CALLBACK)) != ERROR_SUCCESS:
                raise WinError()

        finally:
            RmEndSession(dwSessionHandle)
