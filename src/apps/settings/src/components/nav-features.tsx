import { LucideIcon } from "lucide-react";
import { SidebarGroup, SidebarGroupLabel, SidebarMenu, SidebarMenuButton, SidebarMenuItem } from "./ui/sidebar";

export function NavFeatures({features}:{
  features:{
    title: string
    url: string
    icon: LucideIcon
  }[]
}) {
  return <SidebarGroup>
    <SidebarGroupLabel>Features</SidebarGroupLabel>
    <SidebarMenu>
      {features.map((feature)=>
        <SidebarMenuItem key={feature.title}>
          <SidebarMenuButton asChild>
            <a href={feature.url}>
              <feature.icon />
              <span>{feature.title}</span>
            </a>
          </SidebarMenuButton>
        </SidebarMenuItem>
      )}
    </SidebarMenu>
  </SidebarGroup>
}