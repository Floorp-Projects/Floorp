import { useState, useRef } from "react";
import { NoteList } from "./components/notes/NoteList.tsx";
import { RichTextEditor } from "./components/editor/RichTextEditor.tsx";
import { SerializedEditorState, SerializedLexicalNode } from "lexical";

interface Note {
  id: string;
  title: string;
  content: string;
  createdAt: Date;
  updatedAt: Date;
}

function App() {
  const [notes, setNotes] = useState<Note[]>([]);
  const [selectedNote, setSelectedNote] = useState<Note | null>(null);
  const [title, setTitle] = useState("");
  const [isReorderMode, setIsReorderMode] = useState(false);
  const editorKey = useRef(0);

  const createNewNote = () => {
    const newNote: Note = {
      id: Date.now().toString(),
      title: "新しいメモ",
      content: "",
      createdAt: new Date(),
      updatedAt: new Date()
    };
    setNotes([newNote, ...notes]);
    setSelectedNote(newNote);
    setTitle(newNote.title);
    editorKey.current += 1;
  };

  const updateCurrentNote = (content: string) => {
    if (!selectedNote) return;

    const updatedNote = {
      ...selectedNote,
      title,
      content,
      updatedAt: new Date()
    };

    setNotes(notes.map(note =>
      note.id === selectedNote.id ? updatedNote : note
    ));
    setSelectedNote(updatedNote);
  };

  const deleteNote = (id: string) => {
    setNotes(notes.filter(note => note.id !== id));
    if (selectedNote?.id === id) {
      setSelectedNote(null);
      setTitle("");
    }
  };

  const selectNote = (note: Note) => {
    setSelectedNote(note);
    setTitle(note.title);
    editorKey.current += 1;
  };

  const handleEditorChange = (editorState: SerializedEditorState<SerializedLexicalNode>) => {
    updateCurrentNote(JSON.stringify(editorState));
  };

  return (
    <div className="flex flex-col h-screen bg-base-100 text-base-content">
      <header className="bg-base-200 p-3 flex justify-between items-center">
        <h1 className="text-xl font-bold text-base-content">Floorp Notes</h1>
        <div className="flex gap-2">
          <button
            type="button"
            className={`btn btn-sm ${isReorderMode ? 'btn-secondary' : 'btn-ghost'}`}
            onClick={() => setIsReorderMode(!isReorderMode)}
          >
            {isReorderMode ? '完了' : '並び替え'}
          </button>
          <button
            type="button"
            className="btn btn-sm btn-primary"
            onClick={createNewNote}
          >
            新規メモ
          </button>
        </div>
      </header>

      <div className="flex flex-col flex-1 overflow-hidden">
        <NoteList
          notes={notes}
          selectedNote={selectedNote}
          onSelectNote={selectNote}
          onDeleteNote={deleteNote}
          onReorderNotes={setNotes}
          isReorderMode={isReorderMode}
        />

        <div className="flex-1 flex flex-col p-4 overflow-hidden border-t border-base-content/20">
          {selectedNote ? (
            <div className="flex flex-1 gap-4 overflow-y-auto">
              <div className="flex-1 flex flex-col">
                <input
                  type="text"
                  className="input input-bordered w-full mb-2 focus:outline-none focus:border-primary/30"
                  placeholder="タイトル"
                  value={title}
                  onChange={(e) => {
                    setTitle(e.target.value);
                    updateCurrentNote(selectedNote.content);
                  }}
                />
                <div className="flex-1 flex flex-col rounded-lg overflow-hidden">
                  <RichTextEditor
                    key={editorKey.current}
                    onChange={handleEditorChange}
                    initialContent={selectedNote.content}
                  />
                </div>
              </div>
            </div>
          ) : (
            <div className="flex flex-col items-center justify-center h-full text-base-content/70">
              <p className="mb-4">メモを選択するか、新しいメモを作成してください</p>
              <button
                type="button"
                className="btn btn-primary"
                onClick={createNewNote}
              >
                新規メモ作成
              </button>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}

export default App;
