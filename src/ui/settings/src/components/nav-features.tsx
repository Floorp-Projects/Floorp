import { LucideIcon } from "lucide-react";
import {
  SidebarGroup,
  SidebarGroupLabel,
  SidebarMenu,
  SidebarMenuButton,
  SidebarMenuItem,
} from "@/components/ui/sidebar.tsx";
import { useLocation } from "react-router-dom";

export function NavFeatures({ title, features }: {
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
          const activeClass = isActive
            ? "bg-gray-200 dark:bg-gray-700 rounded-sm"
            : "";

          return (
            <SidebarMenuItem key={feature.title} className={activeClass}>
              <SidebarMenuButton asChild>
                <a href={feature.url}>
                  <feature.icon />
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
