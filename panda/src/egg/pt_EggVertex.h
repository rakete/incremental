// Filename: pt_EggVertex.h
// Created by:  drose (22Feb01)
// 
////////////////////////////////////////////////////////////////////

#ifndef PT_EGGVERTEX_H
#define PT_EGGVERTEX_H

#include <pandabase.h>

#include "eggVertex.h"
#include <pointerTo.h>

////////////////////////////////////////////////////////////////////
//       Class : PT_EggVertex
// Description : A PT(EggVertex).  This is defined here solely we can
//               explicitly export the template class.  It's not
//               strictly necessary, but it doesn't hurt.
////////////////////////////////////////////////////////////////////

EXPORT_TEMPLATE_CLASS(EXPCL_PANDA, EXPTP_PANDA, PointerToBase<EggVertex>)
EXPORT_TEMPLATE_CLASS(EXPCL_PANDA, EXPTP_PANDA, PointerTo<EggVertex>)
EXPORT_TEMPLATE_CLASS(EXPCL_PANDA, EXPTP_PANDA, ConstPointerTo<EggVertex>)

typedef PointerTo<EggVertex> PT_EggVertex;
typedef ConstPointerTo<EggVertex> CPT_EggVertex;

// Tell GCC that we'll take care of the instantiation explicitly here.
#ifdef __GNUC__
#pragma interface
#endif

#endif
