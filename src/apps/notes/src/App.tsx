import { useState } from "react";

interface Note {
  id: string;
  title: string;
  content: string;
  createdAt: Date;
  updatedAt: Date;
}

type EditorMode = "markdown" | "html" | "rich";

function App() {
  const [notes, setNotes] = useState<Note[]>([]);
  const [selectedNote, setSelectedNote] = useState<Note | null>(null);
  const [title, setTitle] = useState("");
  const [content, setContent] = useState("");
  const [editorMode, setEditorMode] = useState<EditorMode>("markdown");

  // 新しいノートを作成
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
    setContent(newNote.content);
  };

  // ノートを更新
  const updateCurrentNote = () => {
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

  // ノートを削除
  const deleteNote = (id: string) => {
    setNotes(notes.filter(note => note.id !== id));
    if (selectedNote?.id === id) {
      setSelectedNote(null);
      setTitle("");
      setContent("");
    }
  };

  // ノートを選択
  const selectNote = (note: Note) => {
    setSelectedNote(note);
    setTitle(note.title);
    setContent(note.content);
  };

  // 日付のフォーマット
  const formatDate = (date: Date) => {
    return new Intl.DateTimeFormat('ja-JP', {
      year: 'numeric',
      month: '2-digit',
      day: '2-digit',
      hour: '2-digit',
      minute: '2-digit'
    }).format(date);
  };

  return (
    <div className="flex flex-col h-screen bg-base-100 text-base-content">
      {/* ヘッダー */}
      <header className="bg-base-200 p-3 flex justify-between items-center">
        <h1 className="text-xl font-bold text-base-content">Floorp Notes</h1>
        <button
          className="btn btn-sm btn-primary"
          onClick={createNewNote}
        >
          新規メモ
        </button>
      </header>

      <div className="flex flex-col flex-1 overflow-hidden">
        {/* メモリスト (上部) */}
        <div className="flex-none overflow-y-auto h-1/3">
          {notes.length === 0 ? (
            <div className="p-4 text-center text-base-content/70">
              メモがありません。「新規メモ」ボタンをクリックして作成してください。
            </div>
          ) : (
            <div className="flex flex-col p-2 gap-1">
              {notes.map(note => (
                <button
                  key={note.id}
                  className={`flex flex-col p-2 rounded-lg border transition-colors ${selectedNote?.id === note.id
                    ? 'bg-primary/10 border-primary'
                    : 'hover:bg-base-200 border-base-content/5'
                    }`}
                  onClick={() => selectNote(note)}
                >
                  <div className="flex justify-between items-center">
                    <span className="font-medium truncate">{note.title}</span>
                    <button
                      className="btn btn-xs btn-ghost btn-circle opacity-70 hover:opacity-100"
                      onClick={(e) => {
                        e.stopPropagation();
                        deleteNote(note.id);
                      }}
                    >
                      ×
                    </button>
                  </div>
                  <div className="flex justify-between items-center mt-1">
                    <span className="text-xs text-base-content/70 truncate">{note.content || "内容なし"}</span>
                    <span className="text-xs text-base-content/70">{formatDate(note.updatedAt)}</span>
                  </div>
                </button>
              ))}
            </div>
          )}
        </div>

        {/* エディタ (下部) */}
        <div className="flex-1 flex flex-col p-4 overflow-hidden border-t border-base-content/20">
          {selectedNote ? (
            <div className="flex flex-1 gap-4">
              <div className="flex-1 flex flex-col">
                <input
                  type="text"
                  className="input input-bordered w-full mb-2 focus:border-primary focus:ring-1 focus:ring-primary"
                  placeholder="タイトル"
                  value={title}
                  onChange={(e) => {
                    setTitle(e.target.value);
                    updateCurrentNote();
                  }}
                />
                <textarea
                  className="textarea textarea-bordered flex-1 resize-none w-full focus:border-primary focus:ring-1 focus:ring-primary"
                  placeholder="メモを入力してください..."
                  value={content}
                  onChange={(e) => {
                    setContent(e.target.value);
                    updateCurrentNote();
                  }}
                />
                <div className="flex justify-between mt-2 text-xs text-base-content/70">
                  <div>作成: {formatDate(selectedNote.createdAt)}</div>
                  <div>更新: {formatDate(selectedNote.updatedAt)}</div>
                </div>
              </div>

              {/* ツールバー */}
              <div className="w-4 flex flex-col items-center gap-2 pt-2">
                <div className="tooltip tooltip-left" data-tip="Markdown">
                  <button
                    className={`btn btn-sm btn-square ${editorMode === "markdown" ? "btn-primary" : "btn-ghost"}`}
                    onClick={() => setEditorMode("markdown")}
                    aria-label="Markdown モード"
                  >
                    <svg xmlns="http://www.w3.org/2000/svg" className="h-5 w-5" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
                      <path d="M12 3h7a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2h-7"></path>
                      <path d="M8 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h3"></path>
                      <path d="M8 13h8"></path>
                      <path d="M8 9h8"></path>
                      <path d="M8 17h4"></path>
                    </svg>
                  </button>
                </div>
                <div className="tooltip tooltip-left" data-tip="HTML">
                  <button
                    className={`btn btn-sm btn-square ${editorMode === "html" ? "btn-primary" : "btn-ghost"}`}
                    onClick={() => setEditorMode("html")}
                    aria-label="HTML モード"
                  >
                    <svg xmlns="http://www.w3.org/2000/svg" className="h-5 w-5" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
                      <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"></path>
                      <path d="M14 2v6h6"></path>
                      <path d="M16 13H8"></path>
                      <path d="M16 17H8"></path>
                      <path d="M10 9H9H8"></path>
                    </svg>
                  </button>
                </div>
                <div className="tooltip tooltip-left" data-tip="リッチテキスト">
                  <button
                    className={`btn btn-sm btn-square ${editorMode === "rich" ? "btn-primary" : "btn-ghost"}`}
                    onClick={() => setEditorMode("rich")}
                    aria-label="リッチテキスト モード"
                  >
                    <svg xmlns="http://www.w3.org/2000/svg" className="h-5 w-5" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
                      <path d="M12 3h7a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2h-7"></path>
                      <path d="M8 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h3"></path>
                      <path d="M8 13h8"></path>
                      <path d="M8 9h8"></path>
                      <path d="M8 17h4"></path>
                      <path d="M8 5h4"></path>
                    </svg>
                  </button>
                </div>
              </div>
            </div>
          ) : (
            <div className="flex flex-col items-center justify-center h-full text-base-content/70">
              <p className="mb-4">メモを選択するか、新しいメモを作成してください</p>
              <button
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
