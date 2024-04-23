/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { LoginHelper } from "resource://gre/modules/LoginHelper.sys.mjs";
import { DataSourceBase } from "resource://gre/modules/megalist/aggregator/datasources/DataSourceBase.sys.mjs";
import { LoginCSVImport } from "resource://gre/modules/LoginCSVImport.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  LoginBreaches: "resource:///modules/LoginBreaches.sys.mjs",
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

  constructor(...args) {
    super(...args);
    // Wait for Fluent to provide strings before loading data
    this.localizeStrings({
      headerLabel: "passwords-section-label",
      originLabel: "passwords-origin-label",
      usernameLabel: "passwords-username-label",
      passwordLabel: "passwords-password-label",
      openCommandLabel: "command-open",
      copyCommandLabel: "command-copy",
      revealCommandLabel: "command-reveal",
      concealCommandLabel: "command-conceal",
      passwordsDisabled: "passwords-disabled",
      deleteCommandLabel: "command-delete",
      editCommandLabel: "command-edit",
      passwordsCreateCommandLabel: "passwords-command-create",
      passwordsImportCommandLabel: "passwords-command-import",
      passwordsExportCommandLabel: "passwords-command-export",
      passwordsRemoveAllCommandLabel: "passwords-command-remove-all",
      passwordsSettingsCommandLabel: "passwords-command-settings",
      passwordsHelpCommandLabel: "passwords-command-help",
      passwordsImportFilePickerTitle: "passwords-import-file-picker-title",
      passwordsImportFilePickerImportButton:
        "passwords-import-file-picker-import-button",
      passwordsImportFilePickerCsvFilterTitle:
        "passwords-import-file-picker-csv-filter-title",
      passwordsImportFilePickerTsvFilterTitle:
        "passwords-import-file-picker-tsv-filter-title",
      dismissBreachCommandLabel: "passwords-dismiss-breach-alert-command",
    }).then(strings => {
      const copyCommand = { id: "Copy", label: strings.copyCommandLabel };
      const editCommand = { id: "Edit", label: strings.editCommandLabel };
      const deleteCommand = { id: "Delete", label: strings.deleteCommandLabel };
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
        { id: "Create", label: strings.passwordsCreateCommandLabel },
        { id: "Import", label: strings.passwordsImportCommandLabel },
        { id: "Export", label: strings.passwordsExportCommandLabel },
        { id: "RemoveAll", label: strings.passwordsRemoveAllCommandLabel },
        { id: "Settings", label: strings.passwordsSettingsCommandLabel },
        { id: "Help", label: strings.passwordsHelpCommandLabel }
      );
      this.#header.executeImport = async () => {
        await this.#importFromFile(
          strings.passwordsImportFilePickerTitle,
          strings.passwordsImportFilePickerImportButton,
          strings.passwordsImportFilePickerCsvFilterTitle,
          strings.passwordsImportFilePickerTsvFilterTitle
        );
      };
      this.#header.executeRemoveAll = () => {
        this.removeAllPasswords();
      };
      this.#header.executeSettings = () => {
        this.#openPreferences();
      };
      this.#header.executeHelp = () => {
        this.#getHelp();
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
            yield { id: "Open", label: strings.openCommandLabel };
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
          get() {
            const commands = [
              { id: "Conceal", label: strings.concealCommandLabel },
              {
                id: "Copy",
                label: strings.copyCommandLabel,
                verify: true,
              },
              editCommand,
              "-",
              deleteCommand,
            ];
            if (this.concealed) {
              commands[0] = {
                id: "Reveal",
                label: strings.revealCommandLabel,
                verify: true,
              };
            }
            return commands;
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
      let summary;
      try {
        summary = await LoginCSVImport.importFromCSV(path);
      } catch (e) {
        // TODO: Display error for import
      }
      if (summary) {
        // TODO: Display successful import summary
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

  removeAllPasswords() {
    let total = 0;
    let currentRecord;
    for (const line of this.lines) {
      if (line.record != currentRecord) {
        total += 1;
        currentRecord = line.record;
      }
    }

    const data = { total };
    this.setLayout({ id: "remove-logins", data });
  }

  confirmRemoveAll() {
    Services.logins.removeAllLogins();
    this.cancelDialog();
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
