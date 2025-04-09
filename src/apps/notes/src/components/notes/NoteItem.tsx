import { CSS } from "@dnd-kit/utilities";
import { useSortable } from "@dnd-kit/sortable";
import { MoveVertical, X } from "lucide-react";

interface Note {
    id: string;
    title: string;
    content: string;
    createdAt: Date;
    updatedAt: Date;
}

interface NoteItemProps {
    note: Note;
    isSelected: boolean;
    onSelect: (note: Note) => void;
    onDelete: (id: string) => void;
    isReorderMode: boolean;
}

export const NoteItem = ({ note, isSelected, onSelect, onDelete, isReorderMode }: NoteItemProps) => {
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

    const extractContent = (root: any) => {
        function extractFromNestedObject(obj: any) {
            if (obj.text !== undefined) {
                return obj.text;
            }

            if (!obj.children) {
                return "内容なし"
            }

            return obj.children.map((child: any) => extractFromNestedObject(child)).join("");
        }
        return extractFromNestedObject(root);
    }

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
                    <span className="text-xs text-base-content/70 truncate">{note.content.length !== 0 ? extractContent(JSON.parse(note.content).root) : "内容なし"}</span>
                    <span className="text-xs text-base-content/70">{formatDate(note.updatedAt)}</span>
                </div>
            </button>
        </div>
    );
};
