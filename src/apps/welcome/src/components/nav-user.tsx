"use client";

import { BadgeCheck, Bell, ChevronsUpDown, LogOut } from "lucide-react";

import {
  SidebarMenu,
  SidebarMenuButton,
  SidebarMenuItem,
  useSidebar,
} from "@/components/common/sidebar";

export function NavUser({
  user,
}: {
  user: {
    name: string;
    email: string;
    avatar: string;
  };
}) {
  const { isMobile } = useSidebar();

  return (
    <SidebarMenu>
      <SidebarMenuItem>
        <div className="dropdown dropdown-end">
          <SidebarMenuButton
            className="hover:bg-base-300"
          >
            <div className="avatar">
              <div className="w-8 h-8 rounded-lg">
                <img src={user.avatar} alt={user.name} />
              </div>
            </div>
            <div className="grid flex-1 text-left text-sm leading-tight">
              <span className="truncate font-semibold">{user.name}</span>
              <span className="truncate text-xs text-base-content/70">
                {user.email}
              </span>
            </div>
            <ChevronsUpDown className="ml-auto size-4" />
          </SidebarMenuButton>
          <ul
            tabIndex={0}
            className="dropdown-content z-[1] menu p-2 shadow bg-base-100 rounded-box w-56 mt-4"
          >
            <li className="menu-title">
              <div className="flex items-center gap-2 px-1">
                <div className="avatar">
                  <div className="w-8 h-8 rounded-lg">
                    <img src={user.avatar} alt={user.name} />
                  </div>
                </div>
                <div className="grid flex-1 text-left text-sm leading-tight">
                  <span className="truncate font-semibold">{user.name}</span>
                  <span className="truncate text-xs text-base-content/70">
                    {user.email}
                  </span>
                </div>
              </div>
            </li>
            <div className="divider my-1"></div>
            <li>
              <a>
                <BadgeCheck className="size-4" />
                Account
              </a>
            </li>
            <li>
              <a>
                <Bell className="size-4" />
                Notifications
              </a>
            </li>
            <div className="divider my-1"></div>
            <li>
              <a className="text-error">
                <LogOut className="size-4" />
                Log out
              </a>
            </li>
          </ul>
        </div>
      </SidebarMenuItem>
    </SidebarMenu>
  );
}
