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

  return (
    <>
      <Background />
      <div className="relative w-full min-h-screen">
        {components.topSites && <TopSites />}

        <div className="fixed top-40 left-1/2 transform -translate-x-1/2 w-full max-w-xl">
          {components.searchBar && <SearchBar />}
        </div>

        <div className="fixed top-4 right-4 flex flex-col gap-4 max-w-sm">
          {components.clock && <Clock />}
        </div>

        <SettingsButton onClick={() => setIsSettingsOpen(true)} />
        <Settings
          isOpen={isSettingsOpen}
          onClose={() => setIsSettingsOpen(false)}
        />
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
