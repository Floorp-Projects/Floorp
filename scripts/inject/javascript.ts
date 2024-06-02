import * as fs from "node:fs/promises";

const NORA_COMMENT_START = "/*@nora|INJECT|START*/";
const NORA_COMMENT_END = "/*@nora|INJECT|END*/";

import swc, {
  type ModuleItem,
  type FunctionDeclaration,
  type ExpressionStatement,
  type CallExpression,
  type Expression,
  type Identifier,
  type Span,
} from "@swc/core";

function checkMemberExprId(
  left: Expression | Identifier,
  right: Expression | Identifier,
): boolean {
  if (left.type === "MemberExpression" && right.type === "MemberExpression") {
    return (
      left.property.type === "Identifier" &&
      right.property.type === "Identifier" &&
      left.property.value === right.property.value &&
      checkMemberExprId(left.object, right.object)
    );
  }
  if (left.type === "Identifier" && right.type === "Identifier") {
    return left.value === right.value;
  }
  return false;
}

function checkSWCExpr(original: ModuleItem[], find: ModuleItem): Span {
  if (find.type === "FunctionDeclaration") {
    const funcDecls = original.filter(
      (v) =>
        //? check is it function and same name
        v.type === find.type && v.identifier.value === find.identifier.value,
    ) as FunctionDeclaration[];
    const find_stmt = find.body?.stmts[0];
    for (const funcDecl of funcDecls) {
      if (
        find_stmt?.type === "ExpressionStatement" &&
        find_stmt.expression.type === "CallExpression"
      ) {
        const stmts = (
          funcDecl.body?.stmts.filter(
            (v) => v.type === find_stmt?.type,
          ) as ExpressionStatement[]
        ).filter((v) => v.expression.type === find_stmt.expression.type);
        if (!stmts) throw Error("No Statements");
        for (const stmt of stmts) {
          const expr = stmt.expression as CallExpression;
          if (
            expr.callee.type === "MemberExpression" &&
            find_stmt.expression.callee.type === "MemberExpression" &&
            checkMemberExprId(expr.callee, find_stmt.expression.callee)
          ) {
            return stmt.span;
          }
        }
      } else {
        throw Error("Expected ExpressionStatement in find");
      }
    }
  }
  throw Error("Not Supported Expression");
}

async function injectToJs(
  path: string,
  findingExpr: string,
  injectStr: string,
) {
  const str = (await fs.readFile(path))
    .toString()
    .replaceAll(
      /\/\*@nora\|INJECT\|START\*\/[\s\S]*\/\*@nora\|INJECT\|END\*\//g,
      "",
    );
  const output = swc.parseSync(str);

  const idx = checkSWCExpr(output.body, swc.parseSync(findingExpr).body[0]).end;

  await fs.writeFile(
    path,
    str.slice(0, idx) +
      NORA_COMMENT_START +
      injectStr +
      NORA_COMMENT_END +
      str.slice(idx),
  );
}

export async function injectJavascript() {
  await Promise.all([
    injectToJs(
      "dist/bin/browser/chrome/browser/content/browser/preferences/preferences.js",
      "function init_all(){Services.telemetry.setEventRecordingEnabled()}",
      `register_module("paneCsk", {init(){}});`,
    ),
  ]);
}
