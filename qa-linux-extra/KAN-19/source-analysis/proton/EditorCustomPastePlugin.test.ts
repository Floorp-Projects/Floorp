import { type BeforePasteEvent, type IEditor, PasteType, PluginEventType } from 'roosterjs-editor-types';

import EditorCustomPastePlugin from './EditorCustomPastePlugin';

const DEFAULT_SANITIZING_OPTIONS = {
    attributeCallbacks: {},
    additionalAllowedAttributes: [],
    additionalTagReplacements: {},
    elementCallbacks: {},
    cssStyleCallbacks: {},
    additionalAllowedCssClasses: [],
    additionalDefaultStyleValues: {},
    additionalGlobalStyleNodes: [],
    additionalPredefinedCssForElement: {},
    preserveHtmlComments: false,
    unknownTagReplacement: null,
};

const createMockSelection = (text: string) => ({
    collapsed: false,
    cloneContents: () => ({ textContent: text }),
});

describe('EditorCustomPastePlugin', () => {
    let plugin: EditorCustomPastePlugin;
    let mockEditor: Pick<IEditor, 'getSelectionRange' | 'deleteSelectedContent' | 'insertNode'>;
    let mockOnPasteFiles: jest.Mock;

    beforeEach(() => {
        mockOnPasteFiles = jest.fn();
        mockEditor = {
            getSelectionRange: jest.fn(),
            deleteSelectedContent: jest.fn(),
            insertNode: jest.fn(),
        };

        plugin = new EditorCustomPastePlugin(mockOnPasteFiles);
        plugin.initialize(mockEditor as IEditor);
    });

    const createPasteEvent = (text: string, types: string[]): BeforePasteEvent => {
        const fragment = document.createDocumentFragment();
        Object.defineProperty(fragment, 'textContent', {
            get: () => '',
            set: () => {},
        });

        return {
            eventType: PluginEventType.BeforePaste,
            clipboardData: {
                text,
                types,
                rawHtml: null,
                image: null,
                customValues: {},
            },
            fragment,
            sanitizingOption: DEFAULT_SANITIZING_OPTIONS,
            htmlBefore: '',
            htmlAfter: '',
            htmlAttributes: {},
            pasteType: PasteType.Normal,
        };
    };

    describe('handleUrlPaste', () => {
        it('should return false when no text is selected', () => {
            const event = createPasteEvent('https://example.com', ['text/plain']);
            mockEditor.getSelectionRange = jest.fn().mockReturnValue(null);
            plugin.onPluginEvent(event);
            expect(mockEditor.insertNode).not.toHaveBeenCalled();
        });

        it('should return false when pasted content is not a valid URL', () => {
            const event = createPasteEvent('not-a-url', ['text/plain']);
            mockEditor.getSelectionRange = jest.fn().mockReturnValue(createMockSelection('selected text'));
            plugin.onPluginEvent(event);
            expect(mockEditor.insertNode).not.toHaveBeenCalled();
        });

        it('should return false when URL protocol is not allowed', () => {
            const event = createPasteEvent('ftp://example.com', ['text/plain']);
            mockEditor.getSelectionRange = jest.fn().mockReturnValue(createMockSelection('selected text'));
            plugin.onPluginEvent(event);
            expect(mockEditor.insertNode).not.toHaveBeenCalled();
        });

        it('should convert selected text to link when valid URL is pasted', () => {
            const event = createPasteEvent('https://example.com', ['text/plain']);
            mockEditor.getSelectionRange = jest.fn().mockReturnValue(createMockSelection('selected text'));
            plugin.onPluginEvent(event);
            expect(mockEditor.deleteSelectedContent).toHaveBeenCalled();
            expect(mockEditor.insertNode).toHaveBeenCalledWith(
                expect.objectContaining({
                    tagName: 'A',
                    href: 'https://example.com/',
                    textContent: 'selected text',
                    target: '_blank',
                    rel: 'noopener noreferrer',
                })
            );
        });

        it('should return false when clipboard contains non plain text type', () => {
            const event = createPasteEvent('<a href="https://example.com">Link</a>', ['text/html']);
            plugin.onPluginEvent(event);
            expect(mockEditor.insertNode).not.toHaveBeenCalled();
        });
    });
});
