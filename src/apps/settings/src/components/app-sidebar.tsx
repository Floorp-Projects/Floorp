import type * as React from "react";
import { useTranslation } from "react-i18next";
import {
  BadgeInfo,
  Briefcase,
  Grip,
  House,
  Option,
  PanelLeft,
  PencilRuler,
  UserRoundPen,
  Wrench,
} from "lucide-react";
import { NavUser } from "@/components/nav-user.tsx";
import { NavHeader } from "@/components/nav-header.tsx";
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
} from "@/components/ui/sidebar.tsx";
import { NavFeatures } from "@/components/nav-features.tsx";

export function AppSidebar({ ...props }: React.ComponentProps<typeof Sidebar>) {
  const { t } = useTranslation();

  const user = {
    name: "shadcn",
    email: "m@example.com",
    avatar: "/avatars/shadcn.jpg",
  };

  const features = [
    { title: t("pages.home"), url: "#/", icon: House },
    { title: t("pages.tabAndAppearance"), url: "#/design", icon: PencilRuler },
    { title: t("pages.browserSidebar"), url: "#/sidebar", icon: PanelLeft },
    { title: t("pages.workspaces"), url: "#/workspaces", icon: Briefcase },
    { title: t("pages.keyboardShortcuts"), url: "#/shortcuts", icon: Option },
    { title: t("pages.webApps"), url: "#/webapps", icon: Grip },
    { title: t("pages.profileAndAccount"), url: "#/accounts", icon: UserRoundPen },
  ];

  return (
    <Sidebar collapsible="icon" {...props}>
      <SidebarHeader>
        <NavHeader />
      </SidebarHeader>

      <SidebarContent>
        <NavFeatures features={features} />
        <SidebarGroup>
          <SidebarGroupLabel>
            {t("sidebar.about", "About")}
          </SidebarGroupLabel>
          <SidebarMenu>
            <SidebarMenuItem>
              <SidebarMenuButton asChild>
                <a href={"#"}>
                  <BadgeInfo />
                  <span>{t("sidebar.aboutNoraneko", "About Noraneko")}</span>
                </a>
              </SidebarMenuButton>
            </SidebarMenuItem>
            <SidebarMenuItem>
              <SidebarMenuButton asChild>
                <a href={"#"}>
                  <Wrench />
                  <span>{t("sidebar.debug", "Debug")}</span>
                </a>
              </SidebarMenuButton>
            </SidebarMenuItem>
          </SidebarMenu>
        </SidebarGroup>
      </SidebarContent>
      <SidebarFooter>
        <NavUser user={user} />
      </SidebarFooter>
      <SidebarRail />
    </Sidebar>
  );
}
