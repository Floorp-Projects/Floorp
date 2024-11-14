// import { z } from "zod";
// import { trpc } from "./client";

// const zPrefType = z.union([
//   z.object({
//     prefType: z.literal("boolean"),
//     prefName: z.string(),
//     value: z.boolean(),
//   }),
//   z.object({
//     prefType: z.literal("integer"),
//     prefName: z.string(),
//     value: z.number(),
//   }),
//   z.object({
//     prefType: z.literal("string"),
//     prefName: z.string(),
//     value: z.string(),
//   }),
// ]);

// type PrefType = z.infer<typeof zPrefType>;

// export async function setBoolPref(prefName: string, value: boolean) {
//   return trpc.post.broadcast.mutate({
//     topic: "settings-parent:setPref",
//     data: JSON.stringify(
//       zPrefType.parse({ prefType: "boolean", prefName, value }),
//     ),
//   });
// }
// export async function setIntPref(prefName: string, value: number) {
//   trpc.post.broadcast.mutate({
//     topic: "settings-parent:setPref",
//     data: JSON.stringify(
//       zPrefType.parse({ prefType: "integer", prefName, value }),
//     ),
//   });
// }
// export async function setStringPref(prefName: string, value: string) {
//   trpc.post.broadcast.mutate({
//     topic: "settings-parent:setPref",
//     data: JSON.stringify(
//       zPrefType.parse({ prefType: "string", prefName, value }),
//     ),
//   });
// }

// export async function getBoolPref(prefName: string): Promise<boolean> {
//   return zPrefType.parse(
//     await trpc.post.broadcast.mutate({
//       topic: "settings-parent:getPref",
//       data: JSON.stringify(
//         zPrefType.parse({ prefType: "boolean", prefName, value: false }),
//       ),
//       returnTopic: "settings-child:getPref",
//     }),
//   ).value as boolean;
// }
// export async function getIntPref(prefName: string): Promise<number> {
//   return zPrefType.parse(
//     await trpc.post.broadcast.mutate({
//       topic: "settings-parent:getPref",
//       data: JSON.stringify(
//         zPrefType.parse({ prefType: "integer", prefName, value: 0 }),
//       ),
//       returnTopic: "settings-child:getPref",
//     }),
//   ).value as number;
// }
// export async function getStringPref(prefName: string): Promise<string> {
//   return zPrefType.parse(
//     await trpc.post.broadcast.mutate({
//       topic: "settings-parent:getPref",
//       data: JSON.stringify(
//         zPrefType.parse({ prefType: "string", prefName, value: "" }),
//       ),
//       returnTopic: "settings-child:getPref",
//     }),
//   ).value as string;
// }
// export async function openChromeURL(url: string) {
//  return trpc.post.broadcast.mutate({
//     topic: "settings-parent:openChromeURL",
//     data: JSON.stringify({ url }),
//   });
// }
