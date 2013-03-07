/* -*- Mode: JavaScript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 et filetype=javascript
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [
  "DownloadTaskbarProgress",
];

////////////////////////////////////////////////////////////////////////////////
//// Constants

const Cc = Components.classes;
const Ci = Components.interfaces;

const kTaskbarIDWin = "@mozilla.org/windows-taskbar;1";
const kTaskbarIDMac = "@mozilla.org/widget/macdocksupport;1";

////////////////////////////////////////////////////////////////////////////////
//// DownloadTaskbarProgress Object

this.DownloadTaskbarProgress =
{
  init: function DTP_init()
  {
    if (DownloadTaskbarProgressUpdater) {
      DownloadTaskbarProgressUpdater._init();
    }
  },

  /**
   * Called when a browser window appears. This has an effect only when we
   * don't already have an active window.
   *
   * @param aWindow
   *        The browser window that we'll potentially use to display the
   *        progress.
   */
  onBrowserWindowLoad: function DTP_onBrowserWindowLoad(aWindow)
  {
    this.init();
    if (!DownloadTaskbarProgressUpdater) {
      return;
    }
    if (!DownloadTaskbarProgressUpdater._activeTaskbarProgress) {
      DownloadTaskbarProgressUpdater._setActiveWindow(aWindow, false);
    }
  },

  /**
   * Called when the download window appears. The download window will take
   * over as the active window.
   */
  onDownloadWindowLoad: function DTP_onDownloadWindowLoad(aWindow)
  {
    if (!DownloadTaskbarProgressUpdater) {
      return;
    }
    DownloadTaskbarProgressUpdater._setActiveWindow(aWindow, true);
  },

  /**
   * Getters for internal DownloadTaskbarProgressUpdater values
   */

  get activeTaskbarProgress() {
    if (!DownloadTaskbarProgressUpdater) {
      return null;
    }
    return DownloadTaskbarProgressUpdater._activeTaskbarProgress;
  },

  get activeWindowIsDownloadWindow() {
    if (!DownloadTaskbarProgressUpdater) {
      return null;
    }
    return DownloadTaskbarProgressUpdater._activeWindowIsDownloadWindow;
  },

  get taskbarState() {
    if (!DownloadTaskbarProgressUpdater) {
      return null;
    }
    return DownloadTaskbarProgressUpdater._taskbarState;
  },

};

////////////////////////////////////////////////////////////////////////////////
//// DownloadTaskbarProgressUpdater Object

var DownloadTaskbarProgressUpdater =
{
  /// Whether the taskbar is initialized.
  _initialized: false,

  /// Reference to the taskbar.
  _taskbar: null,

  /// Reference to the download manager.
  _dm: null,

  /**
   * Initialize and register ourselves as a download progress listener.
   */
  _init: function DTPU_init()
  {
    if (this._initialized) {
      return; // Already initialized
    }
    this._initialized = true;

    if (kTaskbarIDWin in Cc) {
      this._taskbar = Cc[kTaskbarIDWin].getService(Ci.nsIWinTaskbar);
      if (!this._taskbar.available) {
        // The Windows version is probably too old
        DownloadTaskbarProgressUpdater = null;
        return;
      }
    } else if (kTaskbarIDMac in Cc) {
      this._activeTaskbarProgress = Cc[kTaskbarIDMac].
                                      getService(Ci.nsITaskbarProgress);
    } else {
      DownloadTaskbarProgressUpdater = null;
      return;
    }

    this._taskbarState = Ci.nsITaskbarProgress.STATE_NO_PROGRESS;

    this._dm = Cc["@mozilla.org/download-manager;1"].
               getService(Ci.nsIDownloadManager);
    this._dm.addPrivacyAwareListener(this);

    this._os = Cc["@mozilla.org/observer-service;1"].
               getService(Ci.nsIObserverService);
    this._os.addObserver(this, "quit-application-granted", false);

    this._updateStatus();
    // onBrowserWindowLoad/onDownloadWindowLoad are going to set the active
    // window, so don't do it here.
  },

  /**
   * Unregisters ourselves as a download progress listener.
   */
  _uninit: function DTPU_uninit() {
    this._dm.removeListener(this);
    this._os.removeObserver(this, "quit-application-granted");
    this._activeTaskbarProgress = null;
    this._initialized = false;
  },

  /**
   * This holds a reference to the taskbar progress for the window we're
   * working with. This window would preferably be download window, but can be
   * another window if it isn't open.
   */
  _activeTaskbarProgress: null,

  /// Whether the active window is the download window
  _activeWindowIsDownloadWindow: false,

  /**
   * Sets the active window, and whether it's the download window. This takes
   * care of clearing out the previous active window's taskbar item, updating
   * the taskbar, and setting an onunload listener.
   *
   * @param aWindow
   *        The window to set as active.
   * @param aIsDownloadWindow
   *        Whether this window is a download window.
   */
  _setActiveWindow: function DTPU_setActiveWindow(aWindow, aIsDownloadWindow)
  {
#ifdef XP_WIN
    // Clear out the taskbar for the old active window. (If there was no active
    // window, this is a no-op.)
    this._clearTaskbar();

    this._activeWindowIsDownloadWindow = aIsDownloadWindow;
    if (aWindow) {
      // Get the taskbar progress for this window
      let docShell = aWindow.QueryInterface(Ci.nsIInterfaceRequestor).
                       getInterface(Ci.nsIWebNavigation).
                       QueryInterface(Ci.nsIDocShellTreeItem).treeOwner.
                       QueryInterface(Ci.nsIInterfaceRequestor).
                       getInterface(Ci.nsIXULWindow).docShell;
      let taskbarProgress = this._taskbar.getTaskbarProgress(docShell);
      this._activeTaskbarProgress = taskbarProgress;

      this._updateTaskbar();
      // _onActiveWindowUnload is idempotent, so we don't need to check whether
      // we've already set this before or not.
      aWindow.addEventListener("unload", function () {
        DownloadTaskbarProgressUpdater._onActiveWindowUnload(taskbarProgress);
      }, false);
    }
    else {
      this._activeTaskbarProgress = null;
    }
#endif
  },

  /// Current state displayed on the active window's taskbar item
  _taskbarState: null,
  _totalSize: 0,
  _totalTransferred: 0,

  _shouldSetState: function DTPU_shouldSetState()
  {
#ifdef XP_WIN
    // If the active window is not the download manager window, set the state
    // only if it is normal or indeterminate.
    return this._activeWindowIsDownloadWindow ||
           (this._taskbarState == Ci.nsITaskbarProgress.STATE_NORMAL ||
            this._taskbarState == Ci.nsITaskbarProgress.STATE_INDETERMINATE);
#else
    return true;
#endif
  },

  /**
   * Update the active window's taskbar indicator with the current state. There
   * are two cases here:
   * 1. If the active window is the download window, then we always update
   *    the taskbar indicator.
   * 2. If the active window isn't the download window, then we update only if
   *    the status is normal or indeterminate. i.e. one or more downloads are
   *    currently progressing or in scan mode. If we aren't, then we clear the
   *    indicator.
   */
  _updateTaskbar: function DTPU_updateTaskbar()
  {
    if (!this._activeTaskbarProgress) {
      return;
    }

    if (this._shouldSetState()) {
      this._activeTaskbarProgress.setProgressState(this._taskbarState,
                                                   this._totalTransferred,
                                                   this._totalSize);
    }
    // Clear any state otherwise
    else {
      this._clearTaskbar();
    }
  },

  /**
   * Clear taskbar state. This is needed:
   * - to transfer the indicator off a window before transferring it onto
   *   another one
   * - whenever we don't want to show it for a non-download window.
   */
  _clearTaskbar: function DTPU_clearTaskbar()
  {
    if (this._activeTaskbarProgress) {
      this._activeTaskbarProgress.setProgressState(
        Ci.nsITaskbarProgress.STATE_NO_PROGRESS
      );
    }
  },

  /**
   * Update this._taskbarState, this._totalSize and this._totalTransferred.
   * This is called when the download manager is initialized or when the
   * progress or state of a download changes.
   * We compute the number of active and paused downloads, and the total size
   * and total amount already transferred across whichever downloads we have
   * the data for.
   * - If there are no active downloads, then we don't want to show any
   *   progress.
   * - If the number of active downloads is equal to the number of paused
   *   downloads, then we show a paused indicator if we know the size of at
   *   least one download, and no indicator if we don't.
   * - If the number of active downloads is more than the number of paused
   *   downloads, then we show a "normal" indicator if we know the size of at
   *   least one download, and an indeterminate indicator if we don't.
   */
  _updateStatus: function DTPU_updateStatus()
  {
    let numActive = this._dm.activeDownloadCount + this._dm.activePrivateDownloadCount;
    let totalSize = 0, totalTransferred = 0;

    if (numActive == 0) {
      this._taskbarState = Ci.nsITaskbarProgress.STATE_NO_PROGRESS;
    }
    else {
      let numPaused = 0, numScanning = 0;

      // Enumerate all active downloads
      [this._dm.activeDownloads, this._dm.activePrivateDownloads].forEach(function(downloads) {
        while (downloads.hasMoreElements()) {
          let download = downloads.getNext().QueryInterface(Ci.nsIDownload);
          // Only set values if we actually know the download size
          if (download.percentComplete != -1) {
            totalSize += download.size;
            totalTransferred += download.amountTransferred;
          }
          // We might need to display a paused state, so track this
          if (download.state == this._dm.DOWNLOAD_PAUSED) {
            numPaused++;
          } else if (download.state == this._dm.DOWNLOAD_SCANNING) {
            numScanning++;
          }
        }
      }.bind(this));

      // If all downloads are paused, show the progress as paused, unless we
      // don't have any information about sizes, in which case we don't
      // display anything
      if (numActive == numPaused) {
        if (totalSize == 0) {
          this._taskbarState = Ci.nsITaskbarProgress.STATE_NO_PROGRESS;
          totalTransferred = 0;
        }
        else {
          this._taskbarState = Ci.nsITaskbarProgress.STATE_PAUSED;
        }
      }
      // If at least one download is not paused, and we don't have any
      // information about download sizes, display an indeterminate indicator
      else if (totalSize == 0 || numActive == numScanning) {
        this._taskbarState = Ci.nsITaskbarProgress.STATE_INDETERMINATE;
        totalSize = 0;
        totalTransferred = 0;
      }
      // Otherwise display a normal progress bar
      else {
        this._taskbarState = Ci.nsITaskbarProgress.STATE_NORMAL;
      }
    }

    this._totalSize = totalSize;
    this._totalTransferred = totalTransferred;
  },

  /**
   * Called when a window that at one point has been an active window is
   * closed. If this window is currently the active window, we need to look for
   * another window and make that our active window.
   *
   * This function is idempotent, so multiple calls for the same window are not
   * a problem.
   *
   * @param aTaskbarProgress
   *        The taskbar progress for the window that is being unloaded.
   */
  _onActiveWindowUnload: function DTPU_onActiveWindowUnload(aTaskbarProgress)
  {
    if (this._activeTaskbarProgress == aTaskbarProgress) {
      let windowMediator = Cc["@mozilla.org/appshell/window-mediator;1"].
                           getService(Ci.nsIWindowMediator);
      let windows = windowMediator.getEnumerator(null);
      let newActiveWindow = null;
      if (windows.hasMoreElements()) {
        newActiveWindow = windows.getNext().QueryInterface(Ci.nsIDOMWindow);
      }

      // We aren't ever going to reach this point while the download manager is
      // open, so it's safe to assume false for the second operand
      this._setActiveWindow(newActiveWindow, false);
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIDownloadProgressListener

  /**
   * Update status if a download's progress has changed.
   */
  onProgressChange: function DTPU_onProgressChange()
  {
    this._updateStatus();
    this._updateTaskbar();
  },

  /**
   * Update status if a download's state has changed.
   */
  onDownloadStateChange: function DTPU_onDownloadStateChange()
  {
    this._updateStatus();
    this._updateTaskbar();
  },

  onSecurityChange: function() { },

  onStateChange: function() { },

  observe: function DTPU_observe(aSubject, aTopic, aData) {
    if (aTopic == "quit-application-granted") {
      this._uninit();
    }
  }
};
