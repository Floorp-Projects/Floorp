import { DndContext, closestCenter, KeyboardSensor, PointerSensor, useSensor, useSensors, DragEndEvent } from "@dnd-kit/core";
import { arrayMove, SortableContext, sortableKeyboardCoordinates, verticalListSortingStrategy } from "@dnd-kit/sortable";
import { restrictToVerticalAxis, restrictToFirstScrollableAncestor } from '@dnd-kit/modifiers';
import { NoteItem } from "./NoteItem";

interface Note {
    id: string;
    title: string;
    content: string;
    createdAt: Date;
    updatedAt: Date;
}

interface NoteListProps {
    notes: Note[];
    selectedNote: Note | null;
    onSelectNote: (note: Note) => void;
    onDeleteNote: (id: string) => void;
    onReorderNotes: (notes: Note[]) => void;
    isReorderMode: boolean;
}

export const NoteList = ({
    notes,
    selectedNote,
    onSelectNote,
    onDeleteNote,
    onReorderNotes,
    isReorderMode
}: NoteListProps) => {
    const sensors = useSensors(
        useSensor(PointerSensor),
        useSensor(KeyboardSensor, {
            coordinateGetter: sortableKeyboardCoordinates,
        })
    );

    const handleDragEnd = (event: DragEndEvent) => {
        const { active, over } = event;

        if (over && active.id !== over.id) {
            const oldIndex = notes.findIndex(note => note.id === active.id);
            const newIndex = notes.findIndex(note => note.id === over.id);
            const newNotes = arrayMove(notes, oldIndex, newIndex);
            onReorderNotes(newNotes);
        }
    };

    return (
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
                                <NoteItem
                                    key={note.id}
                                    note={note}
                                    isSelected={selectedNote?.id === note.id}
                                    onSelect={isReorderMode ? () => { } : onSelectNote}
                                    onDelete={isReorderMode ? () => { } : onDeleteNote}
                                    isReorderMode={isReorderMode}
                                />
                            ))}
                        </SortableContext>
                    </DndContext>
                </div>
            )}
        </div>
    );
};
