import type { ReactNode } from "react";
import { cn } from "@/lib/utils";

interface CardProps {
  children: ReactNode;
  className?: string;
}

export function Card({ children, className }: CardProps) {
  return (
    <div
      className={cn(
        "card bg-base-100/30 shadow-md border border-base-300/20 backdrop-blur-sm dark:bg-base-300/40",
        className,
      )}
    >
      {children}
    </div>
  );
}

export function CardHeader({ children, className }: CardProps) {
  return (
    <div
      className={cn(
        "card-body pb-2 p-4 border-b border-base-300/20",
        className,
      )}
    >
      {children}
    </div>
  );
}

export function CardTitle({ children, className }: CardProps) {
  return (
    <h2
      className={cn("card-title text-base-content/90 font-medium", className)}
    >
      {children}
    </h2>
  );
}

export function CardDescription({ children, className }: CardProps) {
  return (
    <p className={cn("text-base-content/60 text-sm", className)}>{children}</p>
  );
}

export function CardContent({ children, className }: CardProps) {
  return (
    <div className={cn("card-body pt-0 pb-2 px-4", className)}>{children}</div>
  );
}

export function CardFooter({ children, className }: CardProps) {
  return (
    <div
      className={cn(
        "card-body pt-0 px-4 border-t border-base-300/20 bg-base-200/30 dark:bg-base-200/10",
        className,
      )}
    >
      {children}
    </div>
  );
}
