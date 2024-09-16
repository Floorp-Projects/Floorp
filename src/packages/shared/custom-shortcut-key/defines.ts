import { z } from "zod";
import { commands } from "./commands";

//? https://github.com/colinhacks/zod/discussions/839#discussioncomment-6488540
function zodEnumFromObjKeys<K extends string>(
  obj: Record<K, unknown>,
): z.ZodEnum<[K, ...K[]]> {
  const [firstKey, ...otherKeys] = Object.keys(obj) as K[];
  return z.enum([firstKey, ...otherKeys]);
}

export const keyCodesList = {
  // F num keys
  F1: ["F1", "VK_F1"],
  F2: ["F2", "VK_F2"],
  F3: ["F3", "VK_F3"],
  F4: ["F4", "VK_F4"],
  F5: ["F5", "VK_F5"],
  F6: ["F6", "VK_F6"],
  F7: ["F7", "VK_F7"],
  F8: ["F8", "VK_F8"],
  F9: ["F9", "VK_F9"],
  F10: ["F10", "VK_F10"],
  F11: ["F11", "VK_F11"],
  F12: ["F12", "VK_F12"],
  F13: ["F13", "VK_F13"],
  F14: ["F14", "VK_F14"],
  F15: ["F15", "VK_F15"],
  F16: ["F16", "VK_F16"],
  F17: ["F17", "VK_F17"],
  F18: ["F18", "VK_F18"],
  F19: ["F19", "VK_F19"],
  F20: ["F20", "VK_F20"],
  F21: ["F21", "VK_F21"],
  F22: ["F22", "VK_F22"],
  F23: ["F23", "VK_F23"],
  F24: ["F24", "VK_F24"],
};

const zFloorpCSKData = z.array(
  z.object({
    actionName: z.string(),
    key: z.string(),
    keyCode: z.string(),
    modifiers: z.string(),
  }),
);

type FloorpCSKData = z.infer<typeof zFloorpCSKData>;

export function floorpCSKToNora(data: FloorpCSKData): CSKData {
  const arr = [];
  for (const datum of data) {
    arr.push({
      command: datum.actionName,
      //? keyCode is VK_F1 ~ VK_F24 so should convert to F1 ~ F24
      key: datum.keyCode ? datum.keyCode.replace("VK_", "") : datum.key,
      modifiers: {
        alt: datum.modifiers.includes("alt"),
        ctrl: datum.modifiers.includes("ctrl"),
        meta: false,
        shift: datum.modifiers.includes("shift"),
      },
    });
  }
  return zCSKData.parse(arr);
}

export const zCSKData = z.record(
  zodEnumFromObjKeys(commands),
  z.object({
    modifiers: z.object({
      alt: z.boolean(),
      ctrl: z.boolean(),
      meta: z.boolean(),
      shift: z.boolean(),
    }),
    key: z.string(),
  }),
);

export type CSKData = z.infer<typeof zCSKData>;

export const zCSKCommands = z.union([
  z.object({
    type: z.enum(["disable-csk"]),
    data: z.boolean(),
  }),
  z.object({
    type: z.enum(["update-pref"]),
  }),
]);

export type CSKCommands = z.infer<typeof zCSKCommands>;
