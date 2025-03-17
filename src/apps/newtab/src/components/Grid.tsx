import { type ReactNode } from "react";

interface GridProps {
  children: ReactNode;
}

export function Grid({ children }: GridProps) {
  return (
    <div className="grid grid-cols-4 gap-4">
      {children}
    </div>
  );
}
