import type { JSX } from "solid-js";
import { createRenderer } from "solid-js/universal";

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
    if (node instanceof Element) {
      if (value instanceof Function) {
        //the eventListener name is on~~~
        //so have to remove the `on`
        const evName = name.slice(2).toLowerCase();
        node.addEventListener(evName, value);
      } else if (typeof value === "object" && name === "style") {
        let tmp = "";
        for (const [idx, v] of Object.entries(value)) {
          tmp += `${idx}:${v};`;
        }
        node.setAttribute(name, tmp);
      } else if (typeof value === "string") {
        node.setAttribute(name, value);
      }
    } else {
      node[name] = value;
    }
  },
  insertNode: (parent: Node, node: JSX.Element, anchor?: Node): void => {
    parent.insertBefore(node, anchor ?? null);
  },
  removeNode: (parent: Node, node: JSX.Element): void => {
    parent.removeChild(node);
  },
  getParentNode: (node: Node): ParentNode => {
    return node.parentNode;
  },
  getFirstChild: (node: Node): ChildNode => {
    return node.firstChild;
  },
  getNextSibling: (node: Node): Node => {
    return node.nextSibling;
  },
});
