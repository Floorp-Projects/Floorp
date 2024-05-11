import type {} from "@solid-xul/jsx-runtime";
import type { CustomShortcutKey } from ".";
import { For } from "solid-js";
import { commands } from "./commands";

export function keySet(instance: CustomShortcutKey) {
  return (
    <xul:keyset>
      <For each={instance.cskData.get()}>
        {(item) => (
          <xul:key
            id={item.id}
            modifiers={item.modifiers.join(",")}
            key={item.key}
            keycode={item.keycode}
            command={item.command}
          />
        )}
      </For>
    </xul:keyset>
  );
}

export function commandSet() {
  return (
    <xul:commandset>
      <For each={Object.keys(commands)}>
        {(key) => <xul:command id={commands[key]} />}
      </For>
    </xul:commandset>
  );
}
