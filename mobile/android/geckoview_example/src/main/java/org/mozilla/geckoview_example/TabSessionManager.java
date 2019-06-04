package org.mozilla.geckoview_example;

import android.support.annotation.Nullable;

import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoSessionSettings;

import java.util.ArrayList;

public class TabSessionManager {
    private static ArrayList<TabSession> mTabSessions = new ArrayList<TabSession>();
    private int mCurrentSessionIndex = 0;

    public TabSessionManager() {
    }

    public void addSession(TabSession session) {
        mTabSessions.add(session);
    }

    public TabSession getSession(int index) {
        return mTabSessions.get(index);
    }

    public TabSession getCurrentSession() {
        return getSession(mCurrentSessionIndex);
    }

    public TabSession getSession(GeckoSession session) {
        int index = mTabSessions.indexOf(session);
        if (index == -1) {
            return null;
        }
        return getSession(index);
    }

    public void setCurrentSession(TabSession session) {
        int index = mTabSessions.indexOf(session);
        if (index == -1) {
            mTabSessions.add(session);
            index = mTabSessions.size() - 1;
        }
        mCurrentSessionIndex = index;
    }

    private boolean isCurrentSession(TabSession session) {
        return session == getCurrentSession();
    }

    public void closeSession(@Nullable TabSession session) {
        if (session == null) { return; }
        if (isCurrentSession(session)
            && mCurrentSessionIndex == mTabSessions.size() - 1) {
            --mCurrentSessionIndex;
        }
        session.close();
        mTabSessions.remove(session);
    }

    public TabSession newSession(GeckoSessionSettings settings) {
        TabSession tabSession = new TabSession(settings);
        mTabSessions.add(tabSession);
        return tabSession;
    }

    public int sessionCount() {
        return mTabSessions.size();
    }

    public ArrayList<TabSession> getSessions() {
        return mTabSessions;
    }
}
