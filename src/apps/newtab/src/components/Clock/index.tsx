import { useEffect, useState } from "react";

export function Clock() {
  const [time, setTime] = useState(new Date());

  useEffect(() => {
    const timer = setInterval(() => {
      setTime(new Date());
    }, 1000);

    return () => clearInterval(timer);
  }, []);

  const hours = time.getHours().toString().padStart(2, "0");
  const minutes = time.getMinutes().toString().padStart(2, "0");

  return (
    <div className="bg-white/70 dark:bg-gray-800/70 backdrop-blur-md rounded-lg shadow-lg p-6 h-full flex flex-col items-center justify-center transition-all duration-200 hover:bg-white/80 dark:hover:bg-gray-700/80">
      <div className="flex flex-col items-center space-y-4">
        <div className="text-5xl font-bold text-gray-800 dark:text-gray-100 tracking-widest">
          <span className="inline-block w-[1.1em] text-center">{hours}</span>
          <span className="animate-pulse">:</span>
          <span className="inline-block w-[1.1em] text-center">{minutes}</span>
        </div>
        <div className="text-base font-medium text-gray-600 dark:text-gray-300">
          {time.toLocaleDateString(undefined, {
            weekday: "long",
          })}
        </div>
        <div className="text-sm text-gray-500 dark:text-gray-400 text-center">
          {time.toLocaleDateString(undefined, {
            year: "numeric",
            month: "long",
            day: "numeric",
          })}
        </div>
      </div>
    </div>
  );
}
