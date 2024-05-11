import { render } from "@solid-xul/solid-xul";
import { type Accessor, createSignal, type Setter } from "solid-js";
import { z } from "zod";
import { commands } from "./commands";

//https://github.com/colinhacks/zod/discussions/839#discussioncomment-6488540
function zodEnumFromObjKeys<K extends string>(
  obj: Record<K, unknown>,
): z.ZodEnum<[K, ...K[]]> {
  const [firstKey, ...otherKeys] = Object.keys(obj) as K[];
  return z.enum([firstKey, ...otherKeys]);
}

const zCSKData = z.array(
  z.object({
    id: z.string(),
    modifiers: z.array(z.enum(["shift", "ctrl", "alt", "meta", "accel"])),
    key: z.string(),
    keycode: z.string(),
    command: zodEnumFromObjKeys(commands),
  }),
);

export class CustomShortcutKey {
  private static instance: CustomShortcutKey;
  static getInstance() {
    if (!CustomShortcutKey.instance) {
      CustomShortcutKey.instance = new CustomShortcutKey();
    }
    return CustomShortcutKey.instance;
  }

  cskData: {
    get: Accessor<z.infer<typeof zCSKData>>;
    set: Setter<z.infer<typeof zCSKData>>;
  };
  private constructor() {
    const [getCSKData, setCSKData] = createSignal<z.infer<typeof zCSKData>>([]);
    this.cskData = { get: getCSKData, set: setCSKData };
  }
}
