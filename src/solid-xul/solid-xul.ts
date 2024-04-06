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
  insert,
  spread,
  setProp,
  mergeProps,
} = createRenderer<JSX.Element>({
  createElement: (tag: string): Element => {
    if (tag.startsWith("xul:")) {
      //@ts-expect-error XUL is firefox only
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
      node.setAttribute(name, value as unknown as string);
    } else {
      node[name] = value;
    }
  },
  insertNode: (parent: ParentNode, node: Node, anchor?: Node): void => {
    parent.insertBefore(node, anchor);
  },
  removeNode: (parent: ParentNode, node: Node): void => {
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
