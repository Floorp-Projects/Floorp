/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createRoot, createSignal, onCleanup, onMount } from "solid-js";
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

let instance: ChromeCSSService | null = null;

export class ChromeCSSService {
  readonly AGENT_SHEET = Ci.nsIStyleSheetService.AGENT_SHEET;
  readonly USER_SHEET = Ci.nsIStyleSheetService.USER_SHEET;
  readonly AUTHOR_SHEET = Ci.nsIStyleSheetService.AUTHOR_SHEET;

  readCSS: Record<string, CSSEntry> = {};
  initialized = false;

  private cssFilesSignal = createSignal<
    Array<{ name: string; entry: CSSEntry }>
  >([], { equals: false });

  private dispose: (() => void) | null = null;
  private isCreating = false;

  static getInstance(): ChromeCSSService {
    if (!instance) {
      instance = new ChromeCSSService();
    }
    return instance;
  }

  private constructor() { }

  getCssFiles() {
    return this.cssFilesSignal[0]();
  }

  private setCssFiles(files: Array<{ name: string; entry: CSSEntry }>) {
    this.cssFilesSignal[1](files);
  }

  private get document(): Document {
    return document!;
  }

  private getElement(id: string): XULElement | null {
    return this.document.getElementById(id) as XULElement | null;
  }

  init(): void {
    if (this.initialized) {
      return;
    }

    // Create a target in the DOM for our menu to be rendered
    this.setupMenuTarget();

    // Initial loading of CSS files
    this.rebuild();

    // Mark as initialized
    this.initialized = true;

    // Set up window unload handler
    const handleUnload = () => this.uninit();
    window.addEventListener("unload", handleUnload);
  }

  private setupMenuTarget(): void {
    // Remove any existing Chrome CSS menu elements
    this.cleanupExistingElements();

    // Find the main menubar
    const mainMenubar = this.getElement("main-menubar");
    if (!mainMenubar) {
      console.error("Main menubar not found, cannot setup Chrome CSS menu");
      return;
    }

    // Create a container for the menu
    const menuContainer = this.document.createXULElement(
      "menuitem",
    ) as XULElement;
    menuContainer.id = "usercssloader-placeholder";
    menuContainer.hidden = true;
    mainMenubar.appendChild(menuContainer);

    // Find the main keyset
    const mainKeyset = this.getElement("mainKeyset");
    let keysetContainer = null;

    if (mainKeyset) {
      keysetContainer = this.document.createXULElement("keyset") as XULElement;
      keysetContainer.id = "usercssloader-keyset-placeholder";
      mainKeyset.appendChild(keysetContainer);
    }

    // Create a single reactive root for our application
    createRoot((dispose) => {
      this.dispose = dispose;

      // Render the main Chrome CSS menu
      render(() => <ChromeCSSMenuWrapper service={this} />, mainMenubar);

      // Render the keyset if available
      if (keysetContainer && mainKeyset) {
        render(
          () => <CSSKeyset onRebuild={() => this.rebuild()} />,
          keysetContainer,
        );
      }
    });
  }

  private cleanupExistingElements(): void {
    // Remove any existing Chrome CSS menu elements to prevent duplication
    const existingMenu = this.document.getElementById("usercssloader-menu");
    if (existingMenu && existingMenu.parentNode) {
      existingMenu.parentNode.removeChild(existingMenu);
    }

    const existingPlaceholder = this.document.getElementById(
      "usercssloader-placeholder",
    );
    if (existingPlaceholder && existingPlaceholder.parentNode) {
      existingPlaceholder.parentNode.removeChild(existingPlaceholder);
    }

    const existingKeyset = this.document.getElementById(
      "usercssloader-keyset-placeholder",
    );
    if (existingKeyset && existingKeyset.parentNode) {
      existingKeyset.parentNode.removeChild(existingKeyset);
    }
  }

  uninit(): void {
    if (!this.initialized) {
      return;
    }

    // Save list of disabled CSS entries
    const disabledList = Object.keys(this.readCSS)
      .filter((key) => !this.readCSS[key].enabled)
      .join("|");

    Services.prefs.setStringPref(
      "UserCSSLoader.disabled_list",
      encodeURIComponent(disabledList),
    );

    // Dispose of reactive root
    if (this.dispose) {
      this.dispose();
      this.dispose = null;
    }

    // Clean up DOM elements
    this.cleanupExistingElements();

    // Clear CSS files
    this.setCssFiles([]);

    this.initialized = false;
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
      if (!(await IOUtils.exists(cssFolder))) {
        await IOUtils.makeDirectory(cssFolder);
      }

      const fileList = await IOUtils.getChildren(cssFolder);

      // Reset all flags
      for (const key of Object.keys(this.readCSS)) {
        this.readCSS[key].flag = false;
      }

      for (const filePath of fileList) {
        const fileName = filePath.replace(cssFolder, "").replace(/^[/\\]/, "");
        if (!cssExtension.test(fileName) || excludeUcCss.test(fileName)) {
          continue;
        }

        const cssFile = this.loadCSS(fileName, cssFolder);
        cssFile.flag = true;
      }

      // Process deleted files
      const filesToRemove = [];
      for (const leafName of Object.keys(this.readCSS)) {
        const cssFile = this.readCSS[leafName];
        if (!cssFile.flag) {
          cssFile.enabled = false;
          filesToRemove.push(leafName);
        }
        delete cssFile.flag;
      }

      // Remove deleted files from the collection
      for (const leafName of filesToRemove) {
        delete this.readCSS[leafName];
      }

      // Update the CSS files signal to trigger UI refresh
      this.updateCssFilesList();

      if (this.initialized) {
        try {
          const rebuildCompleteMsg =
            i18next.t("chrome_css.rebuild_complete") ?? "CSS files rebuilt";
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
      cssFile = this.readCSS[fileName] = new CSSEntry(fileName, folder);

      const disabledList = decodeURIComponent(
        Services.prefs.getStringPref("UserCSSLoader.disabled_list", ""),
      );
      cssFile.enabled = !disabledList.includes(fileName);
    } else if (cssFile.enabled) {
      cssFile.enabled = true;
    }

    return cssFile;
  }

  // Update the list of CSS files (will trigger UI refresh due to reactivity)
  updateCssFilesList(): void {
    const cssFiles = Object.keys(this.readCSS).map((name) => ({
      name,
      entry: this.readCSS[name],
    }));

    this.setCssFiles(cssFiles);
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
        this.updateCssFilesList();
      }, 50);
    } catch (error) {
      console.error("Error toggling CSS:", error);
    }
  }

  openFolder(): void {
    try {
      const cssFolder = this.getCSSFolder();

      if (AppConstants.platform === "macosx") {
        // macOS: Use 'open' command to open folder in Finder
        const app = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
        app.initWithPath("/usr/bin/open");

        const process = Cc["@mozilla.org/process/util;1"].createInstance(
          Ci.nsIProcess,
        );
        process.init(app);
        process.run(false, [cssFolder], 1);
      } else {
        // Windows and Linux: Use traditional file.launch()
        const file = FileUtils.File(cssFolder);
        file.launch();
      }
    } catch (error) {
      console.error("Error opening CSS folder:", error);
      // Fallback: Try to use file.launch() regardless of platform
      try {
        const file = FileUtils.File(this.getCSSFolder());
        file.launch();
      } catch (fallbackError) {
        console.error("Fallback folder open also failed:", fallbackError);
      }
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

      // If we found a valid default editor, use it without prompting
      if (editorPath && (await IOUtils.exists(editorPath))) {
        this.openFileInEditor(filePath, editorPath);
        return;
      }

      // Only prompt if no valid editor was found
      const textEditorPath = { value: editorPath };
      const titleMsg =
        i18next.t("chrome_css.editor_path_not_found") ??
        "Editor Path Not Found";
      const descMsg =
        i18next.t("chrome_css.set_pref_description") ??
        "Please set the editor path";
      const confirmed = Services.prompt.prompt(
        window as mozIDOMWindow,
        titleMsg,
        descMsg,
        textEditorPath,
        "",
        { value: false },
      );

      if (confirmed && textEditorPath.value) {
        Services.prefs.setStringPref(
          "view_source.editor.path",
          textEditorPath.value,
        );
        editorPath = textEditorPath.value;
      } else {
        // User cancelled or provided empty path
        return;
      }
    }

    this.openFileInEditor(filePath, editorPath);
  }

  async getDefaultEditorPath(): Promise<string> {
    const editorPath = "";

    if (AppConstants.platform === "win") {
      // Check VS Code first on Windows
      const vscodePath = `${Services.dirsvc.get("Home", Ci.nsIFile).path
        }\\AppData\\Local\\Programs\\Microsoft VS Code\\code.exe`;
      if (await IOUtils.exists(vscodePath)) {
        return vscodePath;
      }

      // Fallback to Notepad (should always exist)
      const notepadPath = "C:\\windows\\system32\\notepad.exe";
      if (await IOUtils.exists(notepadPath)) {
        return notepadPath;
      }
    } else if (AppConstants.platform === "macosx") {
      // macOS: Check for common editors in priority order
      const editors = [
        "/Applications/Visual Studio Code.app/Contents/Resources/app/bin/code",
        "/usr/local/bin/code",
        "/opt/homebrew/bin/code",
        "/Applications/Sublime Text.app/Contents/SharedSupport/bin/subl",
        "/usr/local/bin/subl",
        "/Applications/Atom.app/Contents/Resources/app/atom.sh",
        "/usr/local/bin/atom",
        "/System/Applications/TextEdit.app/Contents/MacOS/TextEdit",
      ];

      for (const editor of editors) {
        if (await IOUtils.exists(editor)) {
          return editor;
        }
      }
    } else {
      // Linux and other Unix-like systems
      const editors = [
        "/usr/bin/code",
        "/usr/local/bin/code",
        "/snap/bin/code",
        "/usr/bin/gedit",
        "/usr/bin/nano",
        "/usr/bin/vim",
      ];

      for (const editor of editors) {
        if (await IOUtils.exists(editor)) {
          return editor;
        }
      }
    }

    // Return empty string if no editor found
    return "";
  }

  openFileInEditor(filePath: string, editorPath: string): void {
    try {
      let path = filePath;
      let args = [path];

      if (AppConstants.platform === "win") {
        path = this.convertUTF8ToShiftJIS(filePath);
        args = [path];
      } else if (AppConstants.platform === "macosx") {
        // macOS: Handle special cases for different editors
        if (editorPath.includes("TextEdit")) {
          // TextEdit requires special handling
          args = [filePath];
        } else if (editorPath.includes("code") || editorPath.includes("Code")) {
          // VS Code
          args = [filePath];
        } else if (editorPath.includes("subl")) {
          // Sublime Text
          args = [filePath];
        } else if (editorPath.includes("atom")) {
          // Atom
          args = [filePath];
        } else {
          // Default: use open command for .app bundles
          if (editorPath.endsWith(".app") || editorPath.includes(".app/")) {
            const openApp = Cc["@mozilla.org/file/local;1"].createInstance(
              Ci.nsIFile,
            );
            openApp.initWithPath("/usr/bin/open");

            const openProcess = Cc[
              "@mozilla.org/process/util;1"
            ].createInstance(Ci.nsIProcess);
            openProcess.init(openApp);
            openProcess.run(false, ["-a", editorPath, filePath], 3);
            return;
          }
        }
      }

      const app = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
      app.initWithPath(editorPath);

      const process = Cc["@mozilla.org/process/util;1"].createInstance(
        Ci.nsIProcess,
      );
      process.init(app);
      process.run(false, args, args.length);
    } catch (error) {
      console.error("Error opening editor:", error);

      // macOS fallback: try using 'open' command
      if (AppConstants.platform === "macosx") {
        try {
          const openApp = Cc["@mozilla.org/file/local;1"].createInstance(
            Ci.nsIFile,
          );
          openApp.initWithPath("/usr/bin/open");

          const openProcess = Cc["@mozilla.org/process/util;1"].createInstance(
            Ci.nsIProcess,
          );
          openProcess.init(openApp);
          openProcess.run(false, ["-e", filePath], 2);
        } catch (fallbackError) {
          console.error(
            "macOS fallback editor open also failed:",
            fallbackError,
          );
        }
      }
    }
  }

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
    if (this.isCreating) {
      return;
    }

    this.isCreating = true;

    try {
      if (!fileName) {
        const promptMsg = i18next.t("chrome_css.please_enter_filename") ??
          "Please enter a filename";
        const userInput = prompt(
          promptMsg,
          new Date().getTime().toString(),
        );

        if (userInput === null) {
          this.isCreating = false;
          return;
        }
        fileName = userInput;
      }

      fileName = fileName
        .replace(/\s+/g, " ")
        .replace(/[\\/:*?"<>|]/g, "");

      if (!fileName || !/\S/.test(fileName)) {
        this.isCreating = false;
        return;
      }

      if (!fileName.endsWith(".css")) {
        fileName += ".css";
      }

      const filePath = PathUtils.join(this.getCSSFolder(), fileName);

      await IOUtils.writeUTF8(filePath, "");
      await this.rebuild();

      // Check if we can open editor automatically
      const editorPath = Services.prefs.getStringPref("view_source.editor.path", "") ||
        await this.getDefaultEditorPath();

      if (editorPath && await IOUtils.exists(editorPath)) {
        // Ask user if they want to open the file for editing
        const openMsg = i18next.t("chrome_css.open_file_in_editor") ??
          `Open ${fileName} in editor?`;
        const shouldOpen = Services.prompt.confirm(
          window as mozIDOMWindow,
          i18next.t("chrome_css.file_created") ?? "File Created",
          openMsg
        );

        if (shouldOpen) {
          this.edit(filePath);
        }
      } else {
        // If no editor is available, just notify that the file was created
        const createdMsg = i18next.t("chrome_css.file_created_successfully") ??
          `File ${fileName} created successfully`;
        Services.prompt.alert(
          window as mozIDOMWindow,
          i18next.t("chrome_css.file_created") ?? "File Created",
          createdMsg
        );
      }
    } catch (error) {
      console.error("Error creating CSS file:", error);
    } finally {
      this.isCreating = false;
    }
  }
}

/**
 * Wrapper component for Chrome CSS Menu
 */
const ChromeCSSMenuWrapper = (props: { service: ChromeCSSService }) => {
  return (
    <xul:menu id="usercssloader-menu" label={i18next.t("chrome_css.menu")}>
      <CSSMenu service={props.service} />
    </xul:menu>
  );
};

/**
 * CSS Menu Component
 */
const CSSMenu = (props: { service: ChromeCSSService }) => {
  const { service } = props;

  const [translations, setTranslations] = createSignal({
    menu: i18next.t("chrome_css.menu"),
    rebuild: i18next.t("chrome_css.rebuild"),
    create: i18next.t("chrome_css.create"),
    openFolder: i18next.t("chrome_css.open_folder"),
    editUserChrome: i18next.t("chrome_css.edit_user_chrome"),
    editUserContent: i18next.t("chrome_css.edit_user_content"),
  });

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

  const safeHandler = (callback: () => void) => (event: Event) => {
    event.preventDefault();
    event.stopPropagation();

    setTimeout(() => {
      try {
        callback();
      } catch (error) {
        console.error("Error in menu handler:", error);
      }
    }, 0);
  };

  return (
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
            onClick={safeHandler(() => service.editUserCSS("userContent.css"))}
          />
        </xul:menupopup>
      </xul:menu>
      <CSSItems service={service} />
    </xul:menupopup>
  );
};

/**
 * CSS Items Component
 */
const CSSItems = (props: { service: ChromeCSSService }) => {
  const { service } = props;

  return (
    <xul:vbox id="usercssloader-css-items">
      {service.getCssFiles().map(({ name, entry }) => (
        <CSSItem
          fileName={name}
          enabled={entry.enabled}
          sheetType={service.getSheetClassName(entry)}
          onToggle={() => service.toggle(name)}
          onEdit={() =>
            service.edit(PathUtils.join(service.getCSSFolder(), name))
          }
          service={service}
        />
      ))}
    </xul:vbox>
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

  const safeToggle = (event?: Event) => {
    if (event) {
      event.preventDefault();
      event.stopPropagation();
    }

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
      safeToggle();
    } else if (event.button === 2) {
      if (event.target instanceof Element) {
        if (typeof window.closeMenus === "function") {
          window.closeMenus(event.target);
        }
      }

      setTimeout(() => {
        try {
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
