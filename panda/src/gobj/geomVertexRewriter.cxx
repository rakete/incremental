// Filename: geomVertexRewriter.cxx
// Created by:  drose (28Mar05)
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

#include "geomVertexRewriter.h"

////////////////////////////////////////////////////////////////////
//     Function: GeomVertexRewriter::output
//       Access: Published
//  Description: 
////////////////////////////////////////////////////////////////////
void GeomVertexRewriter::
output(ostream &out) const {
  const GeomVertexColumn *column = get_column();
  if (column == (GeomVertexColumn *)NULL) {
    out << "GeomVertexRewriter()";
    
  } else {
    out << "GeomVertexRewriter, array = " << get_array_data()
        << ", column = " << column->get_name()
        << " (" << GeomVertexReader::get_packer()->get_name()
        << "), read row " << get_read_row()
        << ", write row " << get_write_row();
  }
}
