import { useState } from "react";
import { TopSites } from "./components/TopSites/index.tsx";
import { Clock } from "./components/Clock/index.tsx";
import { Background } from "./components/Background/index.tsx";
import { BackgroundProvider } from "./contexts/BackgroundContext.tsx";
import { ComponentsProvider } from "./contexts/ComponentsContext.tsx";
import { Settings } from "./components/Settings/index.tsx";
import { SettingsButton } from "./components/SettingsButton/index.tsx";
import { SearchBar } from "./components/SearchBar/index.tsx";
import "./globals.css";
import { useComponents } from "./contexts/ComponentsContext.tsx";

function NewTabContent() {
  const [isSettingsOpen, setIsSettingsOpen] = useState(false);
  const { components } = useComponents();

  // コンポーネント設定のデバッグ
  console.log("🔍 [App Debug] Components configuration:", components);
  console.log("🔍 [App Debug] TopSites enabled:", components.topSites);
  console.log("🔍 [App Debug] Clock enabled:", components.clock);
  console.log("🔍 [App Debug] SearchBar enabled:", components.searchBar);

  return (
    <>
      <Background />
      <div className="relative w-full min-h-screen">
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-4 p-4 w-full">
          <div className="lg:col-span-2 mb-10">
            {/* Debug: Checking TopSites render condition */}
            {(() => {
              console.log(
                "🔍 [App Debug] Checking if TopSites should render:",
                components.topSites,
              );
              return null;
            })()}
            {components.topSites
              ? (
                <>
                  {(() => {
                    console.log("🔍 [App Debug] Rendering TopSites component");
                    return null;
                  })()}
                  <TopSites />
                </>
              )
              : (
                <>
                  {(() => {
                    console.log(
                      "🔍 [App Debug] TopSites component is disabled",
                    );
                    return null;
                  })()}
                </>
              )}
          </div>

          <div className="flex justify-center lg:justify-end h-fit">
            {components.clock && <Clock />}
          </div>
        </div>

        <div className="w-full flex justify-center px-4">
          <div className="w-full max-w-full sm:max-w-md md:max-w-lg lg:max-w-xl mt-8 md:mt-16">
            {components.searchBar && <SearchBar />}
          </div>
        </div>

        <div className="fixed bottom-4 right-4">
          <SettingsButton onClick={() => setIsSettingsOpen(true)} />
          <Settings
            isOpen={isSettingsOpen}
            onClose={() => setIsSettingsOpen(false)}
          />
        </div>
      </div>
    </>
  );
}

export default function App() {
  return (
    <ComponentsProvider>
      <BackgroundProvider>
        <NewTabContent />
      </BackgroundProvider>
    </ComponentsProvider>
  );
}
