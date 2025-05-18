/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createSignal, onCleanup, onMount } from "solid-js";
import { render } from "@nora/solid-xul";
import { CSSEntry } from "./cssEntry.ts";
import i18next from "i18next";
import { addI18nObserver } from "../../../i18n/config.ts";

const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);

/**
 * Chrome CSS Management Service
 */
export class ChromeCSSService {
  readonly AGENT_SHEET = Ci.nsIStyleSheetService.AGENT_SHEET;
  readonly USER_SHEET = Ci.nsIStyleSheetService.USER_SHEET;
  readonly AUTHOR_SHEET = Ci.nsIStyleSheetService.AUTHOR_SHEET;

  readCSS: Record<string, CSSEntry> = {};
  initialized = false;
  menuContainer: HTMLElement | null = null;
  keysetContainer: HTMLElement | null = null;

  constructor() {}

  private get document(): Document {
    return document!;
  }

  private getElement(id: string): HTMLElement | null {
    return this.document.getElementById(id) as HTMLElement | null;
  }

  private createElement(tagName: string): HTMLElement {
    return this.document.createElement(tagName);
  }

  init(): void {
    if (this.initialized) return;

    this.renderMenu();
    this.rebuild();
    this.initialized = true;

    // Handle window unload
    const handleUnload = () => this.uninit();
    window.addEventListener("unload", handleUnload);
    onCleanup(() => {
      window.removeEventListener("unload", handleUnload);
    });
  }

  uninit(): void {
    // Save list of disabled CSS entries
    const disabledList = Object.keys(this.readCSS)
      .filter((key) => !this.readCSS[key].enabled)
      .join("|");

    Services.prefs.setStringPref(
      "UserCSSLoader.disabled_list",
      encodeURIComponent(disabledList),
    );
  }

  renderMenu(): void {
    const mainMenubar = this.getElement("main-menubar");
    if (!mainMenubar) return;

    render(() => <CSSMenu service={this} />, mainMenubar);

    // Render keyset
    if (this.keysetContainer) {
      render(
        () => <CSSKeyset onRebuild={() => this.rebuild()} />,
        this.keysetContainer,
      );
    }
  }

  getCSSFolder(): string {
    let folderPath = Services.prefs.getStringPref("UserCSSLoader.FOLDER", "");
    if (!folderPath) {
      folderPath = PathUtils.join(
        Services.dirsvc.get("ProfD", Ci.nsIFile).path,
        "chrome",
        "CSS",
      );
    }
    return folderPath;
  }

  async rebuild(): Promise<void> {
    const cssExtension = /\.css$/i;
    const excludeUcCss = /\.uc\.css/i;

    const cssFolder = this.getCSSFolder();

    try {
      // Create folder if it doesn't exist
      if (!(await IOUtils.exists(cssFolder))) {
        await IOUtils.makeDirectory(cssFolder);
      }

      // Get list of files in folder
      const fileList = await IOUtils.getChildren(cssFolder);

      // Process each file
      for (const filePath of fileList) {
        const fileName = filePath.replace(cssFolder, "").replace(/^[/\\]/, "");
        if (!cssExtension.test(fileName) || excludeUcCss.test(fileName)) {
          continue;
        }

        // Load CSS file
        const cssFile = this.loadCSS(fileName, cssFolder);
        cssFile.flag = true;
      }

      // Handle deleted files
      for (const leafName of Object.keys(this.readCSS)) {
        const cssFile = this.readCSS[leafName];
        if (!cssFile.flag) {
          cssFile.enabled = false;
          delete this.readCSS[leafName];
        }
        delete cssFile.flag;
      }

      // Redraw menu
      this.renderCSSItems();

      // Show rebuild complete message in status bar
      if (this.initialized) {
        try {
          const rebuildCompleteMsg = i18next.t("chrome_css.rebuild_complete") ??
            "CSS files rebuilt";
          if (typeof window.StatusPanel !== "undefined") {
            window.StatusPanel._label = rebuildCompleteMsg;
          } else if (typeof window.XULBrowserWindow !== "undefined") {
            window.XULBrowserWindow.statusTextField.label = rebuildCompleteMsg;
          }
        } catch (error) {
          console.debug("Status display not available:", error);
        }
      }
    } catch (error) {
      console.error("Error rebuilding CSS files:", error);
    }
  }

  loadCSS(fileName: string, folder: string): CSSEntry {
    let cssFile = this.readCSS[fileName];

    if (!cssFile) {
      // Create new CSS entry
      cssFile = this.readCSS[fileName] = new CSSEntry(fileName, folder);

      // Get state from disabled list
      const disabledList = decodeURIComponent(
        Services.prefs.getStringPref("UserCSSLoader.disabled_list", ""),
      );
      cssFile.enabled = !disabledList.includes(fileName);
    } else if (cssFile.enabled) {
      // Enable existing entry
      cssFile.enabled = true;
    }

    return cssFile;
  }

  renderCSSItems(): void {
    const menupopup = this.getElement("usercssloader-menupopup");
    if (!menupopup) return;

    // Clear existing CSS menu items
    const existingItems = menupopup.querySelectorAll(".usercssloader-css-item");
    existingItems.forEach((item: Element) => item.remove());

    // Recreate CSS menu items
    const cssItems = this.createElement("div");
    cssItems.id = "usercssloader-css-items";
    menupopup.appendChild(cssItems);

    // Prepare CSS file list
    const cssFiles = Object.keys(this.readCSS).map((name) => ({
      name,
      entry: this.readCSS[name],
    }));

    // Render CSS items using SolidJS
    try {
      render(() => (
        <>
          {cssFiles.map(({ name, entry }) => (
            <CSSItem
              fileName={name}
              enabled={entry.enabled}
              sheetType={this.getSheetClassName(entry)}
              onToggle={() => this.toggle(name)}
              onEdit={() =>
                this.edit(PathUtils.join(this.getCSSFolder(), name))}
              service={this}
            />
          ))}
        </>
      ), cssItems);
    } catch (error) {
      console.error("Error rendering CSS items:", error);
    }
  }

  getSheetClassName(cssFile: CSSEntry): string {
    if (cssFile.SHEET === this.AGENT_SHEET) {
      return "AGENT_SHEET";
    } else if (cssFile.SHEET === this.AUTHOR_SHEET) {
      return "AUTHOR_SHEET";
    }
    return "USER_SHEET";
  }

  toggle(fileName: string): void {
    const cssFile = this.readCSS[fileName];
    if (!cssFile) return;

    try {
      cssFile.enabled = !cssFile.enabled;
      setTimeout(() => {
        this.renderCSSItems();
      }, 0);
    } catch (error) {
      console.error("Error toggling CSS:", error);
    }
  }

  openFolder(): void {
    try {
      const file = FileUtils.File(this.getCSSFolder());
      file.launch();
    } catch (error) {
      console.error("Error opening CSS folder:", error);
    }
  }

  editUserCSS(fileName: string): void {
    try {
      const path = PathUtils.join(
        Services.dirsvc.get("ProfD", Ci.nsIFile).path,
        "chrome",
        fileName,
      );
      this.edit(path);
    } catch (error) {
      console.error("Error editing user CSS:", error);
    }
  }

  async edit(filePath: string): Promise<void> {
    let editorPath = Services.prefs.getStringPref(
      "view_source.editor.path",
      "",
    );

    if (!editorPath) {
      editorPath = await this.getDefaultEditorPath();

      const textEditorPath = { value: editorPath };
      const titleMsg = i18next.t("chrome_css.editor_path_not_found") ??
        "Editor Path Not Found";
      const descMsg = i18next.t("chrome_css.set_pref_description") ??
        "Please set the editor path";
      const confirmed = Services.prompt.prompt(
        window as mozIDOMWindow,
        titleMsg,
        descMsg,
        textEditorPath,
        "",
        { value: false },
      );

      if (confirmed) {
        Services.prefs.setStringPref(
          "view_source.editor.path",
          textEditorPath.value,
        );
        editorPath = textEditorPath.value;
      }
    }

    // Open file in editor
    this.openFileInEditor(filePath, editorPath);
  }

  async getDefaultEditorPath(): Promise<string> {
    let editorPath = "";

    if (AppConstants.platform === "win") {
      editorPath = "C:\\windows\\system32\\notepad.exe";

      const vscodePath = `${
        Services.dirsvc.get("Home", Ci.nsIFile).path
      }\\AppData\\Local\\Programs\\Microsoft VS Code\\code.exe`;
      if (await IOUtils.exists(vscodePath)) {
        editorPath = vscodePath;
      }
    } else {
      const geditPath = "/usr/bin/gedit";
      if (await IOUtils.exists(geditPath)) {
        editorPath = geditPath;
      }
    }

    return editorPath;
  }

  openFileInEditor(filePath: string, editorPath: string): void {
    try {
      // Handle path encoding (platform dependent)
      const path = AppConstants.platform === "win"
        ? this.convertUTF8ToShiftJIS(filePath)
        : filePath;

      // Launch editor process
      const app = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
      app.initWithPath(editorPath);

      const process = Cc["@mozilla.org/process/util;1"].createInstance(
        Ci.nsIProcess,
      );
      process.init(app);
      process.run(false, [path], 1);
    } catch (error) {
      console.error("Error opening editor:", error);
    }
  }

  // Character encoding utility (for Windows)
  convertUTF8ToShiftJIS(utf8String: string): string {
    try {
      const decoder = new TextDecoder("utf-8");
      const utf8Bytes = new TextEncoder().encode(utf8String);
      const shiftJISString = decoder.decode(utf8Bytes);
      return shiftJISString;
    } catch (error) {
      console.error("Error converting UTF-8 to Shift-JIS:", error);
      return utf8String;
    }
  }

  async create(fileName?: string): Promise<void> {
    if (!fileName) {
      // Prompt for filename
      const promptMsg = i18next.t("chrome_css.please_enter_filename") ??
        "Please enter a filename";
      const userInput = prompt(
        promptMsg,
        new Date().getTime().toString(),
      );

      if (userInput === null) return;
      fileName = userInput;
    }

    // Normalize filename (handle spaces and special characters)
    fileName = fileName
      .replace(/\s+/g, " ")
      .replace(/[\\/:*?"<>|]/g, "");

    if (!fileName || !/\S/.test(fileName)) return;

    // Add .css extension
    if (!fileName.endsWith(".css")) {
      fileName += ".css";
    }

    // Use PathUtils.join for OS-independent path construction
    const filePath = PathUtils.join(this.getCSSFolder(), fileName);

    try {
      // Create empty CSS file
      await IOUtils.writeUTF8(filePath, "");

      // Open file in editor
      this.edit(filePath);

      // Update menu
      await this.rebuild();
    } catch (error) {
      console.error("Error creating CSS file:", error);
    }
  }
}

/**
 * CSS Menu Component
 */
const CSSMenu = (props: { service: ChromeCSSService }) => {
  const { service } = props;

  // Translation data for localization
  const [translations, setTranslations] = createSignal({
    menu: i18next.t("chrome_css.menu"),
    rebuild: i18next.t("chrome_css.rebuild"),
    create: i18next.t("chrome_css.create"),
    openFolder: i18next.t("chrome_css.open_folder"),
    editUserChrome: i18next.t("chrome_css.edit_user_chrome"),
    editUserContent: i18next.t("chrome_css.edit_user_content"),
  });

  // Update translations when language changes
  addI18nObserver(() => {
    setTranslations({
      menu: i18next.t("chrome_css.menu"),
      rebuild: i18next.t("chrome_css.rebuild"),
      create: i18next.t("chrome_css.create"),
      openFolder: i18next.t("chrome_css.open_folder"),
      editUserChrome: i18next.t("chrome_css.edit_user_chrome"),
      editUserContent: i18next.t("chrome_css.edit_user_content"),
    });
  });

  // Safe event handler function
  const safeHandler = (callback: () => void) => (event: Event) => {
    event.preventDefault();
    event.stopPropagation();
    // Delay execution to avoid blocking UI thread
    setTimeout(() => {
      try {
        callback();
      } catch (error) {
        console.error("Error in menu handler:", error);
      }
    }, 0);
  };

  return (
    <xul:menu label={translations().menu} id="usercssloader-menu">
      <xul:menupopup id="usercssloader-menupopup">
        <xul:menu label={translations().menu}>
          <xul:menupopup id="usercssloader-submenupopup">
            <xul:menuitem
              label={translations().rebuild}
              id="usercssloader-rebuild"
              onClick={safeHandler(() => service.rebuild())}
            />
            <xul:menuseparator />
            <xul:menuitem
              label={translations().create}
              id="usercssloader-create"
              onClick={safeHandler(() => service.create())}
            />
            <xul:menuitem
              label={translations().openFolder}
              id="usercssloader-open-folder"
              onClick={safeHandler(() => service.openFolder())}
            />
            <xul:menuitem
              label={translations().editUserChrome}
              id="usercssloader-edit-chrome"
              onClick={safeHandler(() => service.editUserCSS("userChrome.css"))}
            />
            <xul:menuitem
              label={translations().editUserContent}
              id="usercssloader-edit-content"
              onClick={safeHandler(() =>
                service.editUserCSS("userContent.css")
              )}
            />
          </xul:menupopup>
        </xul:menu>
        <div id="usercssloader-css-items">
          {/* CSS items will be rendered here */}
        </div>
      </xul:menupopup>
    </xul:menu>
  );
};

/**
 * CSS Item Component
 */
const CSSItem = (props: {
  fileName: string;
  enabled: boolean;
  sheetType: string;
  onToggle: () => void;
  onEdit: () => void;
  service: ChromeCSSService;
}) => {
  const { fileName, enabled, sheetType, onToggle, service } = props;

  // Safe menu operation handling
  const safeToggle = (event?: Event) => {
    if (event) {
      event.preventDefault();
      event.stopPropagation();
    }
    // Delay execution to avoid blocking UI thread
    setTimeout(() => {
      try {
        onToggle();
      } catch (error) {
        console.error("Error in toggle handler:", error);
      }
    }, 0);
  };

  const handleClick = (event: MouseEvent) => {
    if (event.button === 0) return;

    event.preventDefault();
    event.stopPropagation();

    if (event.button === 1) {
      // Middle click: toggle enabled/disabled
      safeToggle();
    } else if (event.button === 2) {
      // Right click: open in editor
      if (event.target instanceof Element) {
        if (typeof window.closeMenus === "function") {
          window.closeMenus(event.target);
        }
      }
      // Delay execution to avoid blocking UI thread
      setTimeout(() => {
        try {
          // Use PathUtils.join for OS-independent path construction
          service.edit(PathUtils.join(service.getCSSFolder(), fileName));
        } catch (error) {
          console.error("Error opening editor:", error);
        }
      }, 0);
    }
  };

  return (
    <xul:menuitem
      label={fileName}
      id={`usercssloader-${fileName}`}
      type="checkbox"
      checked={enabled}
      class={`usercssloader-item usercssloader-css-item ${sheetType}`}
      onClick={(e) => handleClick(e)}
      onCommand={safeToggle}
    />
  );
};

/**
 * Keyset Component
 */
const CSSKeyset = (props: { onRebuild: () => void }) => {
  const { onRebuild } = props;

  // Register event listeners
  onMount(() => {
    const handleRebuild = () => onRebuild();
    document!.addEventListener("css-rebuild", handleRebuild);

    onCleanup(() => {
      document!.removeEventListener("css-rebuild", handleRebuild);
    });
  });

  return (
    <xul:key
      id="usercssloader-rebuild-key"
      key="R"
      modifiers="alt"
      command="document.dispatchEvent(new CustomEvent('css-rebuild'))"
    />
  );
};
