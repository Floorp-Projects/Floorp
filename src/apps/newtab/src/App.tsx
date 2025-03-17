import { useState } from "react";
import { Grid } from "./components/Grid.tsx";
import { TopSites } from "./components/TopSites/index.tsx";
import { Clock } from "./components/Clock/index.tsx";
import { Dashboard } from "./components/Dashboard/index.tsx";
import { Background } from "./components/Background/index.tsx";
import { BackgroundProvider } from "./contexts/BackgroundContext.tsx";
import { ComponentsProvider } from "./contexts/ComponentsContext.tsx";
import { Settings } from "./components/Settings/index.tsx";
import { SettingsButton } from "./components/SettingsButton/index.tsx";
import "./globals.css";
import { useComponents } from "./contexts/ComponentsContext.tsx";

function NewTabContent() {
  const [isSettingsOpen, setIsSettingsOpen] = useState(false);
  const { components } = useComponents();

  return (
    <>
      <Background />
      <div className="relative w-full min-h-screen p-8">
        <Grid>
          {components.dashboard && (
            <div className="col-span-full lg:col-span-2">
              <Dashboard />
            </div>
          )}
          {components.topSites && (
            <div className="col-span-full lg:col-span-2 lg:row-start-2">
              <TopSites />
            </div>
          )}
          {components.clock && (
            <div className="lg:row-span-2">
              <Clock />
            </div>
          )}
        </Grid>

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
