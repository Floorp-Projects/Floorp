import * as fs from "node:fs/promises";

const NORA_COMMENT_START = "/*@nora|INJECT|START*/";
const NORA_COMMENT_END = "/*@nora|INJECT|END*/";

import swc from "@swc/core";

type SWCComparative =
  | swc.Expression
  | swc.Identifier
  | swc.PrivateName
  | swc.ComputedPropName
  | swc.Super
  | swc.Import
  | swc.ModuleItem
  | swc.Pattern
  | swc.SpreadElement
  | swc.Property
  | swc.VariableDeclarator
  | swc.Param
  | swc.ClassMember;

function assertSWCMemberExprSameType<T extends SWCComparative>(
  left: T,
  right: SWCComparative,
): right is T {
  return left.type === right.type;
}

function checkMemberExprId(
  left: SWCComparative,
  right: SWCComparative,
): boolean {
  const _assertSame = assertSWCMemberExprSameType;
  switch (left.type) {
    case "MemberExpression": {
      return (
        _assertSame(left, right) &&
        checkMemberExprId(left.property, right.property) &&
        checkMemberExprId(left.object, right.object)
      );
    }
    case "CallExpression": {
      return (
        _assertSame(left, right) &&
        checkMemberExprId(left.callee, right.callee) &&
        left.arguments.length === right.arguments.length &&
        [...left.arguments].every((arg, idx) =>
          checkMemberExprId(arg.expression, right.arguments[idx].expression),
        )
      );
    }
    case "SpreadElement": {
      return (
        _assertSame(left, right) &&
        checkMemberExprId(left.arguments, right.arguments)
      );
    }
    case "Parameter": {
      return _assertSame(left, right) && checkMemberExprId(left.pat, right.pat);
    }
    case "VariableDeclaration": {
      return (
        _assertSame(left, right) &&
        left.kind === right.kind &&
        left.declarations.every((decl, idx) =>
          checkMemberExprId(decl, right.declarations[idx]),
        )
      );
    }
    case "VariableDeclarator": {
      return (
        _assertSame(left, right) &&
        checkMemberExprId(left.id, right.id) &&
        (() => {
          const leftInit = left.init;
          const rightInit = right.init;
          if (!!leftInit && !!rightInit) {
            return checkMemberExprId(leftInit, rightInit);
          }
          return false;
        })()
      );
    }
    case "AssignmentExpression": {
      return (
        _assertSame(left, right) &&
        left.operator === right.operator &&
        checkMemberExprId(left.left, right.left) &&
        checkMemberExprId(left.right, right.right)
      );
    }
    case "ThisExpression": {
      return _assertSame(left, right);
    }
    case "ConditionalExpression": {
      return (
        _assertSame(left, right) &&
        checkMemberExprId(left.test, right.test) &&
        checkMemberExprId(left.consequent, right.consequent) &&
        checkMemberExprId(left.alternate, right.alternate)
      );
    }
    case "Identifier":
    case "StringLiteral":
    case "BooleanLiteral": {
      // if (_assertSame(left, right)) {
      //   console.log(`id left ${left.value} right ${right.value}`);
      // }
      return _assertSame(left, right) && left.value === right.value;
    }
  }
  console.warn(`Not Supported type ${left.type}`);
  return false;
}

function checkSWCStatements(
  original: swc.Statement[],
  find: swc.Statement,
): swc.Span | undefined {
  const _assertSame = assertSWCMemberExprSameType;
  for (const v of original) {
    switch (v.type) {
      case "ExpressionStatement": {
        // console.log("ExpressionStatement!");
        // console.log(v);
        // console.log(find);
        if (!_assertSame(v, find)) {
          continue;
        }
        if (checkMemberExprId(find.expression, v.expression)) {
          return v.span;
        }
        continue;
      }
      case "VariableDeclaration": {
        if (!_assertSame(v, find)) {
          continue;
        }
        if (
          find.kind === v.kind &&
          v.declarations.every((_v, idx) =>
            checkMemberExprId(_v, find.declarations[idx]),
          )
        ) {
          return v.span;
        }
      }
    }
  }
  return undefined;
}

function findIndexOfNoraInject(
  original: swc.Statement[],
  find: swc.Statement[],
): number | undefined {
  let noraInject: "before" | "after" | undefined = undefined;
  let span: swc.Span | undefined = undefined;
  for (const tmp of find) {
    if (
      tmp.type === "ExpressionStatement" &&
      isNoraInjectPlace(tmp.expression)
    ) {
      noraInject = span ? "after" : "before";
    } else {
      // console.log(original);
      console.log(tmp);
      span = checkSWCStatements(original, tmp);
    }
  }
  console.log(`span ${span} no ${noraInject}`);
  if (span && noraInject) {
    return noraInject === "before" ? span.start : span.end;
  }
  return undefined;
}

function isNoraInjectPlace(expression: swc.Expression) {
  return (
    expression.type === "CallExpression" &&
    expression.callee.type === "PrivateName" &&
    expression.callee.id.value === "noraInject"
  );
}

function checkSWCObject(
  expr: swc.ObjectExpression,
  find_expr: swc.ObjectExpression,
): number | undefined {
  const _assertSame = assertSWCMemberExprSameType;
  const find_prop = find_expr.properties[0];
  for (const prop of expr.properties) {
    switch (find_prop.type) {
      case "MethodProperty": {
        //? Method (function in object)
        if (_assertSame(find_prop, prop)) {
          if (checkMemberExprId(find_prop.key, prop.key)) {
            if (
              find_prop.body?.stmts.find(
                (v) =>
                  v.type === "ExpressionStatement" &&
                  isNoraInjectPlace(v.expression),
              )
            ) {
              return findIndexOfNoraInject(
                prop.body!.stmts,
                find_prop.body!.stmts,
              );
            }
            // console.log("aaa");
            return checkSWCExpr(prop.body!.stmts, find_prop.body!.stmts[0]);
          }
        }
      }
    }
  }
  return;
}

function checkSWCExpr(
  original: swc.ModuleItem[],
  find: swc.ModuleItem,
): number | undefined {
  const _assertSame = assertSWCMemberExprSameType;
  //console.log("checkSWCExpr");
  switch (find.type) {
    case "FunctionDeclaration": {
      const funcDecls = original.filter((v) => _assertSame(find, v));
      for (const funcDecl of funcDecls) {
        if (checkMemberExprId(find.identifier, funcDecl.identifier)) {
          const stmts = funcDecl.body!.stmts;
          const find_stmts = find.body!.stmts;
          if (!stmts) {
            throw Error("idk");
          }
          if (
            //? if the #noraInject() found in expression
            find.body?.stmts.find(
              (v) =>
                v.type === "ExpressionStatement" &&
                isNoraInjectPlace(v.expression),
            )
          ) {
            return findIndexOfNoraInject(stmts, find_stmts);
          }
          //? if no the #noraInject found
          return checkSWCExpr(stmts, find_stmts[0]);
        }
      }
      return;
    }
    case "VariableDeclaration": {
      const varDecls = original.filter((v) => _assertSame(find, v));
      const find_stmt = find.declarations[0];
      for (const varDecl of varDecls) {
        if (varDecl.kind === find.kind) {
          for (const decl of varDecl.declarations) {
            if (checkMemberExprId(find_stmt.id, decl.id)) {
              //? Object
              if (
                find_stmt.init?.type === "ObjectExpression" &&
                _assertSame(find_stmt.init, decl.init!)
              ) {
                return checkSWCObject(decl.init, find_stmt.init);
              }
            }
          }
        }
      }
      return;
    }
    case "IfStatement": {
      for (const ifstatement of original) {
        if (_assertSame(find, ifstatement)) {
          if (checkMemberExprId(ifstatement.test, find.test)) {
            if (
              find.consequent.type === "BlockStatement" &&
              _assertSame(find.consequent, ifstatement.consequent)
            ) {
              if (
                find.consequent.stmts.find(
                  (v) =>
                    v.type === "ExpressionStatement" &&
                    isNoraInjectPlace(v.expression),
                )
              ) {
                // console.log(find.consequent);
                return findIndexOfNoraInject(
                  ifstatement.consequent.stmts,
                  find.consequent.stmts,
                );
              }
            }
          }
        }
      }
      return;
    }
    case "BlockStatement": {
      for (const block of original) {
        if (_assertSame(find, block)) {
          return checkSWCExpr(block.stmts, find.stmts[0]);
        }
      }
      return;
    }
    case "ExpressionStatement": {
      for (const expr of original) {
        if (_assertSame(find, expr)) {
          switch (find.expression.type) {
            case "AssignmentExpression": {
              if (_assertSame(find.expression, expr.expression)) {
                //? Object
                if (
                  find.expression.right.type === "ObjectExpression" &&
                  _assertSame(find.expression.right, expr.expression.right)
                ) {
                  return checkSWCObject(
                    expr.expression.right,
                    find.expression.right,
                  );
                }
              }
            }
          }
        }
      }
      return;
    }
    case "ExportDeclaration": {
      for (const decl of original) {
        if (_assertSame(find, decl)) {
          const tmp = checkSWCExpr([decl.declaration], find.declaration);
          if (tmp) {
            return tmp;
          }
        }
      }
      return;
    }
    case "ClassDeclaration": {
      for (const decl of original) {
        if (_assertSame(find, decl)) {
          if (
            checkMemberExprId(decl.identifier, find.identifier) &&
            find.superClass
              ? decl.superClass &&
                checkMemberExprId(decl.superClass, find.superClass)
              : true
          ) {
            const find_member = find.body[0];
            for (const member of decl.body) {
              switch (find_member.type) {
                case "ClassMethod": {
                  if (!_assertSame(find_member, member)) {
                    continue;
                  }

                  if (
                    member.isAbstract === find_member.isAbstract &&
                    member.isOptional === find_member.isAbstract &&
                    member.isOverride === find_member.isOverride &&
                    member.isStatic === find_member.isStatic &&
                    member.accessibility === find_member.accessibility &&
                    checkMemberExprId(member.key, find_member.key) &&
                    member.function.params.every((v, idx) =>
                      checkMemberExprId(v, find_member.function.params[idx]),
                    )
                  ) {
                    // console.log(member.function.body?.stmts);
                    // console.log(find_member.function.body?.stmts);
                    return checkSWCExpr(
                      member.function.body!.stmts,
                      find_member.function.body!.stmts[0],
                    );
                  }
                }
              }
            }
            console.log(find);
          }
        }
      }
      return;
    }
    case "TryStatement": {
      for (const stmt of original) {
        if (_assertSame(find, stmt)) {
          const tmp = findIndexOfNoraInject(stmt.block.stmts, find.block.stmts);
          if (tmp) return tmp;
        }
      }
      return;
    }
    default: {
      console.log(find);
    }
  }
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
  const offset = swc.parseSync("").span.end - 1;
  // console.log(`offset: ${offset}`);
  const output = swc.parseSync(str);

  let idx = checkSWCExpr(
    output.body,
    swc.parseSync(findingExpr, { syntax: "typescript", decorators: true })
      .body[0],
  );
  console.log(idx);
  if (!idx) return;

  idx -= offset;
  idx -= 2;

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
  // console.log(
  //   isNoraInjectPlace(swc.parseSync("#noraInject();").body[0] as swc.Statement),
  // );
  await injectToJs(
    "_dist/bin/browser/chrome/browser/content/browser/preferences/preferences.js",
    `function init_all(){
    Services.telemetry.setEventRecordingEnabled('aboutpreferences', true);
    #noraInject();
  }`,
    `register_module("paneCsk", {init(){}});`,
  );
  await injectToJs(
    "_dist/bin/browser/modules/sessionstore/TabState.sys.mjs",
    `var TabStateInternal = {
      _collectBaseTabData(tab, options) {
        let browser = tab.linkedBrowser;
        #noraInject();
      }
    }`,
    `tabData.floorpDisableHistory = tab.getAttribute("floorp-disablehistory");`,
  );

  await injectToJs(
    "_dist/bin/browser/chrome/browser/content/browser/tabbrowser.js",
    `{
      window._gBrowser = {
        createTabsForSessionRestore(restoreTabsLazily, selectTab, tabDataList) {
          let shouldUpdateForPinnedTabs = false;
          #noraInject();
        }
      };
    }`,
    `for (var i = 0; i < tabDataList.length; i++) {
      const tabData = tabDataList[i];
      if (tabData.floorpDisableHistory) {
        tabDataList.splice(i, 1);
      }
    }`,
  );

  await injectToJs(
    "_dist/bin/chrome/remote/content/webdriver-bidi/WebDriverBidiConnection.sys.mjs",
    `export class WebDriverBiDiConnection extends WebSocketConnection {
      async onPacket(packet) {
        try {
          #noraInject();
          this.sendResult(id, result);
        } catch {}
      }
    }`,
    `if (module === "session" && command === "new") {
      result.capabilities.browserName = "firefox";
    }
    `,
  );
}
