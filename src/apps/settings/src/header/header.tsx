import { useLocation } from "react-router-dom";
import {
  Breadcrumb,
  BreadcrumbItem,
  BreadcrumbList,
  BreadcrumbPage,
  BreadcrumbSeparator,
} from "@/components/common/breadcrumb.tsx";
import { SidebarTrigger } from "@/components/common/sidebar.tsx";
import i18n from "../lib/i18n/i18n.ts";

export function Header() {
  const location = useLocation();
  const rawPath = location.pathname.replace(/^\//, "");
  const segments = rawPath ? rawPath.split("/") : [];
  const capitalizedSegments = segments.map((seg) =>
    seg.charAt(0).toUpperCase() + seg.slice(1)
  );

  return (
    <header className="flex h-16 shrink-0 items-center gap-2 transition-[width,height] ease-linear">
      <div className="flex items-center gap-2 px-8">
        <SidebarTrigger />
        <Breadcrumb>
          <BreadcrumbList className=" flex gap-1.5">
            {segments.length === 0
              ? (
                <BreadcrumbItem>
                  <BreadcrumbPage>Dashboard</BreadcrumbPage>
                </BreadcrumbItem>
              )
              : null}
            {capitalizedSegments.map((name, index) => (
              <div key={index} className="flex items-center gap-1.5">
                <BreadcrumbItem>
                  {index === capitalizedSegments.length - 1
                    ? <BreadcrumbPage>{name}</BreadcrumbPage>
                    : (
                      <BreadcrumbPage>
                        {name}
                      </BreadcrumbPage>
                    )}
                </BreadcrumbItem>
                {index < capitalizedSegments.length - 1 && (
                  <BreadcrumbSeparator />
                )}
              </div>
            ))}
          </BreadcrumbList>
        </Breadcrumb>
      </div>
      <select
        value={i18n.language}
        onChange={(e: React.ChangeEvent<HTMLSelectElement>) =>
          i18n.changeLanguage(e.currentTarget.value)}
        className="p-2 ml-auto mr-4 rounded-md bg-base-200 dark:bg-base-300 text-base-content"
      >
        <option value="en">English</option>
        <option value="ja">日本語</option>
      </select>
    </header>
  );
}
