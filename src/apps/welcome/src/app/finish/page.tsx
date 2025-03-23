import { useEffect } from "react";
import Navigation from "../../components/Navigation";
import { useTranslation } from "react-i18next";

export default function FinishPage() {
    const { t } = useTranslation();

    const setAsDefaultBrowser = () => {
        console.log("Setting Floorp as default browser");
    };

    const closeWelcomePage = () => {
        window.open("about:newtab", "_blank");
        setTimeout(() => {
            window.close();
        }, 50);
    };

    useEffect(() => {
        interface Particle {
            x: number;
            y: number;
            r: number;
            d: number;
            color: string;
            tilt: number;
            opacity: number;
            shapeType: number;
        }

        const setupConfetti = () => {
            const colors = [
                "#f44336", "#e91e63", "#9c27b0", "#673ab7", "#3f51b5",
                "#2196f3", "#03a9f4", "#00bcd4", "#009688", "#4CAF50",
                "#8BC34A", "#CDDC39", "#FFEB3B", "#FFC107", "#FF9800", "#FF5722"
            ];
            const W = window.innerWidth;
            const H = window.innerHeight;

            const canvas = document.createElement("canvas");
            const context = canvas.getContext("2d");

            if (!context) {
                console.error("Canvas context is null");
                return;
            }

            canvas.width = W;
            canvas.height = H;
            canvas.style.position = "fixed";
            canvas.style.top = "0";
            canvas.style.left = "0";
            canvas.style.pointerEvents = "none";
            canvas.style.zIndex = "1000";
            document.body.appendChild(canvas);

            const particles: Particle[] = [];
            const particleCount = 20;
            const gravity = 1.5;

            for (let i = 0; i < particleCount; i++) {
                particles.push({
                    x: Math.random() * W,
                    y: Math.random() * H - H,
                    r: Math.random() * 5 + 2,
                    d: Math.random() * particleCount,
                    color: colors[Math.floor(Math.random() * colors.length)],
                    tilt: Math.floor(Math.random() * 10) - 10,
                    opacity: Math.random() + 0.5,
                    shapeType: Math.floor(Math.random() * 2),
                });
            }

            const draw = () => {
                context.clearRect(0, 0, W, H);

                particles.forEach(p => {
                    context.beginPath();
                    context.fillStyle = p.color;
                    context.globalAlpha = p.opacity;

                    if (p.shapeType === 0) {
                        context.fillRect(p.x, p.y, p.r * 2, p.r * 2);
                    } else {
                        context.arc(p.x, p.y, p.r, 0, Math.PI * 2, false);
                        context.fill();
                    }

                    context.closePath();

                    p.y += gravity + p.r / 2;
                    p.tilt += Math.random() * 4 - 2;

                    if (p.y > H) {
                        p.y = -20;
                        p.x = Math.random() * W;
                    }
                });

                requestAnimationFrame(draw);
            };

            draw();

            setTimeout(() => {
                document.body.removeChild(canvas);
            }, 8000);
        };

        setupConfetti();
    }, []);

    return (
        <main className="flex flex-col justify-center overflow-hidden">
            <div className="container mx-auto px-4 z-10 flex flex-col items-center">
                <div className="flex flex-col items-center justify-center mb-12">
                    <div className="relative mb-6">
                        <div className="w-36 h-36 rounded-full bg-gradient-to-br from-success to-success/70 flex items-center justify-center shadow-lg shadow-success/30 animate-pulse">
                            <svg xmlns="http://www.w3.org/2000/svg" className="h-20 w-20 text-white" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round">
                                <path d="M20 6L9 17l-5-5"></path>
                            </svg>
                        </div>

                        <div className="absolute inset-0 rounded-full border-4 border-dashed border-success/40 animate-[spin_10s_linear_infinite]"></div>
                    </div>

                    <h1 id="completion-title" className="text-7xl font-bold text-center text-success mb-8">
                        {t('finishPage.title')}
                    </h1>

                    <p className="text-xl text-center max-w-2xl animate-[fadeIn_1s_ease-out_forwards]">
                        {t('finishPage.subtitle')}
                    </p>
                </div>

                <div className="card bg-base-100 shadow-xl w-full max-w-2xl backdrop-blur-sm bg-base-100/80 border border-base-300 hover:shadow-2xl transition-all duration-300">
                    <div className="card-body">
                        <h2 className="flex items-center gap-2 text-xl font-semibold text-primary">
                            <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                                <path d="M12 2v4M12 18v4M4.93 4.93l2.83 2.83M16.24 16.24l2.83 2.83M2 12h4M18 12h4M4.93 19.07l2.83-2.83M16.24 7.76l2.83-2.83" />
                            </svg>
                            {t('finishPage.nextSteps')}
                        </h2>

                        <div className="flex flex-col md:flex-row gap-4 my-4 justify-center items-center">
                            <button
                                className="btn btn-primary btn-lg w-full md:w-auto shadow-lg hover:shadow-xl transform hover:-translate-y-1 transition-all duration-300"
                                onClick={setAsDefaultBrowser}
                            >
                                <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className="mr-2">
                                    <path d="M22 11.08V12a10 10 0 1 1-5.93-9.14" />
                                    <path d="m9 11 3 3L22 4" />
                                </svg>
                                {t('finishPage.defaultBrowser')}
                            </button>

                            <div className="flex gap-2">
                                {/* <a href="https://support.floorp.app" target="_blank" rel="noopener noreferrer" className="btn btn-ghost btn-sm">
                                    <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className="mr-1">
                                        <circle cx="12" cy="12" r="10" />
                                        <path d="M9.09 9a3 3 0 0 1 5.83 1c0 2-3 3-3 3" />
                                        <path d="M12 17h.01" />
                                    </svg>
                                    {t('finishPage.support')}
                                </a> */}
                                <a href="https://twitter.com/floorp_browser" target="_blank" rel="noopener noreferrer" className="btn btn-ghost btn-sm">
                                    <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className="mr-1">
                                        <path d="M22 4s-.7 2.1-2 3.4c1.6 10-9.4 17.3-18 11.6 2.2.1 4.4-.6 6-2C3 15.5.5 9.6 3 5c2.2 2.6 5.6 4.1 9 4-.9-4.2 4-6.6 7-3.8 1.1 0 3-1.2 3-1.2z" />
                                    </svg>
                                    Twitter
                                </a>
                                <a href="https://docs.floorp.app" target="_blank" rel="noopener noreferrer" className="btn btn-ghost btn-sm">
                                    <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className="mr-1">
                                        <path d="M4 19.5v-15A2.5 2.5 0 0 1 6.5 2H20v20H6.5a2.5 2.5 0 0 1 0-5H20" />
                                    </svg>
                                    {t('finishPage.documentation')}
                                </a>
                            </div>
                        </div>
                    </div>
                </div>

                <button
                    className="btn btn-success btn-lg shadow-2xl hover:shadow-success/20 transform hover:-translate-y-1 transition-all duration-300 mt-8 px-12 text-white font-bold text-xl"
                    onClick={closeWelcomePage}
                >
                    <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className="mr-2">
                        <path d="M5 12h14"></path>
                        <path d="m12 5 7 7-7 7"></path>
                    </svg>
                    {t('finishPage.getStarted')}
                </button>

                <p className="text-sm opacity-70 mt-4 text-center">
                    {t('finishPage.hint')}
                </p>
            </div>

            <div className="mt-12">
                <Navigation />
            </div>

            <style dangerouslySetInnerHTML={{
                __html: `
                @keyframes fadeIn {
                    from { opacity: 0; transform: translateY(20px); }
                    to { opacity: 1; transform: translateY(0); }
                }
            `}} />
        </main>
    );
}
