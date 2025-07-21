import { useLocation } from "react-router-dom";
import { BreadcrumbSeparator } from "../components/ui/breadcrumb.tsx";
import { Breadcrumb } from "../components/ui/breadcrumb.tsx";
import { SidebarTrigger } from "../components/ui/sidebar.tsx";
import { BreadcrumbPage } from "../components/ui/breadcrumb.tsx";
import { BreadcrumbList } from "../components/ui/breadcrumb.tsx";
import { BreadcrumbItem } from "../components/ui/breadcrumb.tsx";
import i18n from "../lib/i18n/i18n.ts";

export function Header() {
  const location = useLocation();
  const rawPath = location.pathname.replace(/^\//, "");
  const segments = rawPath ? rawPath.split("/") : [];
  const capitalizedSegments = segments.map(
    (seg) => seg.charAt(0).toUpperCase() + seg.slice(1),
  );

  return (
    <header className="flex h-16 shrink-0 items-center gap-2 transition-[width,height] ease-linear">
      <div className="flex items-center gap-2 px-4">
        <SidebarTrigger />
        <Breadcrumb>
          <BreadcrumbList>
            {segments.length === 0 ? (
              <BreadcrumbItem>
                <BreadcrumbPage>Dashboard</BreadcrumbPage>
              </BreadcrumbItem>
            ) : null}
            {capitalizedSegments.map((name, index) => (
              <div key={index} className="flex items-center gap-1.5">
                <BreadcrumbItem>
                  {index === capitalizedSegments.length - 1 ? (
                    <BreadcrumbPage>{name}</BreadcrumbPage>
                  ) : (
                    <BreadcrumbPage>{name}</BreadcrumbPage>
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
          i18n.changeLanguage(e.currentTarget.value)
        }
        className="p-2 ml-auto mr-4 rounded-md bg-gray-100 dark:bg-gray-800 dark:text-gray-100"
      >
        <option value="en">English</option>
        <option value="ja">日本語</option>
      </select>
    </header>
  );
}
