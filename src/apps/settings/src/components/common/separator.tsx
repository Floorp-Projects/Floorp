import { cn } from "@/lib/utils";

interface SeparatorProps {
  className?: string;
  orientation?: "horizontal" | "vertical";
}

export function Separator({
  className,
  orientation = "horizontal",
}: SeparatorProps) {
  return (
    <div
      className={cn(
        "shrink-0 bg-base-300/20",
        orientation === "horizontal"
          ? "h-[1px] w-full my-2"
          : "h-full w-[1px] mx-2",
        className,
      )}
    />
  );
}
