import path from "path";
import fs from "fs";
import typescript from "rollup-plugin-typescript2";
import dts from "rollup-plugin-dts";
import pkg from "./package.json";
import { nodeResolve } from '@rollup/plugin-node-resolve';

const LICENSE = fs.readFileSync("LICENSE", { encoding: "utf-8" });
const banner = [
  "/*!",
  ...LICENSE.split("\n").map(o => ` * ${o}`),
  " */",
  "",
].join("\n");
const input = "src/index.ts";

export default [
  {
    input,
    plugins: [
      typescript({
        check: true,
        clean: true,
        tsconfigOverride: {
          compilerOptions: {
            module: "ES2015",
            removeComments: true,
          }
        },
      }),
      nodeResolve(),
    ],
    output: [
      {
        banner,
        file: pkg.main,
        format: "cjs",
      },
      {
        banner,
        file: pkg.module,
        format: "es",
      },
    ],
  },
  {
    input,
    plugins: [
      dts({
        tsconfig: path.resolve(__dirname, "./tsconfig.json")
      })
    ],
    output: [
      {
        banner,
        file: pkg.types,
      }
    ]
  },
];
