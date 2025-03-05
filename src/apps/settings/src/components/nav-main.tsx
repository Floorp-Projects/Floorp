"use client";

import { type LucideIcon } from "lucide-react";

import {
  SidebarGroup,
  SidebarGroupLabel,
  SidebarMenu,
  SidebarMenuButton,
  SidebarMenuItem,
} from "@/components/common/sidebar";

export function NavMain({
  items,
}: {
  items: {
    title: string;
    url: string;
    icon?: LucideIcon;
    isActive?: boolean;
    items?: {
      title: string;
      url: string;
    }[];
  }[];
}) {
  return (
    <SidebarGroup>
      <SidebarGroupLabel>Platform</SidebarGroupLabel>
      <SidebarMenu>
        {items.map((item) => (
          <div key={item.title} className="collapse collapse-arrow">
            <input
              type="checkbox"
              defaultChecked={item.isActive}
              className="min-h-0"
            />
            <SidebarMenuItem className="collapse-title min-h-0 p-0">
              <SidebarMenuButton>
                {item.icon && <item.icon className="size-4" />}
                <span>{item.title}</span>
              </SidebarMenuButton>
            </SidebarMenuItem>
            <div className="collapse-content px-0 pb-0">
              <div className="menu">
                {item.items?.map((subItem) => (
                  <li key={subItem.title}>
                    <a
                      href={subItem.url}
                      className="rounded-none pl-9"
                    >
                      <span>{subItem.title}</span>
                    </a>
                  </li>
                ))}
              </div>
            </div>
          </div>
        ))}
      </SidebarMenu>
    </SidebarGroup>
  );
}
