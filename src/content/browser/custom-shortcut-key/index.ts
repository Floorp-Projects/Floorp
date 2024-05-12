import { commands } from "./commands";
import type { CSKData } from "./defines";
import { checkIsSystemShortcut } from "./utils";

export class CustomShortcutKey {
  private static instance: CustomShortcutKey;
  private static windows: Window[] = [];
  static getInstance() {
    if (!CustomShortcutKey.instance) {
      CustomShortcutKey.instance = new CustomShortcutKey();
    }
    if (!CustomShortcutKey.windows.includes(window)) {
      CustomShortcutKey.instance.startHandleShortcut(window);
      CustomShortcutKey.windows.push(window);
      console.log("add window");
    }
    return CustomShortcutKey.instance;
  }

  cskData: CSKData = [];
  private constructor() {
    this.initCSKData();

    console.warn("CSK Init Completed");
  }

  private initCSKData() {
    this.cskData = [
      {
        modifiers: {
          ctrl: true,
          shift: true,
          alt: false,
          meta: false,
        },
        key: "V",
        command: "gecko-open-new-window",
      },
    ];
  }
  private startHandleShortcut(_window: Window) {
    _window.addEventListener("keydown", (ev) => {
      //@ts-expect-error
      if (ev.key === "Escape") {
        return;
      }

      //@ts-expect-error
      if (ev.key === "Tab") {
        return;
      }

      //@ts-expect-error
      if (!ev.altKey && !ev.ctrlKey && !ev.shiftKey && !ev.metaKey) {
        //@ts-expect-error
        if (ev.key === "Delete" || ev.key === "Backspace") {
          // Avoid triggering back-navigation.
          //ev.preventDefault();
          //TODO: reset key input
          return;
        }
      }

      // on register
      //ev.preventDefault();

      if (
        //@ts-expect-error
        ["Control", "Alt", "Meta", "Shift"].filter((k) => ev.key.includes(k))
          .length === 0
      ) {
        if (checkIsSystemShortcut(ev)) {
          console.warn(`This Event is registered in System: ${ev}`);
          return;
        }

        for (const shortcutDatum of this.cskData) {
          const { alt, ctrl, meta, shift } = shortcutDatum.modifiers;
          if (
            //@ts-expect-error
            ev.altKey === alt &&
            //@ts-expect-error
            ev.ctrlKey === ctrl &&
            //@ts-expect-error
            ev.metaKey === meta &&
            //@ts-expect-error
            ev.shiftKey === shift &&
            //@ts-expect-error
            ev.key === shortcutDatum.key
          ) {
            commands[shortcutDatum.command].command(ev);
          }
        }
      }
    });
  }
}
