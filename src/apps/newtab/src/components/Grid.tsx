import { type ReactNode } from "react";

export function Grid({ children }: { children: ReactNode }) {
  return (
    <div className="container mx-auto max-w-6xl">
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
        {children}
      </div>
    </div>
  );
}
