// Filename: dxGeomMunger8.h
// Created by:  drose (11Mar05)
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

#ifndef DXGEOMMUNGER8_H
#define DXGEOMMUNGER8_H

#include "pandabase.h"
#include "standardMunger.h"
#include "graphicsStateGuardian.h"
#include "weakPointerTo.h"
#include "weakPointerCallback.h"

////////////////////////////////////////////////////////////////////
//       Class : DXGeomMunger8
// Description : This specialization on GeomMunger finesses vertices
//               for DirectX rendering.  In particular, it makes sure
//               colors are stored in DirectX's packed_argb format,
//               and that all relevant components are packed into a
//               single array, in the correct order.
////////////////////////////////////////////////////////////////////
class EXPCL_PANDADX DXGeomMunger8 : public StandardMunger, public WeakPointerCallback {
public:
  INLINE DXGeomMunger8(GraphicsStateGuardian *gsg, const RenderState *state);
  virtual ~DXGeomMunger8();
  ALLOC_DELETED_CHAIN(DXGeomMunger8);

  virtual void wp_callback(void *);

protected:
  virtual CPT(GeomVertexFormat) munge_format_impl(const GeomVertexFormat *orig,
                                                  const GeomVertexAnimationSpec &animation);
  virtual CPT(GeomVertexFormat) premunge_format_impl(const GeomVertexFormat *orig);

  virtual int compare_to_impl(const GeomMunger *other) const;
  virtual int geom_compare_to_impl(const GeomMunger *other) const;

private:
  WCPT(TextureAttrib) _texture;
  WCPT(TexGenAttrib) _tex_gen;

  // This pointer is derived from _texture, above.  In the case that
  // it is a different pointer, we maintain its reference count
  // explicitly.  If it is the same pointer, we don't reference count
  // it at all (so we won't hold on to the reference count
  // unnecessarily).
  const TextureAttrib *_filtered_texture;
  bool _reffed_filtered_texture;

  static GeomMunger *_deleted_chain;

public:
  static TypeHandle get_class_type() {
    return _type_handle;
  }
  static void init_type() {
    StandardMunger::init_type();
    register_type(_type_handle, "DXGeomMunger8",
                  StandardMunger::get_class_type());
  }
  virtual TypeHandle get_type() const {
    return get_class_type();
  }
  virtual TypeHandle force_init_type() {init_type(); return get_class_type();}

private:
  static TypeHandle _type_handle;
};

#include "dxGeomMunger8.I"

#endif
