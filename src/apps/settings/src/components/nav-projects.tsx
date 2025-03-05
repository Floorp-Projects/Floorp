import {
  Folder,
  Forward,
  type LucideIcon,
  MoreHorizontal,
  Trash2,
} from "lucide-react";

import {
  SidebarGroup,
  SidebarGroupLabel,
  SidebarMenu,
  SidebarMenuAction,
  SidebarMenuButton,
  SidebarMenuItem,
  useSidebar,
} from "@/components/common/sidebar";

export function NavProjects({
  projects,
}: {
  projects: {
    name: string;
    url: string;
    icon: LucideIcon;
  }[];
}) {
  const { isMobile } = useSidebar();

  return (
    <SidebarGroup className="group-data-[collapsible=icon]:hidden">
      <SidebarGroupLabel>Projects</SidebarGroupLabel>
      <SidebarMenu>
        {projects.map((item) => (
          <SidebarMenuItem key={item.name}>
            <SidebarMenuButton asChild>
              <a href={item.url}>
                <item.icon className="size-4" />
                <span>{item.name}</span>
              </a>
            </SidebarMenuButton>
            <div className="dropdown dropdown-end">
              <SidebarMenuAction showOnHover>
                <MoreHorizontal className="size-4" />
                <span className="sr-only">More</span>
              </SidebarMenuAction>
              <ul
                tabIndex={0}
                className="dropdown-content z-[1] menu p-2 shadow bg-base-100 rounded-box w-48"
              >
                <li>
                  <a>
                    <Folder className="text-base-content/70" />
                    <span>View Project</span>
                  </a>
                </li>
                <li>
                  <a>
                    <Forward className="text-base-content/70" />
                    <span>Share Project</span>
                  </a>
                </li>
                <div className="divider my-1"></div>
                <li>
                  <a className="text-error">
                    <Trash2 className="text-error/70" />
                    <span>Delete Project</span>
                  </a>
                </li>
              </ul>
            </div>
          </SidebarMenuItem>
        ))}
        <SidebarMenuItem>
          <SidebarMenuButton className="text-base-content/70">
            <MoreHorizontal className="size-4" />
            <span>More</span>
          </SidebarMenuButton>
        </SidebarMenuItem>
      </SidebarMenu>
    </SidebarGroup>
  );
}
