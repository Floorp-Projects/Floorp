#include "config.h"

#include <stdlib.h>

#define main testglib_main
#include <testglib.c>
#undef main

#define TEST_PRIVATE_THREADS 9
#define TEST_PRIVATE_ROUNDS 5

void
test_mutexes (void)
{
  GMutex *mutex = NULL;
  GCond *cond = NULL;
  GStaticMutex static_mutex = G_STATIC_MUTEX_INIT;
  G_LOCK_DEFINE (test_me);

  if (g_thread_supported ())
    {
      mutex = g_mutex_new ();
      cond = g_cond_new ();
    }

  g_mutex_lock (mutex);
  g_mutex_unlock (mutex);

  g_static_mutex_lock (&static_mutex);
  g_static_mutex_unlock (&static_mutex);

  g_cond_signal (cond);
  g_cond_broadcast (cond);

  G_LOCK (test_me);
  G_UNLOCK (test_me);

  if (g_thread_supported ())
    {
      g_cond_free (cond);
      g_mutex_free (mutex);
    }
}

#if defined(G_THREADS_IMPL_NSPR)	
#warning "note, that you have to link with whatever library"
#warning "nspr is building upon, it might otherwise (as on solaris) lead to"
#warning "run time failure, as the mutex functions are defined in libc, but"
#warning "as noops, that will make some nspr assertions fail."
#include <prthread.h>

gpointer
new_thread (GHookFunc func, gpointer data)
{
  PRThread *thread = PR_CreateThread (PR_SYSTEM_THREAD, func, data,
				      PR_PRIORITY_NORMAL, PR_LOCAL_THREAD,
				      PR_JOINABLE_THREAD, 0);
  return thread;
}
#define join_thread(thread) PR_JoinThread (thread)
#define self_thread() PR_GetCurrentThread ()

#elif defined(G_THREADS_IMPL_SOLARIS)	
#include <thread.h>

gpointer
new_thread (GHookFunc func, gpointer data)
{
  thread_t thread;
  thr_create (NULL, 0, (void *(*)(void *)) func, data, THR_BOUND, &thread);
  return GUINT_TO_POINTER (thread);
}
#define join_thread(thread) \
  thr_join ((thread_t)GPOINTER_TO_UINT (thread), NULL, NULL)
#define self_thread()  GUINT_TO_POINTER (thr_self ())

#elif defined(G_THREADS_IMPL_POSIX)
#include <pthread.h>

gpointer
new_thread(GHookFunc func, gpointer data)
{
  pthread_t thread;
  pthread_attr_t pthread_attr;
  pthread_attr_init (&pthread_attr);
  /* This is the default, it seems, so leave that out for now 
     pthread_attr_setdetachstate (&pthread_attr, PTHREAD_CREATE_JOINABLE);
  */
  pthread_create (&thread, &pthread_attr, (void *(*)(void *)) func, data);
  return GUINT_TO_POINTER (thread);
}
#define join_thread(thread) \
  pthread_join ((pthread_t)GPOINTER_TO_UINT (thread), NULL)
#define self_thread() GUINT_TO_POINTER (pthread_self ())

#else /* we are not having a thread implementation, do nothing */

#define new_thread(func,data) (NULL)
#define join_thread(thread) ((void)0)
#define self_thread() NULL

#endif

#define G_MICROSEC 1000000

void
wait_thread (double seconds)
{
  GMutex *mutex;
  GCond *cond;
  GTimeVal current_time;

  g_get_current_time (&current_time);
  mutex = g_mutex_new ();
  cond = g_cond_new ();

  current_time.tv_sec += (guint) seconds;
  seconds -= (guint) seconds;
  current_time.tv_usec += (guint) (seconds * G_MICROSEC);
  while (current_time.tv_usec >= G_MICROSEC)
    {
      current_time.tv_usec -= G_MICROSEC;
      current_time.tv_sec++;
    }

  g_mutex_lock (mutex);
  g_cond_timed_wait (cond, mutex, &current_time);
  g_mutex_unlock (mutex);

  g_mutex_free (mutex);
  g_cond_free (cond);
}

gpointer
private_constructor (void)
{
  gpointer *result = g_new (gpointer, 2);
  result[0] = 0;
  result[1] = self_thread ();
  g_print ("allocating data for the thread %p.\n", result[1]);
  return result;
}

void
private_destructor (gpointer data)
{
  gpointer *real = data;
  g_print ("freeing data for the thread %p.\n", real[1]);
  g_free (real);
}

GStaticPrivate private_key;

void
test_private_func (void *data)
{
  guint i = 0;
  static unsigned int seed = 0;
  if (!seed)
    {
      GTimeVal now;
      g_get_current_time (&now);
      seed = now.tv_usec;
    }
  wait_thread (1);
  while (i < TEST_PRIVATE_ROUNDS)
    {
#ifdef HAVE_RAND_R
      guint random_value = rand_r (&seed) % 10000;
#else
      guint random_value = rand() % 10000;
#endif
      guint *data = g_static_private_get (&private_key);
      if (!data)
	{
	  data = private_constructor ();
	  g_static_private_set (&private_key, data, private_destructor);
	}
      *data = random_value;
      wait_thread (.2);
      g_assert (*(guint *) g_static_private_get (&private_key) == random_value);
      i++;
    }
}

void
test_private (void)
{
  int i;
  gpointer threads[TEST_PRIVATE_THREADS];
  for (i = 0; i < TEST_PRIVATE_THREADS; i++)
    {
      threads[i] = new_thread (test_private_func, GINT_TO_POINTER(i));
    }
  for (i = 0; i < TEST_PRIVATE_THREADS; i++)
    {
      join_thread (threads[i]);
    }
  g_print ("\n");
}

int
main (void)
{
  test_mutexes ();

  g_thread_init (NULL);

  test_mutexes ();

  test_private ();

  /* later we might want to start n copies of that */
  testglib_main (0, NULL);

  return 0;
}
