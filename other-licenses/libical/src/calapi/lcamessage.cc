/*======================================================================
FILE: lcamessage.cc
   
DESCRIPTION:
   
======================================================================*/

#include "lcamessage.h"



/* METHOD: LCAMessage::LCAMessage */

LCAMessage::LCAMessage (const LCAMessage &lcamessage) 
{
}

/* METHOD: LCAMessage::LCAMessage */

LCAMessage::LCAMessage () 
{
}

/* METHOD: LCAMessage::~LCAMessage */

LCAMessage::~LCAMessage () 
{
}

/* METHOD: LCAMessage::operator== */

int
LCAMessage::operator== (const LCAMessage &lcamessage) const
{
}

/* METHOD: LCAMessage::operator!= */

int
LCAMessage::operator!= (const LCAMessage &lcamessage) const
{
}

/* METHOD: LCAMessage::FindMatch */

int
LCAMessage::FindMatch (icalset *set) const
{
   if(set == 0)
   {
   }

}

/* METHOD: LCAMessage::Event */

LCAEvent&
LCAMessage::Event () const
{
}

/* METHOD: LCAMessage::Match */

LCAEvent&
LCAMessage::Match () const
{
}

/* METHOD: LCAMessage::Sender */

const string&
LCAMessage::Sender () const
{
}

/* METHOD: LCAMessage::Recipient */

const string&
LCAMessage::Recipient () const
{
}


