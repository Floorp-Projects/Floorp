import { z } from "zod";

export const Sidebar3Panel = z.object({
  url: z.string(),
  width: z.number(),
  remote: z.boolean().default(true),
  usercontext: z.number().nullable().default(null),
  changeUserAgent: z.number().nullable().default(null),
});

export type Sidebar3Panel = z.infer<typeof Sidebar3Panel>;

export const Sidebar3Data = z.object({
  version: z.number().int().min(0).default(3),
  panels: z.record(z.string(), Sidebar3Panel),
});

export type Sidebar3Data = z.infer<typeof Sidebar3Data>;
