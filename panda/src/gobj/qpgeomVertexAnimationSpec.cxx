// Filename: qpgeomVertexAnimationSpec.cxx
// Created by:  drose (29Mar05)
//
////////////////////////////////////////////////////////////////////
//
// PANDA 3D SOFTWARE
// Copyright (c) 2001 - 2004, Disney Enterprises, Inc.  All rights reserved
//
// All use of this software is subject to the terms of the Panda 3d
// Software license.  You should have received a copy of this license
// along with this source code; you will also find a current copy of
// the license at http://etc.cmu.edu/panda3d/docs/license/ .
//
// To contact the maintainers of this program write to
// panda3d-general@lists.sourceforge.net .
//
////////////////////////////////////////////////////////////////////

#include "qpgeomVertexAnimationSpec.h"
#include "datagram.h"
#include "datagramIterator.h"

////////////////////////////////////////////////////////////////////
//     Function: qpGeomVertexAnimationSpec::output
//       Access: Published
//  Description: 
////////////////////////////////////////////////////////////////////
void qpGeomVertexAnimationSpec::
output(ostream &out) const {
  switch (_animation_type) {
  case AT_none:
    out << "none";
    break;

  case AT_panda:
    out << "panda";
    break;

  case AT_hardware:
    out << "hardware(" << _num_transforms << ", " 
        << _indexed_transforms << ")";
    break;
  }
}

////////////////////////////////////////////////////////////////////
//     Function: qpGeomVertexAnimationSpec::write_datagram
//       Access: Public
//  Description: Writes the contents of this object to the datagram
//               for shipping out to a Bam file.
////////////////////////////////////////////////////////////////////
void qpGeomVertexAnimationSpec::
write_datagram(BamWriter *, Datagram &dg) {
  dg.add_uint8(_animation_type);
  dg.add_uint16(_num_transforms);
  dg.add_bool(_indexed_transforms);
}

////////////////////////////////////////////////////////////////////
//     Function: qpGeomVertexAnimationSpec::fillin
//       Access: Protected
//  Description: This internal function is called by make_from_bam to
//               read in all of the relevant data from the BamFile for
//               the new qpGeomVertexAnimationSpec.
////////////////////////////////////////////////////////////////////
void qpGeomVertexAnimationSpec::
fillin(DatagramIterator &scan, BamReader *) {
  _animation_type = (AnimationType)scan.get_uint8();
  _num_transforms = scan.get_uint16();
  _indexed_transforms = scan.get_bool();
}
