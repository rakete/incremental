// Filename: guiManager.cxx
// Created by:  cary (25Oct00)
// 
////////////////////////////////////////////////////////////////////

#include "guiManager.h"
#include "config_gui.h"

#include <dataRelation.h>
#include <renderRelation.h>
#include <depthTestTransition.h>
#include <depthWriteTransition.h>
#include <materialTransition.h>
#include <cullFaceTransition.h>
#include <lightTransition.h>
#include <frustum.h>
#include <orthoProjection.h>
#include <eventQueue.h>

GuiManager::GuiMap* GuiManager::_map = (GuiManager::GuiMap*)0L;

GuiManager* GuiManager::get_ptr(GraphicsWindow* w, MouseAndKeyboard* mak,
                Node *root2d) {
  GuiManager* ret;
  if (_map == (GuiMap*)0L) {
    if (gui_cat.is_debug())
      gui_cat->debug() << "allocating a manager map" << endl;
    _map = new GuiMap;
  }
  GuiMap::const_iterator gi;
  gi = _map->find(w);
  if (gi != _map->end()) {
    ret = (*gi).second;
    if (gui_cat.is_debug())
      gui_cat->debug() << "a manager for this window already exists (0x"
               << (void*)ret << ")" << endl;
  } else {
    // going to allocate a new GuiManager for this window
    if (gui_cat.is_debug())
      gui_cat->debug() << "allocating a new manager for this window" << endl;
    // first see if there is a mouseWatcher already under the MouseAndKeyboard
    bool has_watcher = false;
    TypeHandle dgt = DataRelation::get_class_type();
    MouseWatcher* watcher = (MouseWatcher*)0L;
    for (int i=0; i<mak->get_num_children(dgt); ++i)
      if (mak->get_child(dgt, i)->get_child()->get_type() ==
      MouseWatcher::get_class_type()) {
    has_watcher = true;
    watcher = DCAST(MouseWatcher, mak->get_child(dgt, i)->get_child());
      }
    if (!has_watcher) {
      // there isn't already a mousewatcher in the data graph, so we'll make
      // one and re-parent everything to it.
      if (gui_cat.is_debug())
    gui_cat->debug() << "no MouseWatcher found, making one" << endl;
      watcher = new MouseWatcher("GUI watcher");
      DataRelation* tmp = new DataRelation(mak, watcher);
      for (int j=0; j<mak->get_num_children(dgt); ++j) {
    NodeRelation* rel = mak->get_child(dgt, j);
    if (rel != tmp)
      // it's not the node we just created, so reparent it to ours
      rel->change_parent(watcher);
      }
    } else {
      if (gui_cat.is_debug())
      gui_cat->debug() << "found a MouseWatcher, don't have to make one"
               << endl;
    }
    // now setup event triggers for the watcher
#ifdef _DEBUG
    if (has_watcher && !watcher->get_button_down_pattern().empty())
      gui_cat->warning() << "overwriting existing button down pattern '"
             << watcher->get_button_down_pattern()
             << "' with 'gui-button-press'" << endl;
    if (has_watcher && !watcher->get_button_up_pattern().empty())
      gui_cat->warning() << "overwriting existing button up pattern '"
             << watcher->get_button_up_pattern()
             << "' with 'gui-button-release'" << endl;
    if (has_watcher && !watcher->get_enter_pattern().empty())
      gui_cat->warning() << "overwriting existing enter pattern '"
             << watcher->get_enter_pattern()
             << "' with 'gui-enter'" << endl;
    if (has_watcher && !watcher->get_leave_pattern().empty())
      gui_cat->warning() << "overwriting existing exit pattern '"
             << watcher->get_leave_pattern()
             << "' with 'gui-exit'" << endl;
#endif /* _DEBUG */
    watcher->set_button_down_pattern("gui-button-press");
    watcher->set_button_up_pattern("gui-button-release");
    watcher->set_enter_pattern("gui-enter");
    watcher->set_leave_pattern("gui-exit");

    if (root2d == (Node *)NULL) {
      // If we weren't given a 2-d scene graph, then create one now.
      // It lives in its own layer.

      Node* root2d_top = new NamedNode("GUI_top");
      root2d = new NamedNode("GUI");
      NodeRelation* root2d_arc = new RenderRelation(root2d_top, root2d);
      root2d_arc->set_transition(new DepthTestTransition(DepthTestProperty::M_none), 1);
      root2d_arc->set_transition(new DepthWriteTransition(DepthWriteTransition::off()), 1);
      root2d_arc->set_transition(new LightTransition(LightTransition::all_off()), 1);
      root2d_arc->set_transition(new MaterialTransition(MaterialTransition::off()), 1);
      root2d_arc->set_transition(new CullFaceTransition(CullFaceProperty::M_cull_none), 1);
      PT(Camera) cam = new Camera("GUI_cam");
      new RenderRelation(root2d, cam);
      cam->set_scene(root2d_top);
      Frustumf frust2d;
      frust2d.make_ortho_2D();
      cam->set_projection(OrthoProjection(frust2d));
      GraphicsChannel *chan = w->get_channel(0);  // root/full-window channel
      nassertr(chan != (GraphicsChannel*)0L, NULL);
      GraphicsLayer *layer = chan->make_layer();
      nassertr(layer != (GraphicsLayer*)0L, NULL);
      DisplayRegion *dr = layer->make_display_region();
      nassertr(dr != (DisplayRegion*)0L, NULL);
      dr->set_camera(cam);
      if (gui_cat.is_debug())
    gui_cat->debug() << "2D layer created" << endl;
    }

    // make an event handler for our internal events
    EventHandler* eh = new EventHandler(new EventQueue());
    watcher->set_extra_handler(eh);

    // now make the manager for this window
    ret = new GuiManager(watcher, root2d, eh);
    if (gui_cat.is_debug())
      gui_cat->debug() << "new manager allocated (0x" << (void*)ret << ")"
               << endl;
    (*_map)[w] = ret;
  }
  return ret;
}

void GuiManager::add_region(MouseWatcherRegion* region) {
  region->test_ref_count_integrity();
  RegionSet::const_iterator ri;
  ri = _regions.find(region);
  if (ri == _regions.end()) {
    _watcher->add_region(region);
    _regions.insert(region);
  } else
    gui_cat->warning() << "tried adding region ('" << *region
               << "') more then once" << endl;
}

void GuiManager::add_label(GuiLabel* label) {
  label->test_ref_count_integrity();
  LabelSet::const_iterator li;
  li = _labels.find(label);
  if (li == _labels.end()) {
    // add it to the scenegraph
    label->set_arc(new RenderRelation(_root, label->get_geometry()));
    _labels.insert(label);
  } else
    gui_cat->warning() << "tried adding label (0x" << (void*)label
               << ") more then once" << endl;
}

void GuiManager::add_label(GuiLabel* label, Node* parent) {
  label->test_ref_count_integrity();
  LabelSet::const_iterator li;
  li = _labels.find(label);
  if (li == _labels.end()) {
    // add it to the scenegraph
    label->set_arc(new RenderRelation(parent, label->get_geometry()));
    _labels.insert(label);
  } else
    gui_cat->warning() << "tried adding label (0x" << (void*)label
               << ") more then once" << endl;
}

void GuiManager::remove_region(MouseWatcherRegion* region) {
  region->test_ref_count_integrity();
  RegionSet::iterator ri;
  ri = _regions.find(region);
  if (ri == _regions.end())
    gui_cat->warning() << "tried removing region ('" << *region
               << "') that isn't there" << endl;
  else {
    _watcher->remove_region(region);
    _regions.erase(ri);
  }
}

void GuiManager::remove_label(GuiLabel* label) {
  label->test_ref_count_integrity();
  LabelSet::iterator li;
  li = _labels.find(label);
  if (li == _labels.end())
    gui_cat->warning() << "label (0x" << (void*)label
               << ") is not there to be removed" << endl;
  else {
    // remove it to the scenegraph
    remove_arc(label->get_arc());
    label->set_arc((RenderRelation*)0L);
    _labels.erase(li);
  }
}

bool GuiManager::has_region(MouseWatcherRegion* rgn) {
  rgn->test_ref_count_integrity();
  RegionSet::iterator ri;
  ri = _regions.find(rgn);
  return (ri != _regions.end());
}

bool GuiManager::has_label(GuiLabel* lbl) {
  lbl->test_ref_count_integrity();
  LabelSet::iterator li;
  li = _labels.find(lbl);
  return (li != _labels.end());
}

void GuiManager::recompute_priorities(void) {
  _sorts.clear();
  for (LabelSet::iterator i=_labels.begin(); i!=_labels.end(); ++i)
    _sorts.insert(*i);
  int p=this->_start_draw_order;
  for (SortSet::iterator j=_sorts.begin(); j!=_sorts.end(); ++j) {
    if ((!((*j)->has_hard_draw_order())) && ((*j)->get_draw_order() != p))
      p = (*j)->soft_set_draw_order(p);
  }
  _next_draw_order = p;
}

INLINE bool in_range(float x, float a, float b) {
  return ((x > a) && (x < b));
}

INLINE bool overlap(MouseWatcherRegion* a, MouseWatcherRegion* b) {
  LVector4f av = a->get_frame();
  LVector4f bv = b->get_frame();

  if ((in_range(av[0], bv[0], bv[1]) || in_range(av[1], bv[0], bv[1])) &&
      (in_range(av[2], bv[2], bv[3]) || in_range(av[3], bv[2], bv[3])))
    return true;
  return false;
}

bool GuiManager::is_sane(void) const {
  for (RegionSet::const_iterator i=_regions.begin(); i!=_regions.end(); ++i)
    for (RegionSet::const_iterator j=_regions.begin(); j!=_regions.end(); ++j) {
      if ((*i) == (*j))
    continue;
      if (overlap((*i), (*j)))
    return false;
    }
  return true;
}

void GuiManager::sanity_check(void) const {
  for (RegionSet::const_iterator i=_regions.begin(); i!=_regions.end(); ++i)
    for (RegionSet::const_iterator j=_regions.begin(); j!=_regions.end(); ++j) {
      if ((*i) == (*j))
    continue;
      if (overlap((*i), (*j))) {
    LVector4f iv = (*i)->get_frame();
    LVector4f jv = (*j)->get_frame();
    gui_cat->warning() << "GuiManager::sanity_check: overlapping regions '"
               << (*i)->get_name() << "' and '" << (*j)->get_name()
               << "'" << endl << "  (" << iv[0] << ", " << iv[1]
               << ", " << iv[2] << ", " << iv[3] << ") and ("
               << jv[0] << ", " << jv[1] << ", " << jv[2] << ", "
               << jv[3] << ")" << endl;
    
      }
      if ((*i)->get_name() == (*j)->get_name())
    gui_cat->warning() << "GuiManager::sanity_check: regions with same "
               << "name '" << (*i)->get_name() << " 0x"
               << (void*)(*i) << " and 0x" << (void*)(*j) << endl;
    }
}
