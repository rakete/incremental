// Filename: patcher.cxx
// Created by:  mike (09Jan97)
//
////////////////////////////////////////////////////////////////////
//
// PANDA 3D SOFTWARE
// Copyright (c) 2001, Disney Enterprises, Inc.  All rights reserved
//
// All use of this software is subject to the terms of the Panda 3d
// Software license.  You should have received a copy of this license
// along with this source code; you will also find a current copy of
// the license at http://www.panda3d.org/license.txt .
//
// To contact the maintainers of this program write to
// panda3d@yahoogroups.com .
//
////////////////////////////////////////////////////////////////////
#if defined(WIN32_VC) && !defined(NO_PCH)
#include "downloader_headers.h"
#endif

#pragma hdrstop

#if !defined(WIN32_VC) || defined(NO_PCH)
#include "config_downloader.h"

#include <filename.h>
#endif

#include "patcher.h"

////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////
//     Function: Patcher::Constructor
//       Access: Public
//  Description:
////////////////////////////////////////////////////////////////////
Patcher::
Patcher(void) {
  PT(Buffer) buffer = new Buffer(patcher_buffer_size);
  init(buffer);
}

////////////////////////////////////////////////////////////////////
//     Function: Patcher::Constructor
//       Access: Public
//  Description:
////////////////////////////////////////////////////////////////////
Patcher::
Patcher(PT(Buffer) buffer) {
  init(buffer);
}

////////////////////////////////////////////////////////////////////
//     Function: Patcher::Constructor
//       Access: Private
//  Description:
////////////////////////////////////////////////////////////////////
void Patcher::
init(PT(Buffer) buffer) {
  nassertv(!buffer.is_null());
  _buffer = buffer;

  _patchfile = NULL;
  _patchfile = new Patchfile(_buffer);
}

////////////////////////////////////////////////////////////////////
//     Function: Patcher::Destructor
//       Access: Public
//  Description:
////////////////////////////////////////////////////////////////////
Patcher::
~Patcher(void) {
  delete _patchfile;
}

////////////////////////////////////////////////////////////////////
//     Function: Patcher::initiate
//       Access: Public
//  Description:
////////////////////////////////////////////////////////////////////
int Patcher::
initiate(Filename &patch, Filename &infile) {
  return _patchfile->initiate(patch, infile);
}

////////////////////////////////////////////////////////////////////
//     Function: Patcher::run
//       Access: Public
//  Description:
////////////////////////////////////////////////////////////////////
int Patcher::
run(void) {
  return _patchfile->run();
}
