import { useState, useEffect } from "react";
import { invoke } from "@tauri-apps/api/core";
import { useTranslation } from "react-i18next";

import { useSlides } from './app/data/slides';
import BackgroundDecoration from './app/layout/BackgroundDecoration';
import NavBar from './app/layout/NavBar';
import InstallerWelcome from './components/InstallerWelcome';
import Slideshow from './components/Slideshow';
import ProgressBar from './components/ProgressBar';
import InstallComplete from './components/InstallComplete';

function App() {
  const { t } = useTranslation();
  const slides = useSlides();
  const [status, setStatus] = useState("");
  const [error, setError] = useState("");
  const [installing, setInstalling] = useState(false);
  const [completed, setCompleted] = useState(false);
  const [currentSlide, setCurrentSlide] = useState(0);

  useEffect(() => {
    let interval: number | undefined;

    if (installing && !completed) {
      interval = window.setInterval(() => {
        setCurrentSlide((prev) => (prev + 1) % slides.length);
      }, 8000)
    }

    return () => {
      if (interval) window.clearInterval(interval);
    };
  }, [installing, completed, slides.length]);

  useEffect(() => {
    const handleContextMenu = (event: MouseEvent) => {
      event.preventDefault();
    };

    document.addEventListener('contextmenu', handleContextMenu);

    return () => {
      document.removeEventListener('contextmenu', handleContextMenu);
    };
  }, []);

  const handleInstall = async (useAdmin: boolean, customInstallPath: string | null) => {
    try {
      setStatus(t("app.status.installing"));
      setError("");
      setInstalling(true);
      setCompleted(false);

      const result = await invoke<string>("download_and_run_installer", {
        useAdmin,
        customInstallPath
      });

      setStatus(t(result));
      setCompleted(true);
    } catch (e) {
      const errorMessage = e as string;

      if (errorMessage.includes('|')) {
        const [key, param] = errorMessage.split('|');
        setError(t(key, { 0: param }));
      } else {
        setError(t(errorMessage));
      }

      setStatus("");
      setCompleted(true);
    }
  };

  const resetInstaller = () => {
    setInstalling(false);
    setCompleted(false);
    setStatus("");
    setError("");
    setCurrentSlide(0);
  };

  const nextSlide = () => {
    setCurrentSlide((prev) => (prev + 1) % slides.length);
  };

  const prevSlide = () => {
    setCurrentSlide((prev) => (prev - 1 + slides.length) % slides.length);
  };

  const goToSlide = (index: number) => {
    setCurrentSlide(index);
  };

  return (
    <div className="min-h-screen max-h-screen bg-base-100 flex flex-col overflow-hidden">
      <BackgroundDecoration />
      <NavBar />
      <main className="flex-1 container mx-auto p-4 overflow-auto relative z-10 flex flex-col">
        {!installing && !completed ? <InstallerWelcome onInstall={handleInstall} /> : null}

        {installing && !completed ? (
          <Slideshow
            slides={slides}
            currentSlide={currentSlide}
            onNextSlide={nextSlide}
            onPrevSlide={prevSlide}
            onGoToSlide={goToSlide}
          />
        ) : null}

        {completed ? (
          <InstallComplete
            success={!error}
            message={error || status}
            onReset={resetInstaller}
          />
        ) : null}

        {installing && !completed && (
          <ProgressBar
            status={status}
            error={error}
            currentSlide={currentSlide}
            totalSlides={slides.length}
          />
        )}
      </main>
    </div>
  );
}

export default App;