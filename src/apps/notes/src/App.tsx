import { useState } from "react";
import {
  DndContext,
  closestCenter,
  KeyboardSensor,
  PointerSensor,
  useSensor,
  useSensors,
  DragEndEvent
} from "@dnd-kit/core";
import {
  arrayMove,
  SortableContext,
  sortableKeyboardCoordinates,
  useSortable,
  verticalListSortingStrategy
} from "@dnd-kit/sortable";
import {
  restrictToVerticalAxis,
  restrictToFirstScrollableAncestor
} from '@dnd-kit/modifiers';
import { CSS } from "@dnd-kit/utilities";
import {
  MoveVertical,
  X,
  FileText,
  FileCode,
  FileEdit
} from "lucide-react";

interface Note {
  id: string;
  title: string;
  content: string;
  createdAt: Date;
  updatedAt: Date;
}

type EditorMode = "markdown" | "html" | "rich";

function SortableNoteItem({ note, isSelected, onSelect, onDelete, isReorderMode }: {
  note: Note;
  isSelected: boolean;
  onSelect: (note: Note) => void;
  onDelete: (id: string) => void;
  isReorderMode: boolean;
}) {
  const {
    attributes,
    listeners,
    setNodeRef,
    transform,
    transition
  } = useSortable({ id: note.id });

  const style = {
    transform: CSS.Transform.toString(transform),
    transition,
  };

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
    <div
      ref={setNodeRef}
      style={style}
      className="mb-1"
      {...(isReorderMode ? attributes : {})}
    >
      <button
        type="button"
        className={`w-full flex flex-col p-2 rounded-lg border transition-colors ${isReorderMode
          ? 'cursor-grab active:cursor-grabbing border-secondary/50'
          : ''
          } ${isSelected
            ? 'bg-primary/10 border-primary'
            : 'hover:bg-base-200 border-base-content/5'
          }`}
        onClick={() => onSelect(note)}
        {...(isReorderMode ? listeners : {})}
      >
        <div className="flex justify-between items-center">
          <div className="flex items-center gap-1">
            {isReorderMode && (
              <MoveVertical className="h-4 w-4 text-secondary" />
            )}
            <span className="font-medium truncate">{note.title}</span>
          </div>
          {!isReorderMode && (
            <button
              type="button"
              className="btn btn-xs btn-ghost btn-circle opacity-70 hover:opacity-100"
              onClick={(e) => {
                e.stopPropagation();
                onDelete(note.id);
              }}
            >
              <X className="h-3 w-3" />
            </button>
          )}
        </div>
        <div className="flex justify-between items-center mt-1">
          <span className="text-xs text-base-content/70 truncate">{note.content || "内容なし"}</span>
          <span className="text-xs text-base-content/70">{formatDate(note.updatedAt)}</span>
        </div>
      </button>
    </div>
  );
}

function App() {
  const [notes, setNotes] = useState<Note[]>([]);
  const [selectedNote, setSelectedNote] = useState<Note | null>(null);
  const [title, setTitle] = useState("");
  const [content, setContent] = useState("");
  const [editorMode, setEditorMode] = useState<EditorMode>("markdown");
  const [isReorderMode, setIsReorderMode] = useState(false);

  const sensors = useSensors(
    useSensor(PointerSensor),
    useSensor(KeyboardSensor, {
      coordinateGetter: sortableKeyboardCoordinates,
    })
  );

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

  const deleteNote = (id: string) => {
    setNotes(notes.filter(note => note.id !== id));
    if (selectedNote?.id === id) {
      setSelectedNote(null);
      setTitle("");
      setContent("");
    }
  };

  const selectNote = (note: Note) => {
    setSelectedNote(note);
    setTitle(note.title);
    setContent(note.content);
  };

  const handleDragEnd = (event: DragEndEvent) => {
    const { active, over } = event;

    if (over && active.id !== over.id) {
      setNotes((items) => {
        const oldIndex = items.findIndex(item => item.id === active.id);
        const newIndex = items.findIndex(item => item.id === over.id);

        return arrayMove(items, oldIndex, newIndex);
      });
    }
  };

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
        <div className="flex-none overflow-y-auto h-1/3">
          {notes.length === 0 ? (
            <div className="p-4 text-center text-base-content/70">
              メモがありません。「新規メモ」ボタンをクリックして作成してください。
            </div>
          ) : (
            <div className="flex flex-col p-2">
              <DndContext
                sensors={sensors}
                collisionDetection={closestCenter}
                onDragEnd={handleDragEnd}
                modifiers={[restrictToVerticalAxis, restrictToFirstScrollableAncestor]}
              >
                <SortableContext
                  items={notes.map(note => note.id)}
                  strategy={verticalListSortingStrategy}
                >
                  {notes.map(note => (
                    <SortableNoteItem
                      key={note.id}
                      note={note}
                      isSelected={selectedNote?.id === note.id}
                      onSelect={isReorderMode ? () => { } : selectNote}
                      onDelete={isReorderMode ? () => { } : deleteNote}
                      isReorderMode={isReorderMode}
                    />
                  ))}
                </SortableContext>
              </DndContext>
            </div>
          )}
        </div>

        <div className="flex-1 flex flex-col p-4 overflow-hidden border-t border-base-content/20">
          {selectedNote ? (
            <div className="flex flex-1 gap-4">
              <div className="flex-1 flex flex-col">
                <input
                  type="text"
                  className="input input-bordered w-full mb-2 focus:outline-none focus:border-primary/30"
                  placeholder="タイトル"
                  value={title}
                  onChange={(e) => {
                    setTitle(e.target.value);
                    updateCurrentNote();
                  }}
                />
                <textarea
                  className="textarea textarea-bordered flex-1 resize-none w-full focus:outline-none focus:border-primary/30"
                  placeholder="メモを入力してください..."
                  value={content}
                  onChange={(e) => {
                    setContent(e.target.value);
                    updateCurrentNote();
                  }}
                />
                {/* <div className="flex justify-between mt-2 text-xs text-base-content/70">
                  <div>作成: {formatDate(selectedNote.createdAt)}</div>
                  <div>更新: {formatDate(selectedNote.updatedAt)}</div>
                </div> */}
              </div>

              <div className="w-4 flex flex-col items-center gap-2 pt-2">
                <div className="tooltip tooltip-left" data-tip="Markdown">
                  <button
                    type="button"
                    className={`btn btn-sm btn-square ${editorMode === "markdown" ? "btn-primary" : "btn-ghost"}`}
                    onClick={() => setEditorMode("markdown")}
                    aria-label="Markdown モード"
                  >
                    <FileText className="h-5 w-5" />
                  </button>
                </div>
                <div className="tooltip tooltip-left" data-tip="HTML">
                  <button
                    type="button"
                    className={`btn btn-sm btn-square ${editorMode === "html" ? "btn-primary" : "btn-ghost"}`}
                    onClick={() => setEditorMode("html")}
                    aria-label="HTML モード"
                  >
                    <FileCode className="h-5 w-5" />
                  </button>
                </div>
                <div className="tooltip tooltip-left" data-tip="リッチテキスト">
                  <button
                    type="button"
                    className={`btn btn-sm btn-square ${editorMode === "rich" ? "btn-primary" : "btn-ghost"}`}
                    onClick={() => setEditorMode("rich")}
                    aria-label="リッチテキスト モード"
                  >
                    <FileEdit className="h-5 w-5" />
                  </button>
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
