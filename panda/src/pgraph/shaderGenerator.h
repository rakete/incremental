// Filename: shaderGenerator.h
// Created by: jyelon (15Dec07)
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

#ifndef SHADERGENERATOR_H
#define SHADERGENERATOR_H

#include "pandabase.h"
#include "typedWritableReferenceCount.h"
#include "nodePath.h"
class AmbientLight;
class DirectionalLight;
class PointLight;
class Spotlight;
class LightAttrib;
class ShaderAttrib;

////////////////////////////////////////////////////////////////////
//       Class : ShaderGenerator
// Description : The ShaderGenerator is a device that effectively
//               replaces the classic fixed function pipeline with
//               a 'next-gen' fixed function pipeline.  The next-gen
//               fixed function pipeline supports features like 
//               normal mapping, gloss mapping, cartoon lighting,
//               and so forth.  It works by automatically generating
//               a shader from a given RenderState.
//
//               Currently, there is a single default ShaderGenerator
//               object.  It is our intent that in time, people will
//               write classes that derive from ShaderGenerator but
//               which yield slightly different results.
//
//               The ShaderGenerator owes its existence to the 
//               'Bamboo Team' at Carnegie Mellon's Entertainment
//               Technology Center.  This is a group of students
//               who, as a semester project, decided that next-gen
//               graphics should be accessible to everyone, even if
//               they don't know shader programming.  The group 
//               consisted of:
//
//               Aaron Lo, Programmer
//               Heegun Lee, Programmer
//               Erin Fernandez, Artist/Tester
//               Joe Grubb, Artist/Tester
//               Ivan Ortega, Technical Artist/Tester
//
//               Thanks to them!
//
////////////////////////////////////////////////////////////////////

class EXPCL_PANDA_PGRAPH ShaderGenerator : public TypedWritableReferenceCount {
private:
  static PT(ShaderGenerator) _default_generator;

PUBLISHED:
  ShaderGenerator();
  virtual ~ShaderGenerator();
  static ShaderGenerator *get_default();
  static void set_default(ShaderGenerator *generator);
  virtual CPT(RenderAttrib) synthesize_shader(const RenderState *rs);
  
protected:
  CPT(RenderAttrib) create_shader_attrib(const string &txt);

  // Shader register allocation:

  int _vcregs_used;
  int _fcregs_used;
  int _vtregs_used;
  int _ftregs_used;
  void reset_register_allocator();
  INLINE char *alloc_vreg();
  INLINE char *alloc_freg();

  // RenderState analysis information.  Created by analyze_renderstate:

  AttribSlots _attribs;
  Material *_material;
  int _num_textures;
  
  pvector <AmbientLight *>     _alights;
  pvector <DirectionalLight *> _dlights;
  pvector <PointLight *>       _plights;
  pvector <Spotlight *>        _slights;
  pvector <NodePath>           _alights_np;
  pvector <NodePath>           _dlights_np;
  pvector <NodePath>           _plights_np;
  pvector <NodePath>           _slights_np;
  
  bool _vertex_colors;
  bool _flat_colors;
  
  bool _lighting;

  bool _have_ambient;
  bool _have_diffuse;
  bool _have_emission;
  bool _have_specular;
  
  bool _separate_ambient_diffuse;
  
  int _map_index_normal;
  int _map_index_height;
  int _map_index_glow;
  int _map_index_gloss;
  
  int _bitplane_color;
  int _bitplane_normal;
  
  bool _need_material_props;
  
  void analyze_renderstate(const RenderState *rs);
  void clear_analysis();
  
public:
  static TypeHandle get_class_type() {
    return _type_handle;
  }
  static void init_type() {
    TypedReferenceCount::init_type();
    register_type(_type_handle, "ShaderGenerator",
                  TypedReferenceCount::get_class_type());
  }
  virtual TypeHandle get_type() const {
    return get_class_type();
  }
  virtual TypeHandle force_init_type() {init_type(); return get_class_type();}

 private:
  static TypeHandle _type_handle;
};

#include "shaderGenerator.I"

#endif  // SHADERGENERATOR_H

