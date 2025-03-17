import { type ButtonHTMLAttributes, forwardRef } from "react";
import { cn } from "@/lib/utils";

export interface ButtonProps extends ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: "default" | "primary" | "secondary" | "ghost" | "link";
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
            "bg-primary text-primary-content hover:bg-primary/90 active:bg-primary/80":
              variant === "primary",
            "bg-secondary text-secondary-content hover:bg-secondary/90 active:bg-secondary/80":
              variant === "secondary",
            "hover:bg-base-300/50 active:bg-base-300": variant === "ghost",
            "underline-offset-4 hover:underline": variant === "link",
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
