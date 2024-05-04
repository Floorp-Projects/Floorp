import type { JSX } from "solid-js";
import { createRenderer } from "solid-js/universal";
import { z } from "zod";

//TODO: test required
const eventListener = z
  .function()
  .args(z.instanceof(Event))
  .returns(z.void())
  .or(
    z.object({
      handleEvent: z.function().args(z.instanceof(Event)).returns(z.void()),
    }),
  );

const styleObject = z.record(z.string(), z.string());

export const {
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
    const isEventListener = (
      // biome-ignore lint/suspicious/noExplicitAny: <explanation>
      value: any,
      // biome-ignore lint/suspicious/noExplicitAny: <explanation>
    ): value is (this: Element, ev: Event) => any => {
      return value instanceof Function;
    };
    if (node instanceof Element) {
      const resultEvListener = eventListener.safeParse(value);
      const resultStyleObject = styleObject.safeParse(value);
      if (resultEvListener.success) {
        //the eventListener name is on~~~
        //so have to remove the `on`
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
      } else {
        throw Error(
          `unreachable! @nora:solid-xul:setProperty the value is not EventListener, style object, string, number, nor boolean | is ${value}`,
        );
      }
    } else {
      throw Error(
        `unreachable! @nora:solid-xul:setProperty the node is not Element | is ${node}`,
      );
    }
  },
  insertNode: (parent: Node, node: JSX.Element, anchor?: Node): void => {
    console.log(node);
    if (node instanceof Node) {
      parent.insertBefore(node, anchor ?? null);
    } else {
      throw Error("solid-xul insertNode `node` is not Node");
    }
  },
  removeNode: (parent: Node, node: JSX.Element): void => {
    if (node instanceof Node) {
      parent.removeChild(node);
    } else {
      throw Error("solid-xul insertNode `node` is not Element nor XULElement");
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
