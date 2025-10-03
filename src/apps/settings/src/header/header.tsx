import { useLocation } from "react-router-dom";
import {
  Breadcrumb,
  BreadcrumbItem,
  BreadcrumbList,
  BreadcrumbPage,
  BreadcrumbSeparator,
} from "@/components/common/breadcrumb.tsx";
import { SidebarTrigger } from "@/components/common/sidebar.tsx";
import { SettingsSearchInput } from "./SearchInput.tsx";

export function Header() {
  const location = useLocation();
  const rawPath = location.pathname.replace(/^\//, "");
  const segments = rawPath ? rawPath.split("/") : [];
  const capitalizedSegments = segments.map((seg) =>
    seg.charAt(0).toUpperCase() + seg.slice(1)
  );

  return (
    <header className="flex flex-wrap items-center gap-3 px-4 md:px-8 transition-[width,height] ease-linear md:h-16">
      <div className="flex items-center gap-2">
        <SidebarTrigger />
        <Breadcrumb>
          <BreadcrumbList className="flex gap-1.5">
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
      <div className="w-full md:ml-auto md:w-auto md:flex-1 md:max-w-md">
        <SettingsSearchInput />
      </div>
    </header>
  );
}
