/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { LoginHelper } from "resource://gre/modules/LoginHelper.sys.mjs";
import { DataSourceBase } from "resource://gre/modules/megalist/aggregator/datasources/DataSourceBase.sys.mjs";
import { LoginCSVImport } from "resource://gre/modules/LoginCSVImport.sys.mjs";
import { LoginExport } from "resource://gre/modules/LoginExport.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  LoginBreaches: "resource:///modules/LoginBreaches.sys.mjs",
  MigrationUtils: "resource:///modules/MigrationUtils.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "BREACH_ALERTS_ENABLED",
  "signon.management.page.breach-alerts.enabled",
  false
);

/**
 * Data source for Logins.
 *
 * Each login is represented by 3 lines: origin, username and password.
 *
 * Protypes are used to reduce memory need because for different records
 * similar lines will differ in values only.
 */
export class LoginDataSource extends DataSourceBase {
  #originPrototype;
  #usernamePrototype;
  #passwordPrototype;
  #loginsDisabledMessage;
  #enabled;
  #header;
  #exportPasswordsStrings;

  constructor(...args) {
    super(...args);
    // Wait for Fluent to provide strings before loading data
    this.localizeStrings({
      headerLabel: "passwords-section-label",
      originLabel: "passwords-origin-label",
      usernameLabel: "passwords-username-label",
      passwordLabel: "passwords-password-label",
      passwordsDisabled: "passwords-disabled",
      passwordOSAuthDialogCaption: "passwords-os-auth-dialog-caption",
      passwordsImportFilePickerTitle: "passwords-import-file-picker-title",
      passwordsImportFilePickerImportButton:
        "passwords-import-file-picker-import-button",
      passwordsImportFilePickerCsvFilterTitle:
        "passwords-import-file-picker-csv-filter-title",
      passwordsImportFilePickerTsvFilterTitle:
        "passwords-import-file-picker-tsv-filter-title",
      exportPasswordsOSReauthMessage: this.getPlatformFtl(
        "passwords-export-os-auth-dialog-message"
      ),
      passwordsExportFilePickerTitle: "passwords-export-file-picker-title",
      passwordsExportFilePickerDefaultFileName:
        "passwords-export-file-picker-default-filename",
      passwordsExportFilePickerExportButton:
        "passwords-export-file-picker-export-button",
      passwordsExportFilePickerCsvFilterTitle:
        "passwords-export-file-picker-csv-filter-title",
      dismissBreachCommandLabel: "passwords-dismiss-breach-alert-command",
    }).then(strings => {
      const copyCommand = { id: "Copy", label: "command-copy" };
      const editCommand = { id: "Edit", label: "command-edit" };
      const deleteCommand = { id: "Delete", label: "command-delete" };
      const dismissBreachCommand = {
        id: "DismissBreach",
        label: strings.dismissBreachCommandLabel,
      };
      const noOriginSticker = { type: "error", label: "😾 Missing origin" };
      const noPasswordSticker = { type: "error", label: "😾 Missing password" };
      const breachedSticker = { type: "warning", label: "BREACH" };
      const vulnerableSticker = { type: "risk", label: "🤮 Vulnerable" };
      this.#loginsDisabledMessage = strings.passwordsDisabled;
      this.#header = this.createHeaderLine(strings.headerLabel);
      this.#header.commands.push(
        { id: "Create", label: "passwords-command-create" },
        {
          id: "ImportFromBrowser",
          label: "passwords-command-import-from-browser",
        },
        { id: "Import", label: "passwords-command-import" },
        { id: "Export", label: "passwords-command-export" },
        { id: "RemoveAll", label: "passwords-command-remove-all" },
        { id: "Settings", label: "passwords-command-settings" },
        { id: "Help", label: "passwords-command-help" }
      );
      this.#header.executeImport = async () =>
        this.#importFromFile(
          strings.passwordsImportFilePickerTitle,
          strings.passwordsImportFilePickerImportButton,
          strings.passwordsImportFilePickerCsvFilterTitle,
          strings.passwordsImportFilePickerTsvFilterTitle
        );

      this.#header.executeImportFromBrowser = () => this.#importFromBrowser();
      this.#header.executeRemoveAll = () => this.#removeAllPasswords();
      this.#header.executeSettings = () => this.#openPreferences();
      this.#header.executeHelp = () => this.#getHelp();
      this.#header.executeExport = async () => this.#exportAllPasswords();
      this.#exportPasswordsStrings = {
        OSReauthMessage: strings.exportPasswordsOSReauthMessage,
        OSAuthDialogCaption: strings.passwordOSAuthDialogCaption,
        ExportFilePickerTitle: strings.passwordsExportFilePickerTitle,
        FilePickerExportButton: strings.passwordsExportFilePickerExportButton,
        FilePickerDefaultFileName:
          strings.passwordsExportFilePickerDefaultFileName.concat(".csv"),
        FilePickerCsvFilterTitle:
          strings.passwordsExportFilePickerCsvFilterTitle,
      };

      this.#originPrototype = this.prototypeDataLine({
        label: { value: strings.originLabel },
        start: { value: true },
        value: {
          get() {
            return this.record.displayOrigin;
          },
        },
        valueIcon: {
          get() {
            return `page-icon:${this.record.origin}`;
          },
        },
        href: {
          get() {
            return this.record.origin;
          },
        },
        commands: {
          *value() {
            yield { id: "Open", label: "command-open" };
            yield copyCommand;
            yield "-";
            yield deleteCommand;

            if (this.breached) {
              yield dismissBreachCommand;
            }
          },
        },
        executeDismissBreach: {
          value() {
            lazy.LoginBreaches.recordBreachAlertDismissal(this.record.guid);
            delete this.breached;
            this.refreshOnScreen();
          },
        },
        executeCopy: {
          value() {
            this.copyToClipboard(this.record.origin);
          },
        },
        executeDelete: {
          value() {
            this.setLayout({ id: "remove-login" });
          },
        },
        stickers: {
          *value() {
            if (this.isEditing() && !this.editingValue.length) {
              yield noOriginSticker;
            }

            if (this.breached) {
              yield breachedSticker;
            }
          },
        },
      });
      this.#usernamePrototype = this.prototypeDataLine({
        label: { value: strings.usernameLabel },
        value: {
          get() {
            return this.editingValue ?? this.record.username;
          },
        },
        commands: { value: [copyCommand, editCommand, "-", deleteCommand] },
        executeEdit: {
          value() {
            this.editingValue = this.record.username ?? "";
            this.refreshOnScreen();
          },
        },
        executeSave: {
          value(value) {
            try {
              const modifiedLogin = this.record.clone();
              modifiedLogin.username = value;
              Services.logins.modifyLogin(this.record, modifiedLogin);
            } catch (error) {
              //todo
              console.error("failed to modify login", error);
            }
            this.executeCancel();
          },
        },
      });
      this.#passwordPrototype = this.prototypeDataLine({
        label: { value: strings.passwordLabel },
        concealed: { value: true, writable: true },
        end: { value: true },
        value: {
          get() {
            return (
              this.editingValue ??
              (this.concealed ? "••••••••" : this.record.password)
            );
          },
        },
        stickers: {
          *value() {
            if (this.isEditing() && !this.editingValue.length) {
              yield noPasswordSticker;
            }

            if (this.vulnerable) {
              yield vulnerableSticker;
            }
          },
        },
        commands: {
          *value() {
            if (this.concealed) {
              yield { id: "Reveal", label: "command-reveal", verify: true };
            } else {
              yield { id: "Conceal", label: "command-conceal" };
            }
            yield { ...copyCommand, verify: true };
            yield editCommand;
            yield "-";
            yield deleteCommand;
          },
        },
        executeReveal: {
          value() {
            this.concealed = false;
            this.refreshOnScreen();
          },
        },
        executeConceal: {
          value() {
            this.concealed = true;
            this.refreshOnScreen();
          },
        },
        executeCopy: {
          value() {
            this.copyToClipboard(this.record.password);
          },
        },
        executeEdit: {
          value() {
            this.editingValue = this.record.password ?? "";
            this.refreshOnScreen();
          },
        },
        executeSave: {
          value(value) {
            if (!value) {
              return;
            }

            try {
              const modifiedLogin = this.record.clone();
              modifiedLogin.password = value;
              Services.logins.modifyLogin(this.record, modifiedLogin);
            } catch (error) {
              //todo
              console.error("failed to modify login", error);
            }
            this.executeCancel();
          },
        },
      });

      Services.obs.addObserver(this, "passwordmgr-storage-changed");
      Services.prefs.addObserver("signon.rememberSignons", this);
      Services.prefs.addObserver(
        "signon.management.page.breach-alerts.enabled",
        this
      );
      Services.prefs.addObserver(
        "signon.management.page.vulnerable-passwords.enabled",
        this
      );
      this.#reloadDataSource();
    });
  }

  async #importFromFile(title, buttonLabel, csvTitle, tsvTitle) {
    const { BrowserWindowTracker } = ChromeUtils.importESModule(
      "resource:///modules/BrowserWindowTracker.sys.mjs"
    );
    const browsingContext = BrowserWindowTracker.getTopWindow().browsingContext;
    let { result, path } = await this.openFilePickerDialog(
      title,
      buttonLabel,
      [
        {
          title: csvTitle,
          extensionPattern: "*.csv",
        },
        {
          title: tsvTitle,
          extensionPattern: "*.tsv",
        },
      ],
      browsingContext
    );

    if (result != Ci.nsIFilePicker.returnCancel) {
      try {
        const summary = await LoginCSVImport.importFromCSV(path);
        const counts = { added: 0, modified: 0, no_change: 0, error: 0 };

        for (const item of summary) {
          counts[item.result] += 1;
        }
        const l10nArgs = Object.values(counts).map(count => ({ count }));

        this.setLayout({
          id: "import-logins",
          l10nArgs,
        });
      } catch (e) {
        this.setLayout({ id: "import-error" });
      }
    }
  }

  async openFilePickerDialog(
    title,
    okButtonLabel,
    appendFilters,
    browsingContext
  ) {
    return new Promise(resolve => {
      let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
      fp.init(browsingContext, title, Ci.nsIFilePicker.modeOpen);
      for (const appendFilter of appendFilters) {
        fp.appendFilter(appendFilter.title, appendFilter.extensionPattern);
      }
      fp.appendFilters(Ci.nsIFilePicker.filterAll);
      fp.okButtonLabel = okButtonLabel;
      fp.open(async result => {
        resolve({ result, path: fp.file.path });
      });
    });
  }

  #importFromBrowser() {
    const { BrowserWindowTracker } = ChromeUtils.importESModule(
      "resource:///modules/BrowserWindowTracker.sys.mjs"
    );
    const browser = BrowserWindowTracker.getTopWindow().gBrowser;
    try {
      lazy.MigrationUtils.showMigrationWizard(browser.ownerGlobal, {
        entrypoint: lazy.MigrationUtils.MIGRATION_ENTRYPOINTS.PASSWORDS,
      });
    } catch (ex) {
      console.error(ex);
    }
  }

  #removeAllPasswords() {
    let count = 0;
    let currentRecord;
    for (const line of this.lines) {
      if (line.record != currentRecord) {
        count += 1;
        currentRecord = line.record;
      }
    }

    this.setLayout({ id: "remove-logins", l10nArgs: [{ count }] });
  }

  confirmRemoveAll() {
    Services.logins.removeAllLogins();
    this.cancelDialog();
  }

  confirmRemoveLogin([record]) {
    Services.logins.removeLogin(record);
    this.cancelDialog();
  }

  confirmRetryImport() {
    this.#header.executeImport();
    this.cancelDialog();
  }

  #exportAllPasswords() {
    this.setLayout({ id: "export-logins" });
  }

  async confirmExportLogins() {
    const { BrowserWindowTracker } = ChromeUtils.importESModule(
      "resource:///modules/BrowserWindowTracker.sys.mjs"
    );
    const browsingContext = BrowserWindowTracker.getTopWindow().browsingContext;

    const isOSAuthEnabled = LoginHelper.getOSAuthEnabled(
      LoginHelper.OS_AUTH_FOR_PASSWORDS_PREF
    );

    let { isAuthorized, telemetryEvent } = await LoginHelper.requestReauth(
      browsingContext,
      isOSAuthEnabled,
      null, // Prompt regardless of a recent prompt
      this.#exportPasswordsStrings.OSReauthMessage,
      this.#exportPasswordsStrings.OSAuthDialogCaption
    );

    let { method, object, extra = {}, value = null } = telemetryEvent;
    Services.telemetry.recordEvent("pwmgr", method, object, value, extra);

    if (!isAuthorized) {
      this.cancelDialog();
      return;
    }
    this.exportFilePickerDialog(browsingContext);
    this.cancelDialog();
  }

  exportFilePickerDialog(browsingContext) {
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    function fpCallback(aResult) {
      if (aResult != Ci.nsIFilePicker.returnCancel) {
        LoginExport.exportAsCSV(fp.file.path);
        Services.telemetry.recordEvent(
          "pwmgr",
          "mgmt_menu_item_used",
          "export_complete"
        );
      }
    }
    fp.init(
      browsingContext,
      this.#exportPasswordsStrings.ExportFilePickerTitle,
      Ci.nsIFilePicker.modeSave
    );
    fp.appendFilter(
      this.#exportPasswordsStrings.FilePickerCsvFilterTitle,
      "*.csv"
    );
    fp.appendFilters(Ci.nsIFilePicker.filterAll);
    fp.defaultString = this.#exportPasswordsStrings.FilePickerDefaultFileName;
    fp.defaultExtension = "csv";
    fp.okButtonLabel = this.#exportPasswordsStrings.FilePickerExportButton;
    fp.open(fpCallback);
  }

  #openPreferences() {
    const { BrowserWindowTracker } = ChromeUtils.importESModule(
      "resource:///modules/BrowserWindowTracker.sys.mjs"
    );
    const browser = BrowserWindowTracker.getTopWindow().gBrowser;
    browser.ownerGlobal.openPreferences("privacy-logins");
  }

  #getHelp() {
    const { BrowserWindowTracker } = ChromeUtils.importESModule(
      "resource:///modules/BrowserWindowTracker.sys.mjs"
    );
    const browser = BrowserWindowTracker.getTopWindow().gBrowser;
    const SUPPORT_URL =
      Services.urlFormatter.formatURLPref("app.support.baseURL") +
      "password-manager-remember-delete-edit-logins";
    browser.ownerGlobal.openWebLinkIn(SUPPORT_URL, "tab", {
      relatedToCurrent: true,
    });
  }

  /**
   * Enumerate all the lines provided by this data source.
   *
   * @param {string} searchText used to filter data
   */
  *enumerateLines(searchText) {
    if (this.#enabled === undefined) {
      // Async Fluent API makes it possible to have data source waiting
      // for the localized strings, which can be detected by undefined in #enabled.
      return;
    }

    yield this.#header;
    if (this.#header.collapsed || !this.#enabled) {
      return;
    }

    const stats = { count: 0, total: 0 };
    searchText = searchText.toUpperCase();
    yield* this.enumerateLinesForMatchingRecords(
      searchText,
      stats,
      login =>
        login.displayOrigin.toUpperCase().includes(searchText) ||
        login.username.toUpperCase().includes(searchText) ||
        login.password.toUpperCase().includes(searchText)
    );

    this.formatMessages({
      id:
        stats.count == stats.total
          ? "passwords-count"
          : "passwords-filtered-count",
      args: stats,
    }).then(([headerLabel]) => {
      this.#header.value = headerLabel;
    });
  }

  /**
   * Sync lines array with the actual data source.
   * This function reads all logins from the storage, adds or updates lines and
   * removes lines for the removed logins.
   */
  async #reloadDataSource() {
    this.#enabled = Services.prefs.getBoolPref("signon.rememberSignons");
    if (!this.#enabled) {
      this.#reloadEmptyDataSource();
      return;
    }

    const logins = await LoginHelper.getAllUserFacingLogins();
    this.beforeReloadingDataSource();

    const breachesMap = lazy.BREACH_ALERTS_ENABLED
      ? await lazy.LoginBreaches.getPotentialBreachesByLoginGUID(logins)
      : new Map();

    logins.forEach(login => {
      // Similar domains will be grouped together
      // www. will have least effect on the sorting
      const parts = login.displayOrigin.split(".");

      // Exclude TLD domain
      //todo support eTLD and use public suffix here https://publicsuffix.org
      if (parts.length > 1) {
        parts.length -= 1;
      }
      const domain = parts.reverse().join(".");
      const lineId = `${domain}:${login.username}:${login.guid}`;

      let originLine = this.addOrUpdateLine(
        login,
        lineId + "0",
        this.#originPrototype
      );
      this.addOrUpdateLine(login, lineId + "1", this.#usernamePrototype);
      let passwordLine = this.addOrUpdateLine(
        login,
        lineId + "2",
        this.#passwordPrototype
      );

      originLine.breached = breachesMap.has(login.guid);
      passwordLine.vulnerable = lazy.LoginBreaches.isVulnerablePassword(login);
    });

    this.afterReloadingDataSource();
  }

  #reloadEmptyDataSource() {
    this.lines.length = 0;
    //todo: user can enable passwords by activating Passwords header line
    this.#header.value = this.#loginsDisabledMessage;
    this.refreshAllLinesOnScreen();
  }

  observe(_subj, topic, message) {
    if (
      topic == "passwordmgr-storage-changed" ||
      message == "signon.rememberSignons" ||
      message == "signon.management.page.breach-alerts.enabled" ||
      message == "signon.management.page.vulnerable-passwords.enabled"
    ) {
      this.#reloadDataSource();
    }
  }
}
