// Filename: geomVertexReader.h
// Created by:  drose (25Mar05)
//
////////////////////////////////////////////////////////////////////
//
// PANDA 3D SOFTWARE
// Copyright (c) Carnegie Mellon University.  All rights reserved.
//
// All use of this software is subject to the terms of the revised BSD
// license.  You should have received a copy of this license along
// with this source code in a file named "LICENSE."
//
////////////////////////////////////////////////////////////////////

#ifndef GEOMVERTEXREADER_H
#define GEOMVERTEXREADER_H

#include "pandabase.h"
#include "geomVertexData.h"
#include "geomVertexColumn.h"
#include "internalName.h"
#include "luse.h"
#include "pointerTo.h"

////////////////////////////////////////////////////////////////////
//       Class : GeomVertexReader
// Description : This object provides a high-level interface for
//               quickly reading a sequence of numeric values from a
//               vertex table. 
//
//               It is particularly optimized for reading a single
//               column of data values for a series of vertices,
//               without changing columns between each number.
//               Although you can also use one GeomVertexReader to
//               read across the columns if it is convenient, by
//               calling set_column() repeatedly at each vertex, it is
//               faster to read down the columns, and to use a
//               different GeomVertexReader for each column.
//
//               Note that a GeomVertexReader does not keep a
//               reference count to the actual vertex data buffer (it
//               grabs the current data buffer from the GeomVertexData
//               whenever set_column() is called).  This means that it
//               is important not to keep a GeomVertexReader object
//               around over a long period of time in which the data
//               buffer is likely to be deallocated; it is intended
//               for making a quick pass over the data in one session.
//
//               It also means that you should create any
//               GeomVertexWriters *before* creating GeomVertexReaders
//               on the same data, since the writer itself might cause
//               the vertex buffer to be deallocated.  Better yet, use
//               a GeomVertexRewriter if you are going to create both
//               of them anyway.
////////////////////////////////////////////////////////////////////
class EXPCL_PANDA_GOBJ GeomVertexReader : public GeomEnums {
PUBLISHED:
  INLINE GeomVertexReader(Thread *current_thread = Thread::get_current_thread());
  INLINE GeomVertexReader(const GeomVertexData *vertex_data,
                          Thread *current_thread = Thread::get_current_thread());
  INLINE GeomVertexReader(const GeomVertexData *vertex_data,
                          const string &name,
                          Thread *current_thread = Thread::get_current_thread());
  INLINE GeomVertexReader(const GeomVertexData *vertex_data,
                          const InternalName *name,
                          Thread *current_thread = Thread::get_current_thread());
  INLINE GeomVertexReader(const GeomVertexArrayData *array_data,
                          Thread *current_thread = Thread::get_current_thread());
  INLINE GeomVertexReader(const GeomVertexArrayData *array_data, 
                          int column,
                          Thread *current_thread = Thread::get_current_thread());

public:
  INLINE GeomVertexReader(const GeomVertexDataPipelineReader *data_reader,
                          const InternalName *name,
                          bool force = true);

PUBLISHED:
  INLINE GeomVertexReader(const GeomVertexReader &copy);
  INLINE void operator = (const GeomVertexReader &copy);
  INLINE ~GeomVertexReader();

  INLINE const GeomVertexData *get_vertex_data() const;
  INLINE const GeomVertexArrayData *get_array_data() const;
  INLINE const GeomVertexArrayDataHandle *get_array_handle() const;
  INLINE Thread *get_current_thread() const;

  INLINE void set_force(bool force);
  INLINE bool get_force() const;

  INLINE bool set_column(int column);
  INLINE bool set_column(const string &name);
  INLINE bool set_column(const InternalName *name);
  bool set_column(int array, const GeomVertexColumn *column);

  INLINE void clear();
  INLINE bool has_column() const;
  INLINE int get_array() const;
  INLINE const GeomVertexColumn *get_column() const;

  INLINE void set_row_unsafe(int row);
  INLINE void set_row(int row);

  INLINE int get_start_row() const;
  INLINE int get_read_row() const;
  INLINE bool is_at_end() const;

  INLINE float get_data1f();
  INLINE const LVecBase2f &get_data2f();
  INLINE const LVecBase3f &get_data3f();
  INLINE const LVecBase4f &get_data4f();

  INLINE double get_data1d();
  INLINE const LVecBase2d &get_data2d();
  INLINE const LVecBase3d &get_data3d();
  INLINE const LVecBase4d &get_data4d();

  INLINE PN_stdfloat get_data1();
  INLINE const LVecBase2 &get_data2();
  INLINE const LVecBase3 &get_data3();
  INLINE const LVecBase4 &get_data4();

  INLINE int get_data1i();
  INLINE const int *get_data2i();
  INLINE const int *get_data3i();
  INLINE const int *get_data4i();

  void output(ostream &out) const;

protected:
  INLINE GeomVertexColumn::Packer *get_packer() const;

private:
  void initialize();

  INLINE bool set_pointer(int row);
  INLINE void quick_set_pointer(int row);
  INLINE const unsigned char *inc_pointer();

  bool set_vertex_column(int array, const GeomVertexColumn *column,
                         const GeomVertexDataPipelineReader *data_reader);
  bool set_array_column(const GeomVertexColumn *column);

  // It is important that we only store *one* of the following two
  // pointers.  If we are storing a GeomVertexData/array index, we
  // must not keep a pointer to the particular ArrayData we are
  // working on (if we do, it may result in an extra copy of the data
  // due to holding the reference count).
  CPT(GeomVertexData) _vertex_data;
  int _array;
  CPT(GeomVertexArrayData) _array_data;

  Thread *_current_thread;
  GeomVertexColumn::Packer *_packer;
  int _stride;

  CPT(GeomVertexArrayDataHandle) _handle;
  const unsigned char *_pointer_begin;
  const unsigned char *_pointer_end;
  const unsigned char *_pointer;

  int _start_row;
  bool _force;

#ifndef NDEBUG
  // This is defined just for the benefit of having something non-NULL
  // to return from a nassertr() call.
  static const unsigned char empty_buffer[100];
#endif
};

INLINE ostream &
operator << (ostream &out, const GeomVertexReader &reader) {
  reader.output(out);
  return out;
}

#include "geomVertexReader.I"

#endif
