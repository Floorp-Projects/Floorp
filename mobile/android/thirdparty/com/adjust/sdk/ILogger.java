package com.adjust.sdk;

public interface ILogger {
    public void setLogLevel(LogLevel logLevel);

    public void setLogLevelString(String logLevelString);

    public void verbose(String message, Object... parameters);

    public void debug(String message, Object... parameters);

    public void info(String message, Object... parameters);

    public void warn(String message, Object... parameters);

    public void error(String message, Object... parameters);

    public void Assert(String message, Object... parameters);

}
