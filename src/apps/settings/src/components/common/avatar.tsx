import type { ReactNode } from "react";
import { cn } from "@/lib/utils";
import { useState } from "react";

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
  fallback,
}: {
  src: string | undefined;
  alt?: string;
  className?: string;
  fallback?: string;
}) {
  const [error, setError] = useState(false);

  return (
    <div className="relative h-full w-full">
      {!error
        ? (
          <img
            src={src}
            alt={alt}
            className={cn(
              "aspect-square h-full w-full object-cover transition-opacity rounded-full",
              className,
            )}
            onError={() => setError(true)}
          />
        )
        : (
          <div className="flex h-full w-full items-center justify-center bg-muted text-black">
            {fallback || alt?.charAt(0) || "?"}
          </div>
        )}
    </div>
  );
}
