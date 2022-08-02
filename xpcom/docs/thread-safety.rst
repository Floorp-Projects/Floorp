**Thread safety analysis in Gecko**
===================================

Clang thread-safety analysis is supported in Gecko. This means
builds will generate warnings when static analysis detects an issue with
locking of mutex/monitor-protected members and data structures. Note
that Chrome uses the same feature. An example warning: ::

  warning: dom/media/AudioStream.cpp:504:22 [-Wthread-safety-analysis]
  reading variable 'mDataSource' requires holding mutex 'mMonitor'

If your patch causes warnings like this, you’ll need to resolve them;
they will be errors on checkin.

This analysis depends on thread-safety attributions in the source. These
have been added to Mozilla’s Mutex and Monitor classes and subclasses,
but in practice the analysis is largely dependent on additions to the
code being checked, in particular adding MOZ_GUARDED_BY(mutex) attributions
on the definitions of member variables. Like this: ::

  mozilla::Mutex mLock;
  bool mShutdown MOZ_GUARDED_BY(mLock);

For background on Clang’s thread-safety support, see `their
documentation <https://clang.llvm.org/docs/ThreadSafetyAnalysis.html>`__.

Newly added Mutexes and Monitors **MUST** use thread-safety annotations,
and we are enabling static checks to verify this. Legacy uses of Mutexes
and Monitors are marked with MOZ_UNANNOTATED.

If you’re modifying code that has been annotated with
MOZ_GUARDED_BY()/MOZ_REQUIRES()/etc, you should **make sure that the annotations
are updated properly**; e.g. if you change the thread-usage of a member
variable or method you should mark it accordingly, comment, and resolve
any warnings. Since the warnings will be errors in autoland/m-c, you
won’t be able to land code with active warnings.

**Annotating locking and usage requirements in class definitions**
------------------------------------------------------------------

Values that require a lock to access, or which are simply used from more
than one thread, should always have documentation in the definition
about the locking requirements and/or what threads it’s touched from: ::

  // This array is accessed from both the direct video thread, and the graph
  // thread. Protected by mMutex.

  nsTArray<std::pair<ImageContainer::FrameID, VideoChunk>> mFrames
  MOZ_GUARDED_BY(mMutex);

  // Set on MainThread, deleted on either MainThread mainthread, used on
  // MainThread or IO Thread in DoStopSession
  nsCOMPtr<nsITimer> mReconnectDelayTimer MOZ_GUARDED_BY(mMutex);

It’s strongly recommended to group values by access pattern, but it’s
**critical** to make it clear what the requirements to access a value
are. With values protected by Mutexes and Monitors, adding a
MOZ_GUARDED_BY(mutex/monitor) should be sufficient, though you may want to
also document what threads access it, and if they read or write to it.

Values which have more complex access requirements (see single-writer
and time-based-locking below) need clear documentation where they’re
defined: ::

  MutexSingleWriter mMutex;

  // mResource should only be modified on the main thread with the lock.
  // So it can be read without lock on the main thread or on other threads
  // with the lock.
  RefPtr<ChannelMediaResource> mResource MOZ_GUARDED_BY(mMutex);

**WARNING:** thread-safety analysis is not magic; it depends on you telling
it the requirements around access. If you don’t mark something as
MOZ_GUARDED_BY() it won’t figure it out for you, and you can end up with a data
race. When writing multithreaded code, you should always be thinking about
which threads can access what and when, and document this.

**How to annotate different locking patterns in Gecko**
-------------------------------------------------------

Gecko uses a number of different locking patterns. They include:

-  **Always Lock** -
   Multiple threads may read and write the value

-  **Single Writer** -
   One thread does all the writing, other threads
   read the value, but code on the writing thread also reads it
   without the lock

-  **Out-of-band invariants** -
   A value may be accessed from other threads,
   but only after or before certain other events or in a certain state,
   like when after a listener has been added or before a processing
   thread has been shut down.

The simplest and easiest to check with static analysis is **Always
Lock**, and generally you should prefer this pattern. This is very
simple; you add MOZ_GUARDED_BY(mutex/monitor), and must own the lock to
access the value. This can be implemented by some combination of direct
Lock/AutoLock calls in the method; an assertion that the lock is already
held by the current thread, or annotating the method as requiring the
lock (MOZ_REQUIRES(mutex)) in the method definition: ::

  // Ensures mSize is initialized, if it can be.
  // mLock must be held when this is called, and mInput must be non-null.
  void EnsureSizeInitialized() MOZ_REQUIRES(mLock);
  ...
  // This lock protects mSeekable, mInput, mSize, and mSizeInitialized.
  Mutex mLock;
  int64_t mSize MOZ_GUARDED_BY(mLock);

**Single Writer** is tricky for static analysis normally, since it
doesn’t know what thread an access will occur on. In general, you should
prefer using Always Lock in non-performance-sensitive code, especially
since these mutexes are almost always uncontended and therefore cheap to
lock.

To support this fairly common pattern in Mozilla code, we’ve added
MutexSingleWriter and MonitorSingleWriter subclasses. To use these, you
need to subclass SingleWriterLockOwner on one object (typically the
object containing the Mutex), implement ::OnWritingThread(), and pass
the object to the constructor for MutexSingleWriter. In code that
accesses the guarded value from the writing thread, you need to add
mMutex.AssertIsOnWritingThread(), which both does a debug-only runtime
assertion by calling OnWritingThread(), and also asserts to the static
analyzer that the lock is held (which it isn’t).

There is one case this causes problems with: when a method needs to
access the value (without the lock), and then decides to write to the
value from the same method, taking the lock. To the static analyzer,
this looks like a double-lock. Either you will need to add
MOZ_NO_THREAD_SAFETY_ANALYSIS to the method, move the write into another
method you call, or locally disable the warning with
MOZ_PUSH_IGNORE_THREAD_SAFETY and MOZ_POP_THREAD_SAFETY. We’re discussing with
the clang static analysis developers how to better handle this.

Note also that this provides no checking that the lock is taken to write
to the value: ::

  MutexSingleWriter mMutex;
  // mResource should only be modified on the main thread with the lock.
  // So it can be read without lock on the main thread or on other threads
  // with the lock.
  RefPtr<ChannelMediaResource> mResource MOZ_GUARDED_BY(mMutex);
  ...
  nsresult ChannelMediaResource::Listener::OnStartRequest(nsIRequest *aRequest) {
    mMutex.AssertOnWritingThread();

    // Read from the only writing thread; no lock needed
    if (!mResource) {
      return NS_OK;
    }
    return mResource->OnStartRequest(aRequest, mOffset);
  }

If you need to assert you’re on the writing thread, then later take a
lock to modify a value, it will cause a warning: ”acquiring mutex
'mMutex' that is already held”. You can resolve this by turning off
thread-safety analysis for the lock: ::

  mMutex.AssertOnWritingThread();
  ...
  {
    MOZ_PUSH_IGNORE_THREAD_SAFETY
    MutexSingleWriterAutoLock lock(mMutex);
    MOZ_POP_THREAD_SAFETY

**Out-of-band Invariants** is used in a number of places (and may be
combined with either of the above patterns). It's using other knowledge
about the execution pattern of the code to assert that it's safe to avoid
taking certain locks.   A primary example is when a value can
only be accessed from a single thread for part of its lifetime (this can
also be referred to as "time-based locking").

Note that thread-safety analysis always ignores constructors and destructors
(which shouldn’t have races with other threads barring really odd usages).
Since only a single thread can access during those time periods, locking is
not required there.  However, if a method is called from a constructor,
that method may generate warnings since the compiler doesn't know if it
might be called from elsewhere: ::

  ...
  class nsFoo {
  public:
    nsFoo() {
      mBar = true; // Ok since we're in the constructor, no warning
      Init();
    }
    void Init() {  // we're only called from the constructor
      // This causes a thread-safety warning, since the compiler
      // can't prove that Init() is only called from the constructor
      mQuerty = true;
    }
    ...
    mMutex mMutex;
    uint32_t mBar MOZ_GUARDED_BY(mMutex);
    uint32_t mQuerty MOZ_GUARDED_BY(mMutex);
  }

Another example might be a value that’s used from other threads, but only
if an observer has been installed. Thus code that always runs before the
observer is installed, or after it’s removed, does not need to lock.

These patterns are impossible to statically check in most cases. If all
the periods where it’s accessed from one thread only are on the same
thread, you could use the Single Writer pattern support to cover this
case. You would add AssertIsOnWritingThread() calls to methods that meet
the criteria that only a single thread can access the value (but only if
that holds). Unlike regular uses of SingleWriter, however, there’s no way
to check if you added such an assertion to code that runs on the “right”
thread, but during a period where another thread might modify it.

For this reason, we **strongly** suggest that you convert cases of
Out-of-band-invariants/Time-based-locking to Always Lock if you’re
refactoring the code or making major modifications. This is far less prone
to error, and also to future changes breaking the assumptions about other
threads accessing the value. In all but a few cases where code is on a very
‘hot’ path, this will have no impact on performance - taking an uncontended
lock is cheap.

To quiet warnings where these patterns are in use, you'll need to either
add locks (preferred), or suppress the warnings with MOZ_NO_THREAD_SAFETY_ANALYSIS or
MOZ_PUSH_IGNORE_THREAD_SAFETY/MOZ_POP_THREAD_SAFETY.

**This pattern especially needs good documentation in the code as to what
threads will access what members under what conditions!**::

  // Can't be accessed by multiple threads yet
  nsresult nsAsyncStreamCopier::InitInternal(nsIInputStream* source,
                                             nsIOutputStream* sink,
					     nsIEventTarget* target,
					     uint32_t chunkSize,
					     bool closeSource,
					     bool closeSink)
	MOZ_NO_THREAD_SAFETY_ANALYSIS {

and::

  // We can't be accessed by another thread because this hasn't been
  // added to the public list yet
  MOZ_PUSH_IGNORE_THREAD_SAFETY
  mRestrictedPortList.AppendElement(gBadPortList[i]);
  MOZ_POP_THREAD_SAFETY

and::

  // This is called on entries in another entry's mCallback array, under the lock
  // of that other entry. No other threads can access this entry at this time.
  bool CacheEntry::Callback::DeferDoom(bool* aDoom) const {

**Known limitations**
---------------------

**Static analysis can’t handle all reasonable patterns.** In particular,
per their documentation, it can’t handle conditional locks, like: ::

  if (OnMainThread()) {
    mMutex.Lock();
  }

You should resolve this either via MOZ_NO_THREAD_SAFETY_ANALYSIS on the
method, or MOZ_PUSH_IGNORE_THREAD_SAFETY/MOZ_POP_THREAD_SAFETY.

**Sometimes the analyzer can’t figure out that two objects are both the
same Mutex**, and it will warn you. You may be able to resolve this by
making sure you’re using the same pattern to access the mutex: ::

   mChan->mMonitor->AssertCurrentThreadOwns();
   ...
   {
 -    MonitorAutoUnlock guard(*monitor);
 +    MonitorAutoUnlock guard(*(mChan->mMonitor.get())); // avoids mutex warning
     ok = node->SendUserMessage(port, std::move(aMessage));
   }

**Maybe<MutexAutoLock>** doesn’t tell the static analyzer when the mutex
is owned or freed; follow locking via the MayBe<> by
**mutex->AssertCurrentThreadOwns();** (and ditto for Monitors): ::

  Maybe<MonitorAutoLock> lock(std::in_place, *mMonitor);
  mMonitor->AssertCurrentThreadOwns(); // for threadsafety analysis

If you reset() the Maybe<>, you may need to surround it with
MOZ_PUSH_IGNORE_THREAD_SAFETY and MOZ_POP_THREAD_SAFETY macros: ::

  MOZ_PUSH_IGNORE_THREAD_SAFETY
  aLock.reset();
  MOZ_POP_THREAD_SAFETY

**Passing a protected value by-reference** sometimes will confuse the
analyzer. Use MOZ_PUSH_IGNORE_THREAD_SAFETY and MOZ_POP_THREAD_SAFETY macros to
resolve this.

**Classes which need thread-safety annotations**
------------------------------------------------

-  Mutex

-  StaticMutex

-  RecursiveMutex

-  BaseProfilerMutex

-  Monitor

-  StaticMonitor

-  ReentrantMonitor

-  RWLock

-  Anything that hides an internal Mutex/etc and presents a Mutex-like
      API (::Lock(), etc).

**Additional Notes**
--------------------

Some code passes **Proof-of-Lock** AutoLock parameters, as a poor form of
static analysis. While it’s hard to make mistakes if you pass an AutoLock
reference, it is possible to pass a lock to the wrong Mutex/Monitor.

Proof-of-lock is basically redundant to MOZ_REQUIRES() and obsolete, and
depends on the optimizer to remove it, and per above it can be misused,
with effort.  With MOZ_REQUIRES(), any proof-of-lock parameters can be removed,
though you don't have to do so immediately.

In any method taking an aProofOfLock parameter, add a MOZ_REQUIRES(mutex) to
the definition (and optionally remove the proof-of-lock), or add a
mMutex.AssertCurrentThreadOwns() to the method: ::

    nsresult DispatchLockHeld(already_AddRefed<WorkerRunnable> aRunnable,
 -                            nsIEventTarget* aSyncLoopTarget,
 -                            const MutexAutoLock& aProofOfLock);
 +                            nsIEventTarget* aSyncLoopTarget) MOZ_REQUIRES(mMutex);

or (if for some reason it's hard to specify the mutex in the header)::

    nsresult DispatchLockHeld(already_AddRefed<WorkerRunnable> aRunnable,
 -                            nsIEventTarget* aSyncLoopTarget,
 -                            const MutexAutoLock& aProofOfLock);
 +                            nsIEventTarget* aSyncLoopTarget) {
 +  mMutex.AssertCurrentThreadOwns();

In addition to MOZ_GUARDED_BY() there’s also MOZ_PT_GUARDED_BY(), which says
that the pointer isn’t guarded, but the data pointed to by the pointer
is.

Classes that expose a Mutex-like interface can be annotated like Mutex;
see some of the examples in the tree that use MOZ_CAPABILITY and
MOZ_ACQUIRE()/MOZ_RELEASE().

Shared locks are supported, though we don’t use them much. See RWLock.
