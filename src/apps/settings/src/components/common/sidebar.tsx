import { createContext, type ReactNode, useContext } from "react";
import { cn } from "@/lib/utils";

const SidebarContext = createContext<{
  isMobile: boolean;
}>({
  isMobile: false,
});

export const useSidebar = () => useContext(SidebarContext);

interface SidebarProps {
  children?: ReactNode;
  className?: string;
  collapsible?: "icon";
}

export function Sidebar({ children, className }: SidebarProps) {
  return (
    <div className="drawer w-auto lg:drawer-open">
      <input id="settings-drawer" type="checkbox" className="drawer-toggle" />
      <div className={cn("drawer-side", className)}>
        <label
          htmlFor="settings-drawer"
          aria-label="close sidebar"
          className="drawer-overlay"
        >
        </label>
        <div className="min-h-screen w-80 bg-base-200/50 text-base-content border-r border-base-300">
          {children}
        </div>
      </div>
    </div>
  );
}

export function SidebarHeader({ children, className }: SidebarProps) {
  return (
    <div
      className={cn(
        "border-b border-base-300/50 px-6 py-3 backdrop-blur-sm bg-base-200/30",
        className,
      )}
    >
      {children}
    </div>
  );
}

export function SidebarContent({ children, className }: SidebarProps) {
  return (
    <div
      className={cn(
        "flex-1 overflow-auto px-4 py-2 backdrop-blur-sm",
        className,
      )}
    >
      {children}
    </div>
  );
}

export function SidebarFooter({ children, className }: SidebarProps) {
  return (
    <div
      className={cn(
        "mt-auto border-t border-base-300/50 py-4 px-6 backdrop-blur-sm bg-base-200/30",
        className,
      )}
    >
      {children}
    </div>
  );
}

export function SidebarGroup({ children, className }: SidebarProps) {
  return <div className={cn("px-2 py-2", className)}>{children}</div>;
}

export function SidebarGroupLabel({ children, className }: SidebarProps) {
  return (
    <h3
      className={cn(
        "mb-2 px-4 text-sm font-medium text-base-content/60",
        className,
      )}
    >
      {children}
    </h3>
  );
}

export function SidebarMenu({ children, className }: SidebarProps) {
  return <div className={cn("space-y-1", className)}>{children}</div>;
}

export function SidebarMenuItem(
  { children, className, href }: SidebarProps & { href?: string },
) {
  return (
    <div
      className={cn(
        "flex items-center gap-2 rounded-lg px-3 py-2 transition-colors",
        className,
      )}
    >
      <a href={href} className="flex items-center gap-2 w-full">
        {children}
      </a>
    </div>
  );
}

export function SidebarMenuButton({
  children,
  className,
  asChild,
  ...props
}: SidebarProps & { asChild?: boolean }) {
  const Comp = asChild ? "span" : "button";
  return (
    <Comp
      className={cn(
        "flex w-full items-center gap-2 rounded-lg px-3 py-2 text-sm font-medium transition-colors hover:bg-base-300/50 active:bg-base-300",
        className,
      )}
      {...props}
    >
      {children}
    </Comp>
  );
}

export function SidebarMenuAction({
  children,
  className,
  showOnHover,
  ...props
}: SidebarProps & { showOnHover?: boolean }) {
  return (
    <button
      className={cn(
        "flex h-7 w-7 items-center justify-center rounded-lg transition-colors hover:bg-base-300/50 active:bg-base-300",
        showOnHover && "opacity-0 group-hover:opacity-100",
        className,
      )}
      {...props}
    >
      {children}
    </button>
  );
}

export function SidebarTrigger() {
  return (
    <label
      htmlFor="settings-drawer"
      className="btn btn-square btn-ghost drawer-button lg:hidden"
    >
      <svg
        xmlns="http://www.w3.org/2000/svg"
        fill="none"
        viewBox="0 0 24 24"
        className="inline-block h-5 w-5 stroke-current"
      >
        <path
          strokeLinecap="round"
          strokeLinejoin="round"
          strokeWidth="2"
          d="M4 6h16M4 12h16M4 18h16"
        >
        </path>
      </svg>
    </label>
  );
}

export function SidebarRail({ className }: { className?: string }) {
  return (
    <div
      className={cn(
        "absolute right-0 top-0 bottom-0 w-2 bg-transparent group-hover:bg-base-300/50 transition-colors duration-300",
        className,
      )}
    />
  );
}
