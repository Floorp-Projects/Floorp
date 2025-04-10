import { rpc } from "./rpc/rpc.ts";
import { LexicalEditor } from "lexical";
import { $createParagraphNode, $createTextNode, $getRoot } from "lexical";

const NOTES_PREF_NAME = "floorp.browser.note.memos";

export interface NotesData {
  titles: string[];
  contents: string[];
}

const DEFAULT_NOTES: NotesData = {
  titles: ["Floorp Notes へようこそ！"],
  contents: [
    "Floorp Notes へようこそ！使い方を説明します。\n\nFloorp Notes は、デバイス間で同期できるメモ機能です。 同期を有効にするには、Mozilla アカウントで Floorp にサインインする必要があります。\nFloorp Notes は Floorp に保存され、Firefox Sync を使用してデバイス間で同期されます。 Firefox Sync は、同期の内容を Mozilla アカウントのパスワードで暗号化するため、誰にもその内容を知られることはありません。",
  ],
};

export async function getNotes(): Promise<NotesData> {
  try {
    const notesStr = await rpc.getStringPref(NOTES_PREF_NAME);
    if (!notesStr) {
      return DEFAULT_NOTES;
    }

    const parsedNotes = JSON.parse(notesStr) as NotesData;
    console.log(parsedNotes);
    return parsedNotes;
  } catch (e) {
    console.error("Failed to load note data:", e);
    return DEFAULT_NOTES;
  }
}

export async function saveNotes(data: NotesData): Promise<void> {
  try {
    await rpc.setStringPref(NOTES_PREF_NAME, JSON.stringify(data));
  } catch (e) {
    console.error("Failed to save note data:", e);
    throw e;
  }
}

export function serializeEditorToText(editor: LexicalEditor): string {
  let text = "";

  editor.getEditorState().read(() => {
    const root = $getRoot();
    const children = root.getChildren();
    const textParts: string[] = [];

    for (const child of children) {
      const childText = child.getTextContent();
      if (childText) {
        textParts.push(childText);
      }
    }

    text = textParts.join("\n");
  });

  return text;
}

export function deserializeTextToEditor(
  text: string,
  editor: LexicalEditor,
): void {
  editor.update(() => {
    const root = $getRoot();
    root.clear();

    const lines = text.split("\n");

    for (const line of lines) {
      const paragraphNode = $createParagraphNode();
      if (line.length > 0) {
        paragraphNode.append($createTextNode(line));
      }
      root.append(paragraphNode);
    }
  });
}

export async function addNote(title: string, content: string): Promise<void> {
  const notes = await getNotes();
  notes.titles.push(title);
  notes.contents.push(content);
  await saveNotes(notes);
}

export async function updateNote(
  index: number,
  title: string,
  content: string,
): Promise<void> {
  const notes = await getNotes();
  if (index >= 0 && index < notes.titles.length) {
    notes.titles[index] = title;
    notes.contents[index] = content;
    await saveNotes(notes);
  } else {
    throw new Error(`Index ${index} is out of range`);
  }
}

export async function deleteNote(index: number): Promise<void> {
  const notes = await getNotes();
  if (index >= 0 && index < notes.titles.length) {
    notes.titles.splice(index, 1);
    notes.contents.splice(index, 1);
    await saveNotes(notes);
  } else {
    throw new Error(`Index ${index} is out of range`);
  }
}

export async function reorderNotes(
  fromIndex: number,
  toIndex: number,
): Promise<void> {
  const notes = await getNotes();

  if (
    fromIndex < 0 ||
    fromIndex >= notes.titles.length ||
    toIndex < 0 ||
    toIndex >= notes.titles.length
  ) {
    throw new Error("Index is out of range");
  }

  const [movedTitle] = notes.titles.splice(fromIndex, 1);
  notes.titles.splice(toIndex, 0, movedTitle);

  const [movedContent] = notes.contents.splice(fromIndex, 1);
  notes.contents.splice(toIndex, 0, movedContent);

  await saveNotes(notes);
}
