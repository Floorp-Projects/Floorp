import type { ReactNode } from "react";
import { cn } from "@/lib/utils";

interface AvatarProps {
  children?: ReactNode;
  className?: string;
  size?: "sm" | "md" | "lg";
}

export function Avatar({ children, className, size = "md" }: AvatarProps) {
  return (
    <div
      className={cn("relative flex shrink-0 overflow-hidden rounded-lg", {
        "h-8 w-8": size === "sm",
        "h-10 w-10": size === "md",
        "h-12 w-12": size === "lg",
      }, className)}
    >
      {children}
    </div>
  );
}

export function AvatarImage({
  src,
  alt,
  className,
}: {
  src: string;
  alt?: string;
  className?: string;
}) {
  return (
    <img
      src={src}
      alt={alt}
      className={cn(
        "aspect-square h-full w-full object-cover transition-opacity",
        className,
      )}
    />
  );
}

export function AvatarFallback({ children, className }: AvatarProps) {
  return (
    <div
      className={cn(
        "flex h-full w-full items-center justify-center rounded-lg",
        "bg-base-300/50 text-base-content/70 text-sm font-medium",
        className,
      )}
    >
      {children}
    </div>
  );
}
