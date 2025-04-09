import { LexicalComposer } from "@lexical/react/LexicalComposer";
import { ContentEditable } from "@lexical/react/LexicalContentEditable";
import { editorConfig } from "./config.ts";
import { Toolbar } from "./Toolbar.tsx";

import { RichTextPlugin } from "@lexical/react/LexicalRichTextPlugin";
import { HistoryPlugin } from "@lexical/react/LexicalHistoryPlugin";
import { OnChangePlugin } from "@lexical/react/LexicalOnChangePlugin";
import { ListPlugin } from "@lexical/react/LexicalListPlugin";
import { CheckListPlugin } from "@lexical/react/LexicalCheckListPlugin";
import { SerializedEditorState, SerializedLexicalNode } from "lexical";
import { useEffect, useRef } from "react";
import { useLexicalComposerContext } from "@lexical/react/LexicalComposerContext";

interface RichTextEditorProps {
    onChange: (editorState: SerializedEditorState<SerializedLexicalNode>) => void;
    initialContent?: string;
}

export const RichTextEditor = ({ onChange, initialContent }: RichTextEditorProps) => {
    return (
        <LexicalComposer initialConfig={editorConfig}>
            <EditorContent onChange={onChange} initialContent={initialContent} />
        </LexicalComposer>
    );
};

const EditorContent = ({ onChange, initialContent }: RichTextEditorProps) => {
    const [editor] = useLexicalComposerContext();
    const isInitialized = useRef(false);

    useEffect(() => {
        if (initialContent && !isInitialized.current) {
            const parsedContent = JSON.parse(initialContent);
            editor.setEditorState(editor.parseEditorState(parsedContent));
            isInitialized.current = true;
        }
    }, [editor]);

    return (
        <div className="flex flex-col h-full">
            <Toolbar />
            <div className="flex-1 overflow-auto p-4">
                <RichTextPlugin
                    contentEditable={
                        <ContentEditable className="h-full outline-none" />
                    }
                    ErrorBoundary={({ children }) => children}
                />
            </div>
            {onChange && (
                <OnChangePlugin
                    onChange={(editorState) => {
                        editorState.read(() => {
                            const content = editorState.toJSON();
                            onChange(content);
                        });
                    }}
                />
            )}
            <ListPlugin />
            <CheckListPlugin />
            <HistoryPlugin />
        </div>
    );
};