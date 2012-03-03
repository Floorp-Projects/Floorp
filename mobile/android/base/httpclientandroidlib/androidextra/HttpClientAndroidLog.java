package ch.boye.httpclientandroidlib.androidextra;

import android.util.Log;

public class HttpClientAndroidLog {
	
	private String logTag;
	private boolean debugEnabled;
	private boolean errorEnabled;
	private boolean warnEnabled;
	private boolean infoEnabled;
	
	public HttpClientAndroidLog(Object tag) {
		logTag=tag.toString();
		debugEnabled=false;
		errorEnabled=false;
		warnEnabled=false;
		infoEnabled=false;
	}
	
	public void enableDebug(boolean enable) {
		debugEnabled=enable;
	}

	public boolean isDebugEnabled() {
		return debugEnabled;
	}
	
	public void debug(Object message) {
		Log.d(logTag, message.toString());
	}

	public void debug(Object message, Throwable t) {
		Log.d(logTag, message.toString(), t);
	}
	
	public void enableError(boolean enable) {
		errorEnabled=enable;
	}

	public boolean isErrorEnabled() {
		return errorEnabled;
	}
	
	public void error(Object message) {
		Log.e(logTag, message.toString());
	}

	public void error(Object message, Throwable t) {
		Log.e(logTag, message.toString(), t);
	}
	
	public void enableWarn(boolean enable) {
		warnEnabled=enable;
	}

	public boolean isWarnEnabled() {
		return warnEnabled;
	}
	
	public void warn(Object message) {
		Log.w(logTag, message.toString());
	}

	public void warn(Object message, Throwable t) {
		Log.w(logTag, message.toString(), t);
	}
	
	public void enableInfo(boolean enable) {
		infoEnabled=enable;
	}

	public boolean isInfoEnabled() {
		return infoEnabled;
	}
	
	public void info(Object message) {
		Log.i(logTag, message.toString());
	}

	public void info(Object message, Throwable t) {
		Log.i(logTag, message.toString(), t);
	}
	
}
