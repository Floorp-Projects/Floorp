import { type ButtonHTMLAttributes, forwardRef } from "react";
import { cn } from "@/lib/utils";

export interface ButtonProps extends ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: "default" | "primary" | "secondary" | "ghost" | "link" | "light";
  size?: "default" | "sm" | "lg";
  asChild?: boolean;
}

const Button = forwardRef<HTMLButtonElement, ButtonProps>(
  (
    {
      className,
      variant = "primary",
      size = "default",
      asChild = false,
      ...props
    },
    ref,
  ) => {
    const Comp = asChild ? "span" : "button";
    return (
      <Comp
        className={cn(
          "inline-flex items-center justify-center rounded-lg font-medium transition-colors duration-200",
          "focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-base-content/20",
          "disabled:pointer-events-none disabled:opacity-50",
          {
            "bg-primary/90 text-primary-content hover:bg-primary/80 active:bg-primary/70":
              variant === "primary",
            "bg-secondary/90 text-secondary-content hover:bg-secondary/80 active:bg-secondary/70":
              variant === "secondary",
            "hover:bg-base-300/40 active:bg-base-300/60": variant === "ghost",
            "underline-offset-4 hover:underline": variant === "link",
            "bg-base-200/40 text-base-content hover:bg-base-200/60 active:bg-base-200/70 dark:bg-base-300/25 dark:hover:bg-base-300/40 dark:active:bg-base-300/50":
              variant === "light",
            "h-8 px-4 text-sm": size === "sm",
            "h-10 px-6 text-base": size === "default",
            "h-12 px-8 text-lg": size === "lg",
          },
          className,
        )}
        ref={ref}
        {...props}
      />
    );
  },
);
Button.displayName = "Button";

export { Button };
