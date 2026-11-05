import type { RefObject } from 'react';

import type { DefaultFormat, EditorPlugin, IEditor } from 'roosterjs-editor-types';
import { Direction } from 'roosterjs-editor-types';

import { isMac } from '@proton/shared/lib/helpers/browser';
import type { MailSettings } from '@proton/shared/lib/interfaces/MailSettings';
import { DEFAULT_FONT_FACE, DEFAULT_FONT_SIZE, getFontFaceValueFromId } from '@proton/shared/lib/mail/fontFace';
import { DIRECTION } from '@proton/shared/lib/mail/mailSettings';

import { DEFAULT_BACKGROUND, DEFAULT_FONT_COLOR, ROOSTER_SNAPSHOTS_MAX_SIZE } from '../../constants';
import type { ModalLinkProps } from '../../hooks/interface';
import type { EditorActions, OnEditorEventListened } from '../../interface';
import EditorCustomPastePlugin from '../plugins/EditorCustomPastePlugin';
import EditorEventListener from '../plugins/EditorEventListener';
import UndoSnapshots from '../plugins/UndoSnapshots';
import getRoosterEditorActions from './getRoosterEditorActions';

interface Options {
    onEditorEvent: OnEditorEventListened;
    initialContent?: string;
    showModalLink: (props: ModalLinkProps) => void;
    iframeRef: RefObject<HTMLIFrameElement>;
    mailSettings?: MailSettings;
    onPasteFiles: (files: File[]) => void;
    openEmojiPicker: () => void;
}

interface InitRoosterReturns {
    editor: IEditor;
    actions: EditorActions;
}

export const getRoosterDirection = (direction: DIRECTION) =>
    direction === DIRECTION.RIGHT_TO_LEFT ? Direction.RightToLeft : Direction.LeftToRight;

export const initRoosterEditor = async (element: HTMLDivElement, options: Options): Promise<InitRoosterReturns> => {
    const {
        ContentEdit,
        Paste,
        HyperLink,
        Editor,
        CutPasteListChain,
        ExperimentalFeatures,
        createSnapshots,
        addSnapshot,
        canMoveCurrentSnapshot,
        canUndoAutoComplete,
        clearProceedingSnapshots,
        moveCurrentSnapshot,
        setDirection,
        createLink,
        ImageEdit,
    } = await import(/* webpackChunkName: "roosterjs", webpackPrefetch: true */ 'roosterjs');

    const borderColor = getComputedStyle(document.documentElement).getPropertyValue('--primary');
    const plugins: EditorPlugin[] = [
        new ContentEdit({
            indentWhenAltShiftRight: !isMac(),
            outdentWhenAltShiftLeft: !isMac(),
        }),
        new Paste(),
        new CutPasteListChain(),
        new HyperLink(),
        new EditorEventListener(options.onEditorEvent),
        new EditorCustomPastePlugin(options.onPasteFiles),
        new ImageEdit({
            disableRotate: true,
            preserveRatio: true,
            borderColor,
            minWidth: 20,
            minHeight: 20,
            disableSideResize: true,
        }),
    ];

    const fontSize = options.mailSettings?.FontSize ? `${options.mailSettings.FontSize}px` : `${DEFAULT_FONT_SIZE}px`;
    const fontFamily = (() => {
        if (options.mailSettings?.FontFace) {
            return getFontFaceValueFromId(options.mailSettings.FontFace) || DEFAULT_FONT_FACE;
        }
        return DEFAULT_FONT_FACE;
    })();

    const defaultFormat: DefaultFormat = {
        bold: false,
        fontFamily,
        fontSize,
        textColor: DEFAULT_FONT_COLOR,
        backgroundColor: DEFAULT_BACKGROUND,
    };

    if (element === null) {
        throw new Error('Ref should be provided to instanciate the editor');
    }

    // Create custom undoSnapshotService in order to be able to clear snapshots
    const undoSnapshotService = new UndoSnapshots(createSnapshots(ROOSTER_SNAPSHOTS_MAX_SIZE), {
        addSnapshot,
        canMoveCurrentSnapshot,
        canUndoAutoComplete,
        clearProceedingSnapshots,
        createSnapshots,
        moveCurrentSnapshot,
    });

    const editor = new Editor(element, {
        plugins,
        defaultFormat,
        initialContent: options.initialContent,
        undoSnapshotService,
        experimentalFeatures: [ExperimentalFeatures.AutoFormatList],
    });

    const actions = getRoosterEditorActions(
        editor,
        options.iframeRef,
        () => {
            undoSnapshotService.clearSnapshots();
        },
        (direction: DIRECTION) => {
            setDirection(editor, getRoosterDirection(direction));
        },
        () => {
            options.showModalLink?.({ editor, createLink });
        },
        options.openEmojiPicker
    );

    return { editor, actions };
};
