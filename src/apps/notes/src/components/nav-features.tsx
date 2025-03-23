import type { LucideIcon } from "lucide-react";
import {
  SidebarGroup,
  SidebarGroupLabel,
  SidebarMenu,
  SidebarMenuButton,
  SidebarMenuItem,
} from "@/components/common/sidebar.tsx";
import { useLocation } from "react-router-dom";

export function NavFeatures({
  title,
  features,
}: {
  title: string;
  features: {
    title: string;
    url: string;
    icon: LucideIcon;
  }[];
}) {
  const location = useLocation();
  const currentRoute = location.pathname;

  return (
    <SidebarGroup>
      <SidebarGroupLabel>{title}</SidebarGroupLabel>
      <SidebarMenu>
        {features.map((feature) => {
          const featurePath = feature.url.startsWith("#")
            ? feature.url.slice(1)
            : feature.url;
          const isActive = featurePath === "/"
            ? currentRoute === "/"
            : currentRoute.startsWith(featurePath);

          return (
            <SidebarMenuItem key={feature.title} href={feature.url}>
              <SidebarMenuButton
                asChild
                className={isActive ? "bg-base-300" : ""}
              >
                <a className="flex items-center gap-2">
                  <feature.icon className="size-4" />
                  <span>{feature.title}</span>
                </a>
              </SidebarMenuButton>
            </SidebarMenuItem>
          );
        })}
      </SidebarMenu>
    </SidebarGroup>
  );
}
