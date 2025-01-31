import * as React from "react"
import {
  BookOpen,
  Bot,
  Frame,
  Map,
  PieChart,
  Settings2,
  House,
  PencilRuler,
  PanelLeft,
  Briefcase,
  Option,
  Grip,
  UserRoundPen,
  BadgeInfo,
  Wrench
} from "lucide-react"

import { NavUser } from "@/components/nav-user"
import { NavHeader } from "@/components/nav-header"
import {
  Sidebar,
  SidebarContent,
  SidebarFooter,
  SidebarGroup,
  SidebarGroupLabel,
  SidebarHeader,
  SidebarMenu,
  SidebarMenuButton,
  SidebarMenuItem,
  SidebarRail,
} from "@/components/ui/sidebar"
import { NavFeatures } from "./nav-features"

// This is sample data.
const data = {
  user: {
    name: "shadcn",
    email: "m@example.com",
    avatar: "/avatars/shadcn.jpg",
  },
  features: [
    {
      title: "Tab & Appearance",
      url: "#",
      icon: PencilRuler
    },
    {
      title: "Panel Sidebar",
      url: "#",
      icon: PanelLeft
    },
    {
      title: "Workspaces",
      url: "#",
      icon: Briefcase
    },{
      title: "Keyboard Shortcuts",
      url: "#",
      icon: Option
    },
    {
      title: "Web Apps",
      url: "#",
      icon: Grip
    },
    {
      title: "Profile and Account",
      url: "#",
      icon: UserRoundPen
    }
  ]
}

export function AppSidebar({ ...props }: React.ComponentProps<typeof Sidebar>) {
  return (
    <Sidebar collapsible="icon" {...props}>
      <SidebarHeader>
        <NavHeader />
      </SidebarHeader>

      <SidebarContent>
        <NavFeatures features={data.features}/>
        <SidebarGroup>
          <SidebarGroupLabel>
            About
          </SidebarGroupLabel>
          <SidebarMenu>
            <SidebarMenuItem>
              <SidebarMenuButton asChild>
                <a href={"#"}>
                  <BadgeInfo />
                  <span>About Noraneko</span>
                </a>
              </SidebarMenuButton>
            </SidebarMenuItem>
            <SidebarMenuItem>
              <SidebarMenuButton asChild>
                <a href={"#"}>
                  <Wrench />
                  <span>Debug</span>
                </a>
              </SidebarMenuButton>
            </SidebarMenuItem>
          </SidebarMenu>
        </SidebarGroup>
      </SidebarContent>
      <SidebarFooter>
        <NavUser user={data.user} />
      </SidebarFooter>
      <SidebarRail />
    </Sidebar>
  )
}
