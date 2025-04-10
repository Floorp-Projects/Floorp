import { useCallback, useEffect, useState } from "react";
import { useLexicalComposerContext } from "@lexical/react/LexicalComposerContext";
import { $getSelection, $isRangeSelection, FORMAT_TEXT_COMMAND, FORMAT_ELEMENT_COMMAND, $createParagraphNode } from "lexical";
import { $createQuoteNode, $isQuoteNode, $createHeadingNode, $isHeadingNode, HeadingTagType } from "@lexical/rich-text";
import { $isListNode, ListNode, INSERT_UNORDERED_LIST_COMMAND, INSERT_ORDERED_LIST_COMMAND } from "@lexical/list";
import { $setBlocksType } from "@lexical/selection";
import { useTranslation } from "react-i18next";
import {
    Bold,
    Italic,
    Underline,
    Strikethrough,
    List,
    ListOrdered,
    AlignLeft,
    AlignCenter,
    AlignRight,
    Quote,
    Heading1,
    Heading2,
    Heading3
} from "lucide-react";

export const Toolbar = () => {
    const { t } = useTranslation();
    const [editor] = useLexicalComposerContext();
    const [activeFormats, setActiveFormats] = useState<{
        bold: boolean;
        italic: boolean;
        underline: boolean;
        strikethrough: boolean;
        align: 'left' | 'center' | 'right' | null;
        list: 'bullet' | 'number' | null;
        heading: HeadingTagType | null;
        quote: boolean;
    }>({
        bold: false,
        italic: false,
        underline: false,
        strikethrough: false,
        align: null,
        list: null,
        heading: null,
        quote: false,
    });

    useEffect(() => {
        return editor.registerUpdateListener(({ editorState }) => {
            editorState.read(() => {
                const selection = $getSelection();
                if (!$isRangeSelection(selection)) {
                    setActiveFormats({
                        bold: false,
                        italic: false,
                        underline: false,
                        strikethrough: false,
                        align: null,
                        list: null,
                        heading: null,
                        quote: false,
                    });
                    return;
                }

                const format = selection.format;
                const element = selection.anchor.getNode().getParent();
                const listNode = element ? $isListNode(element) : false;
                const listType = listNode ? (element as ListNode).getListType() : null;
                const headingNode = element ? $isHeadingNode(element) : false;
                const headingType = headingNode ? (element as any).getTag() : null;
                const quoteNode = element ? $isQuoteNode(element) : false;

                setActiveFormats({
                    bold: (format & 1) === 1,
                    italic: (format & 2) === 2,
                    underline: (format & 4) === 4,
                    strikethrough: (format & 8) === 8,
                    align: element?.getFormatType() as 'left' | 'center' | 'right' | null,
                    list: listType === 'bullet' || listType === 'number' ? listType : null,
                    heading: headingType,
                    quote: quoteNode,
                });
            });
        });
    }, [editor]);

    const formatText = useCallback((command: 'bold' | 'italic' | 'underline' | 'strikethrough') => {
        editor.update(() => {
            const selection = $getSelection();
            if ($isRangeSelection(selection) && !selection.isCollapsed()) {
                editor.dispatchCommand(FORMAT_TEXT_COMMAND, command);
            }
        });
    }, [editor]);

    const formatElement = useCallback((command: 'left' | 'center' | 'right') => {
        editor.update(() => {
            const selection = $getSelection();
            if ($isRangeSelection(selection) && !selection.isCollapsed()) {
                editor.dispatchCommand(FORMAT_ELEMENT_COMMAND, command);
            }
        });
    }, [editor]);

    const formatHeading = useCallback((type: HeadingTagType) => {
        editor.update(() => {
            const selection = $getSelection();
            if (!$isRangeSelection(selection)) return;

            const element = selection.anchor.getNode().getParent();
            const isHeading = element ? $isHeadingNode(element) : false;
            const currentType = isHeading ? (element as any).getTag() : null;

            if (isHeading && currentType === type) {
                $setBlocksType(selection, () => $createParagraphNode());
            } else {
                $setBlocksType(selection, () => $createHeadingNode(type));
            }
        });
    }, [editor]);

    const toggleQuote = useCallback(() => {
        editor.update(() => {
            const selection = $getSelection();
            if (!$isRangeSelection(selection)) return;

            const element = selection.anchor.getNode().getParent();
            const isQuote = element ? $isQuoteNode(element) : false;

            if (isQuote) {
                $setBlocksType(selection, () => $createParagraphNode());
            } else {
                $setBlocksType(selection, () => $createQuoteNode());
            }
        });
    }, [editor]);

    const toggleUnOrderList = useCallback(() => {
        editor.dispatchCommand(INSERT_UNORDERED_LIST_COMMAND, undefined);
    }, [editor]);

    const toggleOrderList = useCallback(() => {
        editor.dispatchCommand(INSERT_ORDERED_LIST_COMMAND, undefined);
    }, [editor]);

    return (
        <>
            <div className="flex flex-col gap-1 p-1">
                <div className="flex flex-wrap gap-1">
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.heading === 'h1' ? 'btn-primary' : 'btn-ghost'}`}
                        onClick={() => formatHeading('h1')}
                        aria-label={t("editor.heading1")}
                    >
                        <Heading1 className="h-4 w-4" />
                    </button>
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.heading === 'h2' ? 'btn-primary' : 'btn-ghost'}`}
                        onClick={() => formatHeading('h2')}
                        aria-label={t("editor.heading2")}
                    >
                        <Heading2 className="h-4 w-4" />
                    </button>
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.heading === 'h3' ? 'btn-primary' : 'btn-ghost'}`}
                        onClick={() => formatHeading('h3')}
                        aria-label={t("editor.heading3")}
                    >
                        <Heading3 className="h-4 w-4" />
                    </button>
                    <div className="divider divider-horizontal mx-0"></div>
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.bold ? 'btn-primary' : 'btn-ghost'}`}
                        onClick={() => formatText('bold')}
                        aria-label={t("editor.bold")}
                    >
                        <Bold className="h-4 w-4" />
                    </button>
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.italic ? 'btn-primary' : 'btn-ghost'}`}
                        onClick={() => formatText('italic')}
                        aria-label={t("editor.italic")}
                    >
                        <Italic className="h-4 w-4" />
                    </button>
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.underline ? 'btn-primary' : 'btn-ghost'}`}
                        onClick={() => formatText('underline')}
                        aria-label={t("editor.underline")}
                    >
                        <Underline className="h-4 w-4" />
                    </button>
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.strikethrough ? 'btn-primary' : 'btn-ghost'}`}
                        onClick={() => formatText('strikethrough')}
                        aria-label={t("editor.strikethrough")}
                    >
                        <Strikethrough className="h-4 w-4" />
                    </button>
                </div>
                <div className="flex flex-wrap gap-1">
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.list === 'bullet' ? 'btn-primary' : 'btn-ghost'}`}
                        onClick={toggleUnOrderList}
                        aria-label={t("editor.bulletList")}
                    >
                        <List className="h-4 w-4" />
                    </button>
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.list === 'number' ? 'btn-primary' : 'btn-ghost'}`}
                        onClick={toggleOrderList}
                        aria-label={t("editor.numberedList")}
                    >
                        <ListOrdered className="h-4 w-4" />
                    </button>
                    <div className="divider divider-horizontal mx-0"></div>
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.align === 'left' ? 'btn-primary' : 'btn-ghost'}`}
                        onClick={() => formatElement('left')}
                        aria-label={t("editor.alignLeft")}
                    >
                        <AlignLeft className="h-4 w-4" />
                    </button>
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.align === 'center' ? 'btn-primary' : 'btn-ghost'}`}
                        onClick={() => formatElement('center')}
                        aria-label={t("editor.alignCenter")}
                    >
                        <AlignCenter className="h-4 w-4" />
                    </button>
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.align === 'right' ? 'btn-primary' : 'btn-ghost'}`}
                        onClick={() => formatElement('right')}
                        aria-label={t("editor.alignRight")}
                    >
                        <AlignRight className="h-4 w-4" />
                    </button>
                    <div className="divider divider-horizontal mx-0"></div>
                    <button
                        type="button"
                        className={`btn btn-sm ${activeFormats.quote ? 'btn-primary' : 'btn-ghost'}`}
                        onClick={toggleQuote}
                        aria-label={t("editor.quote")}
                    >
                        <Quote className="h-4 w-4" />
                    </button>
                </div>
            </div>
            <div className="divider my-0"></div>
        </>
    );
};