import { HeadingNode, QuoteNode } from "@lexical/rich-text";
import { ListItemNode, ListNode } from "@lexical/list";

export const editorConfig = {
  namespace: "FloorpNotes",
  onError(error: Error) {
    console.error(error);
  },
  theme: {
    paragraph: "mb-2",
    quote: "border-l-4 border-base-300 pl-4 my-2",
    heading: {
      h1: "text-2xl font-bold mb-2",
      h2: "text-xl font-bold mb-2",
      h3: "text-lg font-bold mb-2",
    },
    list: {
      ul: "list-disc pl-4 mb-2",
      ol: "list-decimal pl-4 mb-2",
    },
  },
  nodes: [HeadingNode, QuoteNode, ListNode, ListItemNode],
};
