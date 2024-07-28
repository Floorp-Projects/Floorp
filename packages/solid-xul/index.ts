import { createRoot, type JSX } from "solid-js";
import { createRenderer } from "solid-js/universal";
import type { ViteHotContext } from "vite/types/hot";
import { z } from "zod";

//TODO: test required
const eventListener = z
  .function()
  .args(z.instanceof(Event))
  .or(
    z.object({
      handleEvent: z.function().args(z.instanceof(Event)),
    }),
  );

const styleObject = z.record(z.string(), z.string());

const hotCtxMap = new Map<ViteHotContext, Array<() => void>>();

const {
  render,
  effect,
  memo,
  createComponent,
  createElement,
  createTextNode,
  insertNode,
  /**
   * insertBefore
   */
  insert,
  spread,
  setProp,
  mergeProps,
} = createRenderer<JSX.Element>({
  createElement: (tag: string): Element => {
    if (tag.startsWith("xul:")) {
      return document.createXULElement(tag.replace("xul:", ""));
    }
    return document.createElement(tag);
  },
  createTextNode: (value: string): Text => {
    return document.createTextNode(value);
  },
  replaceText: (textNode: Text, value: string): void => {
    textNode.data = value;
  },
  isTextNode: (node: Node): boolean => {
    return node.nodeType === 3;
  },
  setProperty: <T>(
    node: JSX.Element,
    name: string,
    value: T,
    prev?: T,
  ): void => {
    if (node instanceof Element) {
      const resultEvListener = eventListener.safeParse(value);
      const resultStyleObject = styleObject.safeParse(value);
      if (resultEvListener.success) {
        //? the eventListener name is on~~~
        //? so have to remove the `on`
        const evName = name.slice(2).toLowerCase();
        node.addEventListener(evName, resultEvListener.data);
      } else if (resultStyleObject.success) {
        let tmp = "";
        for (const [idx, v] of Object.entries(resultStyleObject.data)) {
          tmp += `${idx}:${v};`;
        }
        node.setAttribute(name, tmp);
      } else if (typeof value === "string") {
        node.setAttribute(name, value);
      } else if (typeof value === "number" || typeof value === "boolean") {
        node.setAttribute(name, value.toString());
      } else if (value === undefined) {
        node.removeAttribute(name);
      } else {
        throw Error(
          `unreachable! @nora/solid-xul:setProperty the value is not EventListener, style object, string, number, nor boolean | is ${value}`,
        );
      }
    } else {
      throw Error(
        `unreachable! @nora/solid-xul:setProperty the node is not Element | is ${node}`,
      );
    }
  },
  insertNode: (parent: Node, node: JSX.Element, anchor?: Node): void => {
    //console.log(node);
    if (node instanceof Node) {
      parent.insertBefore(node, anchor ?? null);
    } else {
      throw Error(
        "unreachable! @nora/solid-xul:setProperty insertNode `node` is not Node",
      );
    }
  },
  removeNode: (parent: Node, node: JSX.Element): void => {
    if (node instanceof Node) {
      parent.removeChild(node);
    } else {
      throw Error(
        "unreachable! @nora/solid-xul:setProperty insertNode `node` is not Element nor XULElement",
      );
    }
  },
  getParentNode: (node: Node): JSX.Element => {
    return node.parentNode;
  },
  getFirstChild: (node: Node): JSX.Element => {
    return node.firstChild;
  },
  getNextSibling: (node: Node): JSX.Element => {
    return node.nextSibling;
  },
});

const _render = (
  code: () => JSX.Element,
  node: JSX.Element,
  options?: { marker?: Element; hotCtx?: ViteHotContext },
) => {
  let disposer: () => void = () => {};
  createRoot((dispose) => {
    const elem = insert(node, code(), options ? options.marker : undefined);
    disposer = () => {
      dispose();
      if (elem instanceof Element) {
        elem.remove();
      } else if (
        Array.isArray(elem) &&
        elem.every((e) => e instanceof Element)
      ) {
        elem.forEach((e) => e.remove());
      }
    };
    if (options?.hotCtx) {
      hotCtxMap.set(options.hotCtx, [
        ...(hotCtxMap.get(options.hotCtx) ?? []),
        disposer,
      ]);
      console.log("register disposer to hotCtx");
      options.hotCtx.dispose(() => {
        hotCtxMap.get(options.hotCtx!)?.forEach((v) => v());
        hotCtxMap.delete(options.hotCtx!);
      });
    }
  });

  return disposer;
};

export {
  _render as render,
  effect,
  memo,
  createComponent,
  createElement,
  createTextNode,
  insertNode,
  /**
   * insertBefore
   */
  insert,
  spread,
  setProp,
  mergeProps,
};
