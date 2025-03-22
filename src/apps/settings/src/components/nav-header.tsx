import { Settings } from "lucide-react";
import {
  SidebarMenu,
  SidebarMenuButton,
  SidebarMenuItem,
} from "@/components/common/sidebar.tsx";

export function NavHeader() {
  return (
    <SidebarMenu>
      <SidebarMenuItem>
        <SidebarMenuButton className="pointer-events-none">
          <div className="flex aspect-square size-8 items-center justify-center rounded-lg text-primary-foreground">
            <Settings className="size-6" />
          </div>
          <div className="text-sm">
            <span className="truncate font-semibold">
              Floorp Hub
            </span>
          </div>
        </SidebarMenuButton>
      </SidebarMenuItem>
    </SidebarMenu>
  );
}
