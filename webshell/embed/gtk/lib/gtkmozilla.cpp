/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of the Original Code is Alexander. Portions
 * created by Alexander Larsson are Copyright (C) 1999
 * Alexander Larsson. All Rights Reserved. 
 */
#include <gtk/gtk.h>
#include "gtkmozilla.h"
#include "GtkMozillaContainer.h"

#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#ifndef NECKO
#include "nsINetService.h"
#else
#include "nsIIOService.h"
#endif // NECKO
//#include "nsXPComCIID.h"

static NS_DEFINE_IID(kIEventQueueServiceIID,
                     NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kEventQueueServiceCID,
                     NS_EVENTQUEUESERVICE_CID);

extern "C" void NS_SetupRegistry();

static void gtk_mozilla_realize(GtkWidget *widget);
static void gtk_mozilla_finalize (GtkObject *object);

typedef gboolean (*GtkSignal_BOOL__POINTER_INT) (GtkObject * object,
                                                 gpointer arg1,
                                                 gint arg2,
                                                 gpointer user_data);

extern "C" void 
gtk_mozilla_marshal_BOOL__POINTER_INT (GtkObject * object,
                                       GtkSignalFunc func,
                                       gpointer func_data,
                                       GtkArg * args)
{
  GtkSignal_BOOL__POINTER_INT rfunc;
  gboolean *return_val;
  return_val = GTK_RETLOC_BOOL (args[2]);
  rfunc = (GtkSignal_BOOL__POINTER_INT) func;
  *return_val = (*rfunc) (object,
                          GTK_VALUE_POINTER (args[0]),
                          GTK_VALUE_INT (args[1]),
                          func_data);
}

enum {
  WILL_LOAD_URL,
  BEGIN_LOAD_URL,
  END_LOAD_URL,
  LAST_SIGNAL
};

static guint mozilla_signals[LAST_SIGNAL] = { 0 };
static GtkLayoutClass *parent_class = NULL;

static void
gtk_mozilla_class_init (GtkMozillaClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;

  parent_class = (GtkLayoutClass *)gtk_type_class (GTK_TYPE_LAYOUT);

  mozilla_signals[WILL_LOAD_URL] =
    gtk_signal_new ("will_load_url",
                    GTK_RUN_LAST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkMozillaClass, will_load_url),
                    gtk_mozilla_marshal_BOOL__POINTER_INT,
                    GTK_TYPE_BOOL, 2,
                    GTK_TYPE_STRING,
                    GTK_TYPE_INT);
  
  mozilla_signals[BEGIN_LOAD_URL] =
    gtk_signal_new ("begin_load_url",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkMozillaClass, begin_load_url),
                    gtk_marshal_NONE__POINTER,
                    GTK_TYPE_NONE, 1,
                    GTK_TYPE_STRING);

  mozilla_signals[END_LOAD_URL] =
    gtk_signal_new ("end_load_url",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkMozillaClass, end_load_url),
                    gtk_marshal_NONE__POINTER_INT,
                    GTK_TYPE_NONE, 2,
                    GTK_TYPE_STRING,
                    GTK_TYPE_INT);

  gtk_object_class_add_signals (object_class, mozilla_signals, LAST_SIGNAL);

  object_class->finalize = gtk_mozilla_finalize;
  widget_class->realize = gtk_mozilla_realize;

  klass->will_load_url = NULL;
  klass->begin_load_url = NULL;
  klass->end_load_url = NULL;
}

static void
gtk_mozilla_realize (GtkWidget *widget)
{
  //printf("gtk_mozilla_realize()\n");
  GtkMozilla *moz = GTK_MOZILLA(widget);
  class GtkMozillaContainer *moz_container;

  if (GTK_WIDGET_CLASS (parent_class)->realize)
    (* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

  moz_container = (class GtkMozillaContainer *)moz->mozilla_container;
  
  moz_container->Show();
}

static void
gtk_mozilla_init (GtkMozilla *moz)
{
  //printf("gtk_mozilla_init()\n");
  
  moz->mozilla_container = NULL;
  
  gtk_layout_set_hadjustment (GTK_LAYOUT (moz), NULL);
  gtk_layout_set_vadjustment (GTK_LAYOUT (moz), NULL);

  GTK_WIDGET_SET_FLAGS (GTK_WIDGET(moz), GTK_CAN_FOCUS);
  
  moz->mozilla_container = new GtkMozillaContainer(moz);
}

static void event_processor_callback(gpointer data,
                                     gint source,
                                     GdkInputCondition condition)
{
  printf("event_processor_callback()\n");
  nsIEventQueue *eventQueue = (nsIEventQueue*)data;
  eventQueue->ProcessPendingEvents();
}


GtkType
gtk_mozilla_get_type (void)
{
  static GtkType mozilla_type = 0;

  if (!mozilla_type) {
    static const GtkTypeInfo mozilla_info = {
      "GtkMozilla",
      sizeof (GtkMozilla),
      sizeof (GtkMozillaClass),
      (GtkClassInitFunc) gtk_mozilla_class_init,
      (GtkObjectInitFunc) gtk_mozilla_init,
      (GtkArgSetFunc) NULL,
      (GtkArgGetFunc) NULL,
    };
    mozilla_type = gtk_type_unique (GTK_TYPE_LAYOUT, &mozilla_info);

    //printf("Setting up registry.\n");
    
    NS_SetupRegistry();

    //printf("Creating event queue.\n");
    
    nsIEventQueueService *aEventQService = nsnull;

    // Create the Event Queue for the UI thread...

    nsresult rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                               kIEventQueueServiceIID,
                                               (nsISupports **)&aEventQService);
    
    if (!NS_SUCCEEDED(rv)) {
      printf("Could not obtain the event queue service\n");
      return rv;
    }
    
    // Create the event queue
    rv = aEventQService->CreateThreadEventQueue();
    if (!NS_SUCCEEDED(rv)) {
      printf("Could not create the event queue for the the thread");
      return rv;
    }

    nsIEventQueue * EQueue = nsnull;

    rv = aEventQService->GetThreadEventQueue(PR_GetCurrentThread(), &EQueue);
    if (!NS_SUCCEEDED(rv)) {
      printf("Could not get the newly created thread event queue.\n");
      return rv;
    }

    NS_RELEASE(aEventQService);

    //printf("Initializing NetService.\n");

#ifndef NECKO
    NS_InitINetService();
#endif

    gdk_input_add(EQueue->GetEventQueueSelectFD(),
                  GDK_INPUT_READ,
                  event_processor_callback,
                  EQueue);

  }

  return mozilla_type;
}

GtkWidget*
gtk_mozilla_new (void)
{
  GtkMozilla *moz;

  moz = GTK_MOZILLA(gtk_type_new(GTK_TYPE_MOZILLA));
  
  return GTK_WIDGET (moz);
}

static void
gtk_mozilla_finalize (GtkObject *object)
{
  GtkMozilla *moz;
  class GtkMozillaContainer *moz_container;

  //printf("gtk_mozilla_finalize()\n");

  g_return_if_fail(object != NULL);
  g_return_if_fail(GTK_IS_MOZILLA(object));

  moz = GTK_MOZILLA(object);

  moz_container = (class GtkMozillaContainer *)moz->mozilla_container;
  moz->mozilla_container = NULL;
  if (moz_container) {
    delete moz_container;
  }
  
  GTK_OBJECT_CLASS(parent_class)->finalize (object);
}

void
gtk_mozilla_resize(GtkMozilla *moz, gint width, gint height)
{
  //printf("gtk_mozilla_resize()\n");
  class GtkMozillaContainer *moz_container;
  
  moz_container = (class GtkMozillaContainer *)moz->mozilla_container;
  moz_container->Resize(width, height);
}

void
gtk_mozilla_load_url(GtkMozilla *moz, const char *url)
{
  //printf("gtk_mozilla_load_url()\n");
  class GtkMozillaContainer *moz_container;
  
  moz_container = (class GtkMozillaContainer *)moz->mozilla_container;
  moz_container->LoadURL(url);
}

void
gtk_mozilla_stop(GtkMozilla *moz)
{
  class GtkMozillaContainer *moz_container;
  
  moz_container = (class GtkMozillaContainer *)moz->mozilla_container;
  moz_container->Stop();
}

void
gtk_mozilla_reload(GtkMozilla *moz, GtkMozillaReloadType reload_type)
{
  class GtkMozillaContainer *moz_container;
  
  moz_container = (class GtkMozillaContainer *)moz->mozilla_container;
  moz_container->Reload(reload_type);
}

void
gtk_mozilla_back(GtkMozilla *moz)
{
  class GtkMozillaContainer *moz_container;
  
  moz_container = (class GtkMozillaContainer *)moz->mozilla_container;
  moz_container->Back();
}
  
void
gtk_mozilla_forward(GtkMozilla *moz)
{
  class GtkMozillaContainer *moz_container;
  
  moz_container = (class GtkMozillaContainer *)moz->mozilla_container;
  moz_container->Forward();
}
  
gint
gtk_mozilla_can_back(GtkMozilla *moz)
{
  class GtkMozillaContainer *moz_container;
  
  moz_container = (class GtkMozillaContainer *)moz->mozilla_container;
  return moz_container->CanBack();
}
  
gint
gtk_mozilla_can_forward(GtkMozilla *moz)
{
  class GtkMozillaContainer *moz_container;
  
  moz_container = (class GtkMozillaContainer *)moz->mozilla_container;
  return moz_container->CanForward();
}
  
void
gtk_mozilla_goto_history(GtkMozilla *moz, gint index)
{
  class GtkMozillaContainer *moz_container;
  
  moz_container = (class GtkMozillaContainer *)moz->mozilla_container;
  moz_container->GoTo(index);
}
  
gint
gtk_mozilla_get_history_length(GtkMozilla *moz)
{
  class GtkMozillaContainer *moz_container;
  
  moz_container = (class GtkMozillaContainer *)moz->mozilla_container;
  return moz_container->GetHistoryLength();
}
  
gint
gtk_mozilla_get_history_index(GtkMozilla *moz)
{
  class GtkMozillaContainer *moz_container;
  
  moz_container = (class GtkMozillaContainer *)moz->mozilla_container;
  return moz_container->GetHistoryIndex();
}

gint
gtk_mozilla_stream_start(GtkMozilla *moz,
                         const char *base_url,
                         const char *action,
                         const char *content_type)
{
  class GtkMozillaContainer *moz_container;
  
  moz_container = (class GtkMozillaContainer *)moz->mozilla_container;
//  return moz_container->StartStream(base_url, action, content_type);
}

gint
gtk_mozilla_stream_start_html(GtkMozilla *moz,
                              const char *base_url)
{
  gtk_mozilla_stream_start(moz, base_url, "view", "text/html");
}

gint
gtk_mozilla_stream_write(GtkMozilla *moz,
                         const char *data,
                         gint offset,
                         gint len)
{
  class GtkMozillaContainer *moz_container;
  
  moz_container = (class GtkMozillaContainer *)moz->mozilla_container;

  // Returns the number of bytes written!
  return moz_container->WriteStream(data, offset, len);
}

void
gtk_mozilla_stream_end(GtkMozilla *moz)
{
  class GtkMozillaContainer *moz_container;
  
  moz_container = (class GtkMozillaContainer *)moz->mozilla_container;
  moz_container->EndStream();
}



