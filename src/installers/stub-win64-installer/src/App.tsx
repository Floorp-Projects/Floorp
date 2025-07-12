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
  const [cpuCheckComplete, setCpuCheckComplete] = useState(false);
  const [cpuSupported, setCpuSupported] = useState(true);

  useEffect(() => {
    // Check CPU support on app startup
    const checkCpuSupport = async () => {
      try {
        const supported = await invoke<boolean>("check_cpu_support");
        setCpuSupported(supported);
        setCpuCheckComplete(true);

        if (!supported) {
          setError("rust.errors.cpu_not_supported");
        }
      } catch (e) {
        console.error("Failed to check CPU support:", e);
        setCpuSupported(false);
        setCpuCheckComplete(true);
        setError("rust.errors.cpu_check_failed");
      }
    };

    checkCpuSupport();
  }, []);

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

  // Show loading state while checking CPU support
  if (!cpuCheckComplete) {
    return (
      <div className="min-h-screen max-h-screen bg-base-100 flex flex-col overflow-hidden">
        <BackgroundDecoration />
        <NavBar />
        <main className="flex-1 container mx-auto p-4 overflow-auto relative z-10 flex flex-col items-center justify-center">
          <div className="card bg-base-100 shadow-lg p-8 text-center">
            <div className="loading loading-spinner loading-lg text-primary mb-4"></div>
            <h2 className="text-xl font-bold mb-2">システムチェック中...</h2>
            <p className="text-sm opacity-70">CPU要件を確認しています</p>
          </div>
        </main>
      </div>
    );
  }

  // Show error if CPU is not supported
  if (!cpuSupported) {
    return (
      <div className="min-h-screen max-h-screen bg-base-100 flex flex-col overflow-hidden">
        <BackgroundDecoration />
        <NavBar />
        <main className="flex-1 container mx-auto p-4 overflow-auto relative z-10 flex flex-col items-center justify-center">
          <div className="card bg-base-100 shadow-lg p-8 text-center max-w-md">
            <div className="text-error mb-4">
              <svg className="w-16 h-16 mx-auto" fill="currentColor" viewBox="0 0 20 20">
                <path fillRule="evenodd" d="M18 10a8 8 0 11-16 0 8 8 0 0116 0zm-7 4a1 1 0 11-2 0 1 1 0 012 0zm-1-9a1 1 0 00-1 1v4a1 1 0 102 0V6a1 1 0 00-1-1z" clipRule="evenodd" />
              </svg>
            </div>
            <h2 className="text-xl font-bold mb-4">サポートされていないデバイス</h2>
            <p className="text-sm mb-6">
              FloorpはSSE4.1をサポートするプロセッサーが必要です。<br />
              このデバイスはサポート対象外です。
            </p>
            <button 
              className="btn btn-error" 
              onClick={() => window.close()}
            >
              終了
            </button>
          </div>
        </main>
      </div>
    );
  }

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