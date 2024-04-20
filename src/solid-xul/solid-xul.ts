import type { JSX } from "solid-js";
import { createRenderer } from "solid-js/universal";
import type { XULElement } from "@types/gecko/lib.gecko.dom";

export const {
  render,
  effect,
  memo,
  createComponent,
  createElement,
  createTextNode,
  insertNode,
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
        node.addEventListener(name.slice(2).toLowerCase(), value);
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
