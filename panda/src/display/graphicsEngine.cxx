// Filename: graphicsEngine.cxx
// Created by:  drose (24Feb02)
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

#include "graphicsEngine.h"
#include "graphicsPipe.h"
#include "parasiteBuffer.h"
#include "config_display.h"
#include "pipeline.h"
#include "drawCullHandler.h"
#include "binCullHandler.h"
#include "cullResult.h"
#include "cullTraverser.h"
#include "clockObject.h"
#include "pStatTimer.h"
#include "pStatClient.h"
#include "pStatCollector.h"
#include "mutexHolder.h"
#include "reMutexHolder.h"
#include "cullFaceAttrib.h"
#include "string_utils.h"
#include "geomCacheManager.h"
#include "renderState.h"
#include "transformState.h"
#include "thread.h"
#include "pipeline.h"
#include "throw_event.h"
#include "bamCache.h"
#include "cullableObject.h"
#include "geomVertexArrayData.h"
#include "vertexDataSaveFile.h"
#include "vertexDataBook.h"
#include "vertexDataPage.h"
#include "config_pgraph.h"

#if defined(WIN32)
  #define WINDOWS_LEAN_AND_MEAN
  #include <WinSock2.h>
  #include <wtypes.h>
  #undef WINDOWS_LEAN_AND_MEAN  
#else
  #include <sys/time.h>
#endif

PStatCollector GraphicsEngine::_wait_pcollector("Wait:Thread sync");
PStatCollector GraphicsEngine::_cycle_pcollector("App:Cycle");
PStatCollector GraphicsEngine::_app_pcollector("App:Show code:General");
PStatCollector GraphicsEngine::_render_frame_pcollector("App:render_frame");
PStatCollector GraphicsEngine::_do_frame_pcollector("*:do_frame");
PStatCollector GraphicsEngine::_yield_pcollector("App:Yield");
PStatCollector GraphicsEngine::_cull_pcollector("Cull");
PStatCollector GraphicsEngine::_cull_setup_pcollector("Cull:Setup");
PStatCollector GraphicsEngine::_cull_sort_pcollector("Cull:Sort");
PStatCollector GraphicsEngine::_draw_pcollector("Draw");
PStatCollector GraphicsEngine::_sync_pcollector("Draw:Sync");
PStatCollector GraphicsEngine::_flip_pcollector("Wait:Flip");
PStatCollector GraphicsEngine::_flip_begin_pcollector("Wait:Flip:Begin");
PStatCollector GraphicsEngine::_flip_end_pcollector("Wait:Flip:End");
PStatCollector GraphicsEngine::_transform_states_pcollector("TransformStates");
PStatCollector GraphicsEngine::_transform_states_unused_pcollector("TransformStates:Unused");
PStatCollector GraphicsEngine::_render_states_pcollector("RenderStates");
PStatCollector GraphicsEngine::_render_states_unused_pcollector("RenderStates:Unused");
PStatCollector GraphicsEngine::_cyclers_pcollector("PipelineCyclers");
PStatCollector GraphicsEngine::_dirty_cyclers_pcollector("Dirty PipelineCyclers");
PStatCollector GraphicsEngine::_delete_pcollector("App:Delete");


PStatCollector GraphicsEngine::_sw_sprites_pcollector("SW Sprites");
PStatCollector GraphicsEngine::_vertex_data_small_pcollector("Vertex Data:Small");
PStatCollector GraphicsEngine::_vertex_data_independent_pcollector("Vertex Data:Independent");
PStatCollector GraphicsEngine::_vertex_data_pending_pcollector("Vertex Data:Pending");
PStatCollector GraphicsEngine::_vertex_data_resident_pcollector("Vertex Data:Resident");
PStatCollector GraphicsEngine::_vertex_data_compressed_pcollector("Vertex Data:Compressed");
PStatCollector GraphicsEngine::_vertex_data_unused_disk_pcollector("Vertex Data:Disk:Unused");
PStatCollector GraphicsEngine::_vertex_data_used_disk_pcollector("Vertex Data:Disk:Used");

// These are counted independently by the collision system; we
// redefine them here so we can reset them at each frame.
PStatCollector GraphicsEngine::_cnode_volume_pcollector("Collision Volumes:CollisionNode");
PStatCollector GraphicsEngine::_gnode_volume_pcollector("Collision Volumes:GeomNode");
PStatCollector GraphicsEngine::_geom_volume_pcollector("Collision Volumes:Geom");
PStatCollector GraphicsEngine::_node_volume_pcollector("Collision Volumes:PandaNode");
PStatCollector GraphicsEngine::_volume_pcollector("Collision Volumes:CollisionSolid");
PStatCollector GraphicsEngine::_test_pcollector("Collision Tests:CollisionSolid");
PStatCollector GraphicsEngine::_volume_polygon_pcollector("Collision Volumes:CollisionPolygon");
PStatCollector GraphicsEngine::_test_polygon_pcollector("Collision Tests:CollisionPolygon");
PStatCollector GraphicsEngine::_volume_plane_pcollector("Collision Volumes:CollisionPlane");
PStatCollector GraphicsEngine::_test_plane_pcollector("Collision Tests:CollisionPlane");
PStatCollector GraphicsEngine::_volume_sphere_pcollector("Collision Volumes:CollisionSphere");
PStatCollector GraphicsEngine::_test_sphere_pcollector("Collision Tests:CollisionSphere");
PStatCollector GraphicsEngine::_volume_tube_pcollector("Collision Volumes:CollisionTube");
PStatCollector GraphicsEngine::_test_tube_pcollector("Collision Tests:CollisionTube");
PStatCollector GraphicsEngine::_volume_inv_sphere_pcollector("Collision Volumes:CollisionInvSphere");
PStatCollector GraphicsEngine::_test_inv_sphere_pcollector("Collision Tests:CollisionInvSphere");
PStatCollector GraphicsEngine::_volume_geom_pcollector("Collision Volumes:CollisionGeom");
PStatCollector GraphicsEngine::_test_geom_pcollector("Collision Tests:CollisionGeom");
PStatCollector GraphicsEngine::_occlusion_untested_pcollector("Occlusion results:Not tested");
PStatCollector GraphicsEngine::_occlusion_passed_pcollector("Occlusion results:Visible");
PStatCollector GraphicsEngine::_occlusion_failed_pcollector("Occlusion results:Occluded");
PStatCollector GraphicsEngine::_occlusion_tests_pcollector("Occlusion tests");

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::Constructor
//       Access: Published
//  Description: Creates a new GraphicsEngine object.  The Pipeline is
//               normally left to default to NULL, which indicates the
//               global render pipeline, but it may be any Pipeline
//               you choose.
////////////////////////////////////////////////////////////////////
GraphicsEngine::
GraphicsEngine(Pipeline *pipeline) :
  _pipeline(pipeline),
  _app("app"),
  _lock("GraphicsEngine::_lock")
{
  if (_pipeline == (Pipeline *)NULL) {
    _pipeline = Pipeline::get_render_pipeline();
  }

  _windows_sorted = true;
  _window_sort_index = 0;
  _needs_open_windows = false;
  
  set_threading_model(GraphicsThreadingModel(threading_model));
  if (!_threading_model.is_default()) {
    display_cat.info()
      << "Using threading model " << _threading_model << "\n";
  }
  _auto_flip = auto_flip;
  _portal_enabled = false;
  _flip_state = FS_flip;

  _singular_warning_last_frame = false;
  _singular_warning_this_frame = false;
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::Destructor
//       Access: Published
//  Description: Gracefully cleans up the graphics engine and its
//               related threads and windows.
////////////////////////////////////////////////////////////////////
GraphicsEngine::
~GraphicsEngine() {
#ifdef DO_PSTATS
  if (_app_pcollector.is_started()) {
    _app_pcollector.stop();
  }
#endif

  remove_all_windows();
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::set_threading_model
//       Access: Published
//  Description: Specifies how future objects created via make_gsg(),
//               make_buffer(), and make_window() will be threaded.
//               This does not affect any already-created objects.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
set_threading_model(const GraphicsThreadingModel &threading_model) {
  if (!Thread::is_threading_supported()) {
    if (!threading_model.is_single_threaded()) {
      display_cat.warning()
        << "Threading model " << threading_model
        << " requested but threading is not available.  Ignoring.\n";
      return;
    }
  }
    
#ifndef THREADED_PIPELINE
  if (!threading_model.is_single_threaded()) {
    display_cat.warning()
      << "Threading model " << threading_model
      << " requested but multithreaded render pipelines not enabled in build.\n";
    if (!allow_nonpipeline_threads) {
      display_cat.warning()
        << "Ignoring requested threading model.\n";
      return;
    }
    display_cat.warning()
      << "Danger!  Creating requested render threads anyway!\n";
  }
#endif  // THREADED_PIPELINE
  ReMutexHolder holder(_lock);
  _threading_model = threading_model;
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::get_threading_model
//       Access: Published
//  Description: Returns the threading model that will be applied to
//               future objects.  See set_threading_model().
////////////////////////////////////////////////////////////////////
GraphicsThreadingModel GraphicsEngine::
get_threading_model() const {
  GraphicsThreadingModel result;
  {
    ReMutexHolder holder(_lock);
    result = _threading_model;
  }
  return result;
}

// THIS IS THE OLD CODE FOR make_gsg
//  PT(GraphicsStateGuardian) gsg = pipe->make_gsg(properties, share_with);



////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::make_output
//       Access: Published
//  Description: Creates a new window (or buffer) and returns it.
//               The GraphicsEngine becomes the owner of the window,
//               it will persist at least until remove_window() is
//               called later.
//
//               If a null pointer is supplied for the gsg, then this
//               routine will create a new gsg.
//               
//               This routine is only called from the app thread.
////////////////////////////////////////////////////////////////////

GraphicsOutput *GraphicsEngine::
make_output(GraphicsPipe *pipe,
            const string &name, int sort,
            const FrameBufferProperties &fb_prop,
            const WindowProperties &win_prop,
            int flags,
            GraphicsStateGuardian *gsg,
            GraphicsOutput *host) {

  //  The code here is tricky because the gsg that is passed in
  //  might be in the uninitialized state.  As a result,
  //  pipe::make_output may not be able to tell which DirectX
  //  capabilities or OpenGL extensions are supported and which
  //  are not.  Worse yet, it can't query the API, because that
  //  can only be done from the draw thread, and this is the app
  //  thread.
  //
  //  So here's the workaround: this routine calls pipe::make_output,
  //  which returns a "non-certified" window.  That means that
  //  the pipe doesn't promise that the draw thread will actually
  //  succeed in initializing the window.  This routine then calls
  //  open_windows, which attempts to initialize the window.
  //
  //  If open_windows fails to initialize the window, then
  //  this routine will ask pipe::make_output to try again, this
  //  time using a different set of OpenGL extensions or DirectX
  //  capabilities.   This is what the "retry" parameter to
  //  pipe::make_output is for - it specifies, in an abstract
  //  manner, which set of capabilties/extensions to try.
  //
  //  The only problem with this design is that it requires the
  //  engine to call open_windows, which is slow.  To make
  //  things faster, the pipe can choose to "precertify"
  //  its creations.  If it chooses to do so, this is a guarantee
  //  that the windows it returns will not fail in open_windows.
  //  However, most graphics pipes will only precertify if you
  //  pass them an already-initialized gsg.  Long story short,
  //  if you want make_output to be fast, use an
  //  already-initialized gsg.

  // Simplify the input parameters.
  
  int x_size = 0, y_size = 0;
  if (win_prop.has_size()) {
    x_size = win_prop.get_x_size();
    y_size = win_prop.get_y_size();
  }
  if ((x_size == 0)&&(y_size == 0)) {
    flags |= GraphicsPipe::BF_size_track_host;
  }
  if (host != 0) {
    host = host->get_host();
  }

  // If a gsg or host was supplied, and either is not yet initialized,
  // then call open_windows to get both ready.  If that fails,
  // give up on using the supplied gsg and host.

  if (host == (GraphicsOutput *)NULL) {
    if (gsg != (GraphicsStateGuardian*)NULL) {
      if ((!gsg->is_valid())||(gsg->needs_reset())) {
        open_windows();
      }
      if ((!gsg->is_valid())||(gsg->needs_reset())) {
        gsg = NULL;
      }
    }
  } else {
    if ((host->get_gsg()==0)||
        (!host->is_valid())||
        (!host->get_gsg()->is_valid())||
        (host->get_gsg()->needs_reset())) {
      open_windows();
    }
    if ((host->get_gsg()==0)||
        (!host->is_valid())||
        (!host->get_gsg()->is_valid())||
        (host->get_gsg()->needs_reset())) {
      host = NULL;
      gsg = NULL;
    } else {
      gsg = host->get_gsg();
    }
  }

  // Sanity check everything.

  GraphicsThreadingModel threading_model = get_threading_model();
  nassertr(pipe != (GraphicsPipe *)NULL, NULL);
  if (gsg != (GraphicsStateGuardian *)NULL) {
    nassertr(pipe == gsg->get_pipe(), NULL);
    nassertr(this == gsg->get_engine(), NULL);
    nassertr(threading_model.get_draw_name() ==
             gsg->get_threading_model().get_draw_name(), NULL);
  }
  
  // Determine if a parasite buffer meets the user's specs.

  bool can_use_parasite = false;
  if ((host != 0)&&
      ((flags&GraphicsPipe::BF_require_window)==0)&&
      ((flags&GraphicsPipe::BF_refuse_parasite)==0)&&
      ((flags&GraphicsPipe::BF_can_bind_color)==0)&&
      ((flags&GraphicsPipe::BF_can_bind_every)==0)&&
      ((flags&GraphicsPipe::BF_rtt_cumulative)==0)) {
    if ((flags&GraphicsPipe::BF_fb_props_optional) ||
        (host->get_fb_properties().subsumes(fb_prop))) {
      can_use_parasite = true;
    }
  }

  // If parasite buffers are preferred, then try a parasite first.
  // Even if prefer-parasite-buffer is set, parasites are not preferred
  // if the host window is too small, or if the host window does not
  // have the requested properties.
  
  if ((prefer_parasite_buffer) &&
      (can_use_parasite) &&
      (x_size <= host->get_x_size())&&
      (y_size <= host->get_y_size())&&
      (host->get_fb_properties().subsumes(fb_prop))) {
    ParasiteBuffer *buffer = new ParasiteBuffer(host, name, x_size, y_size, flags);
    buffer->_sort = sort;
    do_add_window(buffer, threading_model);
    do_add_gsg(host->get_gsg(), pipe, threading_model);
    return buffer;
  }

  // Ask the pipe to create a window.
  
  for (int retry=0; retry<10; retry++) {
    bool precertify = false;
    PT(GraphicsOutput) window = 
      pipe->make_output(name, fb_prop, win_prop, flags, gsg, host, retry, precertify);
    if (window != (GraphicsOutput *)NULL) {
      window->_sort = sort;
      if ((precertify) && (gsg != 0) && (window->get_gsg()==gsg)) {
        do_add_window(window, threading_model);
        do_add_gsg(window->get_gsg(), pipe, threading_model);
        return window;
      }
      do_add_window(window, threading_model);
      open_windows();
      if (window->is_valid()) {
        do_add_gsg(window->get_gsg(), pipe, threading_model);
        if (window->get_fb_properties().subsumes(fb_prop)) {
          return window;
        } else {
          if (flags & GraphicsPipe::BF_fb_props_optional) {
            display_cat.warning()
              << "FrameBufferProperties available less than requested.\n";
            return window;
          }
          display_cat.error()
            << "Could not get requested FrameBufferProperties; abandoning window.\n";
          display_cat.error(false)
            << "  requested: " << fb_prop << "\n"
            << "  got: " << window->get_fb_properties() << "\n";
        }
      } else {
        display_cat.error()
          << "Window wouldn't open; abandoning window.\n";
      }

      // No good; delete the window and keep trying.
      bool removed = remove_window(window);
      nassertr(removed, NULL);
    }
  }
  
  // Parasite buffers were not preferred, but the pipe could not
  // create a window to the user's specs.  Try a parasite as a
  // last hope.
  
  if (can_use_parasite) {
    if (x_size > host->get_x_size()) {
      x_size = Texture::down_to_power_2(host->get_x_size());
    }
    if (y_size > host->get_y_size()) {
      y_size = Texture::down_to_power_2(host->get_y_size());
    }
    if (flags & GraphicsPipe::BF_size_square) {
      x_size = y_size = min(x_size, y_size);
    }
    ParasiteBuffer *buffer = new ParasiteBuffer(host, name, x_size, y_size, flags);
    buffer->_sort = sort;
    do_add_window(buffer, threading_model);
    do_add_gsg(host->get_gsg(), pipe, threading_model);
    return buffer;
  }

  // Could not create a window to the user's specs.

  return NULL;
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::remove_window
//       Access: Published
//  Description: Removes the indicated window or offscreen buffer from
//               the set of windows that will be processed when
//               render_frame() is called.  This also closes the
//               window if it is open, and removes the window from its
//               GraphicsPipe, allowing the window to be destructed if
//               there are no other references to it.  (However, the
//               window may not be actually closed until next frame,
//               if it is controlled by a sub-thread.)
//
//               The return value is true if the window was removed,
//               false if it was not found.
//
//               Unlike remove_all_windows(), this function does not
//               terminate any of the threads that may have been
//               started to service this window; they are left running
//               (since you might open a new window later on these
//               threads).  If your intention is to clean up before
//               shutting down, it is better to call
//               remove_all_windows() then to call remove_window() one
//               at a time.
////////////////////////////////////////////////////////////////////
bool GraphicsEngine::
remove_window(GraphicsOutput *window) {
  Thread *current_thread = Thread::get_current_thread();

  // First, make sure we know what this window is.
  PT(GraphicsOutput) ptwin = window;
  size_t count;
  {
    ReMutexHolder holder(_lock, current_thread);
    if (!_windows_sorted) {
      do_resort_windows();
    }
    count = _windows.erase(ptwin);
  }
  if (count == 0) {
    // Never heard of this window.  Do nothing.
    return false;
  }

  do_remove_window(window, current_thread);

  nassertr(count == 1, true);
  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::remove_all_windows
//       Access: Published
//  Description: Removes and closes all windows from the engine.  This
//               also cleans up and terminates any threads that have
//               been started to service those windows.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
remove_all_windows() {
  Thread *current_thread = Thread::get_current_thread();

  Windows::iterator wi;
  for (wi = _windows.begin(); wi != _windows.end(); ++wi) {
    GraphicsOutput *win = (*wi);
    do_remove_window(win, current_thread);
  }
  
  _windows.clear();

  _app.do_close(this, current_thread);
  _app.do_pending(this, current_thread);
  terminate_threads(current_thread);

  // It seems a safe assumption that we're about to exit the
  // application or otherwise shut down Panda.  Although it's a bit of
  // a hack, since it's not really related to removing windows, this
  // would nevertheless be a fine time to ensure the model cache (if
  // any) has been flushed to disk.
  BamCache *cache = BamCache::get_global_ptr();
  cache->flush_index();

  // And, hey, let's stop the vertex paging thread, if any.
  VertexDataPage::stop_thread();

  // Well, and why not clean up all threads here?
  Thread::prepare_for_exit();
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::reset_all_windows
//       Access: Published
//  Description: Resets the framebuffer of the current window.  This
//               is currently used by DirectX 8 only. It calls a
//               reset_window function on each active window to 
//               release/create old/new framebuffer
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
reset_all_windows(bool swapchain) {
  Windows::iterator wi;
  for (wi = _windows.begin(); wi != _windows.end(); ++wi) {
    GraphicsOutput *win = (*wi);
    //    if (win->is_active())
    win->reset_window(swapchain);
  }
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::is_empty
//       Access: Published
//  Description: Returns true if there are no windows or buffers
//               managed by the engine, false if there is at least
//               one.
////////////////////////////////////////////////////////////////////
bool GraphicsEngine::
is_empty() const {
  return _windows.empty();
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::get_num_windows
//       Access: Published
//  Description: Returns the number of windows (or buffers) managed by
//               the engine.
////////////////////////////////////////////////////////////////////
int GraphicsEngine::
get_num_windows() const {
  return _windows.size();
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::get_window
//       Access: Published
//  Description: Returns the nth window or buffers managed by the
//               engine, in sorted order.
////////////////////////////////////////////////////////////////////
GraphicsOutput *GraphicsEngine::
get_window(int n) const {
  nassertr(n >= 0 && n < (int)_windows.size(), NULL);

  if (!_windows_sorted) {
    ((GraphicsEngine *)this)->do_resort_windows();
  }
  return _windows[n];
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::render_frame
//       Access: Published
//  Description: Renders the next frame in all the registered windows,
//               and flips all of the frame buffers.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
render_frame() {
  Thread *current_thread = Thread::get_current_thread();

  // Since this gets called every frame, we should take advantage of
  // the opportunity to flush the cache if necessary.
  BamCache *cache = BamCache::get_global_ptr();
  cache->consider_flush_index();

  // Anything that happens outside of GraphicsEngine::render_frame()
  // is deemed to be App.
#ifdef DO_PSTATS
  _render_frame_pcollector.start();
  if (_app_pcollector.is_started()) {
    _app_pcollector.stop();
  }
#endif

  if (_needs_open_windows) {
    // Make sure our buffers and windows are fully realized before we
    // render a frame.  We do this particularly to realize our
    // offscreen buffers, so that we don't render a frame before the
    // offscreen buffers are ready (which might result in a frame
    // going by without some textures having been rendered).
    open_windows();
  }

  ClockObject *global_clock = ClockObject::get_global_clock();

  if (display_cat.is_spam()) {
    display_cat.spam()
      << "render_frame() - frame " << global_clock->get_frame_count() << "\n";
  }

  ReMutexHolder holder(_lock, current_thread);

  if (!_windows_sorted) {
    do_resort_windows();
  }

  if (sync_flip && _flip_state != FS_flip) {
    do_flip_frame(current_thread);
  }

  // Are any of the windows ready to be deleted?
  Windows new_windows;
  new_windows.reserve(_windows.size());
  Windows::iterator wi;
  for (wi = _windows.begin(); wi != _windows.end(); ++wi) {
    GraphicsOutput *win = (*wi);
    if (win->get_delete_flag()) {
      do_remove_window(win, current_thread);
      
    } else {
      new_windows.push_back(win);
      
      // Let's calculate each scene's bounding volume here in App,
      // before we cycle the pipeline.  The cull traversal will
      // calculate it anyway, but if we calculate it in App first
      // before it gets calculated in the Cull thread, it will be more
      // likely to stick for subsequent frames, so we won't have to
      // recompute it each frame.
      int num_drs = win->get_num_active_display_regions();
      for (int i = 0; i < num_drs; ++i) {
        DisplayRegion *dr = win->get_active_display_region(i);
        if (dr != (DisplayRegion *)NULL) {
          NodePath camera_np = dr->get_camera(current_thread);
          if (!camera_np.is_empty()) {
            Camera *camera = DCAST(Camera, camera_np.node());
            NodePath scene = camera->get_scene();
            if (scene.is_empty()) {
              scene = camera_np.get_top(current_thread);
            }
            if (!scene.is_empty()) {
              scene.get_bounds(current_thread);
            }
          }
        }
      }
    }
  }

  // Now it's time to do any drawing from the main frame--after all of
  // the App code has executed, but before we begin the next frame.
  _app.do_frame(this, current_thread);
  
  // Grab each thread's mutex again after all windows have flipped,
  // and wait for the thread to finish.
  {
    PStatTimer timer(_wait_pcollector, current_thread);
    Threads::const_iterator ti;
    for (ti = _threads.begin(); ti != _threads.end(); ++ti) {
      RenderThread *thread = (*ti).second;
      thread->_cv_mutex.lock();
      
      while (thread->_thread_state != TS_wait) {
        thread->_cv_done.wait();
      }
    }
  }

#if defined(THREADED_PIPELINE) && defined(DO_PSTATS)
  _cyclers_pcollector.set_level(_pipeline->get_num_cyclers());
  _dirty_cyclers_pcollector.set_level(_pipeline->get_num_dirty_cyclers());

#ifdef DEBUG_THREADS
  if (PStatClient::is_connected()) {
    _pipeline->iterate_all_cycler_types(pstats_count_cycler_type, this);
    _pipeline->iterate_dirty_cycler_types(pstats_count_dirty_cycler_type, this);
  }
#endif  // DEBUG_THREADS

#endif  // THREADED_PIPELINE && DO_PSTATS

  GeomCacheManager::flush_level();
  CullTraverser::flush_level();
  RenderState::flush_level();
  TransformState::flush_level();
  CullableObject::flush_level();

  // Now cycle the pipeline and officially begin the next frame.
#ifdef THREADED_PIPELINE
  {
    PStatTimer timer(_cycle_pcollector, current_thread);
    _pipeline->cycle();
  }
#endif  // THREADED_PIPELINE

  global_clock->tick(current_thread);
  if (global_clock->check_errors(current_thread)) {
    throw_event("clock_error");
  }

#ifdef DO_PSTATS
  PStatClient::main_tick();

  // Reset our pcollectors that track data across the frame.
  CullTraverser::_nodes_pcollector.clear_level();
  CullTraverser::_geom_nodes_pcollector.clear_level();
  CullTraverser::_geoms_pcollector.clear_level();
  GeomCacheManager::_geom_cache_active_pcollector.clear_level();
  GeomCacheManager::_geom_cache_record_pcollector.clear_level();
  GeomCacheManager::_geom_cache_erase_pcollector.clear_level();
  GeomCacheManager::_geom_cache_evict_pcollector.clear_level();
  
  GraphicsStateGuardian::init_frame_pstats();

  _transform_states_pcollector.set_level(TransformState::get_num_states());
  _render_states_pcollector.set_level(RenderState::get_num_states());
  if (pstats_unused_states) {
    _transform_states_unused_pcollector.set_level(TransformState::get_num_unused_states());
    _render_states_unused_pcollector.set_level(RenderState::get_num_unused_states());
  }

  _sw_sprites_pcollector.clear_level();

  _cnode_volume_pcollector.clear_level();
  _gnode_volume_pcollector.clear_level();
  _geom_volume_pcollector.clear_level();
  _node_volume_pcollector.clear_level();
  _volume_pcollector.clear_level();
  _test_pcollector.clear_level();
  _volume_polygon_pcollector.clear_level();
  _test_polygon_pcollector.clear_level();
  _volume_plane_pcollector.clear_level();
  _test_plane_pcollector.clear_level();
  _volume_sphere_pcollector.clear_level();
  _test_sphere_pcollector.clear_level();
  _volume_tube_pcollector.clear_level();
  _test_tube_pcollector.clear_level();
  _volume_inv_sphere_pcollector.clear_level();
  _test_inv_sphere_pcollector.clear_level();
  _volume_geom_pcollector.clear_level();
  _test_geom_pcollector.clear_level();
  _occlusion_untested_pcollector.clear_level();
  _occlusion_passed_pcollector.clear_level();
  _occlusion_failed_pcollector.clear_level();
  _occlusion_tests_pcollector.clear_level();

  if (PStatClient::is_connected()) {
    size_t small_buf = GeomVertexArrayData::get_small_lru()->get_total_size();
    size_t independent = GeomVertexArrayData::get_independent_lru()->get_total_size();
    size_t resident = VertexDataPage::get_global_lru(VertexDataPage::RC_resident)->get_total_size();
    size_t compressed = VertexDataPage::get_global_lru(VertexDataPage::RC_compressed)->get_total_size();
    size_t pending = VertexDataPage::get_pending_lru()->get_total_size();

    VertexDataSaveFile *save_file = VertexDataPage::get_save_file();
    size_t total_disk = save_file->get_total_file_size();
    size_t used_disk = save_file->get_used_file_size();

    _vertex_data_small_pcollector.set_level(small_buf);
    _vertex_data_independent_pcollector.set_level(independent);
    _vertex_data_pending_pcollector.set_level(pending);
    _vertex_data_resident_pcollector.set_level(resident);
    _vertex_data_compressed_pcollector.set_level(compressed);
    _vertex_data_unused_disk_pcollector.set_level(total_disk - used_disk);
    _vertex_data_used_disk_pcollector.set_level(used_disk);
  }

#endif  // DO_PSTATS

  GeomVertexArrayData::lru_epoch();
  
  // Now signal all of our threads to begin their next frame.
  Threads::const_iterator ti;
  for (ti = _threads.begin(); ti != _threads.end(); ++ti) {
    RenderThread *thread = (*ti).second;
    if (thread->_thread_state == TS_wait) {
      thread->_thread_state = TS_do_frame;
      thread->_cv_start.signal();
    }
    thread->_cv_mutex.release();
  }

  // Some threads may still be drawing, so indicate that we have to
  // wait for those threads before we can flip.
  _flip_state = _auto_flip ? FS_flip : FS_draw;

  if (yield_timeslice) { 
    // Nap for a moment to yield the timeslice, to be polite to other
    // running applications.
    PStatTimer timer(_yield_pcollector, current_thread);
    Thread::force_yield();
  }

  // Anything that happens outside of GraphicsEngine::render_frame()
  // is deemed to be App.
  _app_pcollector.start();
  _render_frame_pcollector.stop();
}


////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::open_windows
//       Access: Published
//  Description: Fully opens (or closes) any windows that have
//               recently been requested open or closed, without
//               rendering any frames.  It is not necessary to call
//               this explicitly, since windows will be automatically
//               opened or closed when the next frame is rendered, but
//               you may call this if you want your windows now
//               without seeing a frame go by.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
open_windows() {
  Thread *current_thread = Thread::get_current_thread();

  ReMutexHolder holder(_lock, current_thread);

  if (!_windows_sorted) {
    do_resort_windows();
  }

  // We do it twice, to allow both cull and draw to process the
  // window.
  for (int i = 0; i < 2; ++i) {
    _app.do_windows(this, current_thread);
    _app.do_pending(this, current_thread);

    PStatTimer timer(_wait_pcollector, current_thread);
    Threads::const_iterator ti;
    for (ti = _threads.begin(); ti != _threads.end(); ++ti) {
      RenderThread *thread = (*ti).second;
      thread->_cv_mutex.lock();
      
      while (thread->_thread_state != TS_wait) {
        thread->_cv_done.wait();
      }
      
      thread->_thread_state = TS_do_windows;
      thread->_cv_start.signal();
      thread->_cv_mutex.release();
    }
  }

  _needs_open_windows = false;
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::sync_frame
//       Access: Published
//  Description: Waits for all the threads that started drawing their
//               last frame to finish drawing.  The windows are not
//               yet flipped when this returns; see also flip_frame().
//               It is not usually necessary to call this explicitly,
//               unless you need to see the previous frame right away.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
sync_frame() {
  Thread *current_thread = Thread::get_current_thread();
  ReMutexHolder holder(_lock, current_thread);

  if (_flip_state == FS_draw) {
    do_sync_frame(current_thread);
  }
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::flip_frame
//       Access: Published
//  Description: Waits for all the threads that started drawing their
//               last frame to finish drawing, and then flips all the
//               windows.  It is not usually necessary to call this
//               explicitly, unless you need to see the previous frame
//               right away.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
flip_frame() {
  Thread *current_thread = Thread::get_current_thread();
  ReMutexHolder holder(_lock, current_thread);

  if (_flip_state != FS_flip) {
    do_flip_frame(current_thread);
  }
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::extract_texture_data
//       Access: Published
//  Description: Asks the indicated GraphicsStateGuardian to retrieve
//               the texture memory image of the indicated texture and
//               store it in the texture's ram_image field.  The image
//               can then be written to disk via Texture::write(), or
//               otherwise manipulated on the CPU.
//
//               This is useful for retrieving the contents of a
//               texture that has been somehow generated on the
//               graphics card, instead of having been loaded the
//               normal way via Texture::read() or Texture::load().
//               It is particularly useful for getting the data
//               associated with a compressed texture image.
//
//               Since this requires a round-trip to the draw thread,
//               it may require waiting for the current thread to
//               finish rendering if it is called in a multithreaded
//               environment.  However, you can call this several
//               consecutive times on different textures for little
//               additional cost.
//
//               If the texture has not yet been loaded to the GSG in
//               question, it will be loaded immediately.
//
//               The return value is true if the operation is
//               successful, false otherwise.
////////////////////////////////////////////////////////////////////
bool GraphicsEngine::
extract_texture_data(Texture *tex, GraphicsStateGuardian *gsg) {
  ReMutexHolder holder(_lock);

  string draw_name = gsg->get_threading_model().get_draw_name();
  if (draw_name.empty()) {
    // A single-threaded environment.  No problem.
    return gsg->extract_texture_data(tex);

  } else {
    // A multi-threaded environment.  We have to wait until the draw
    // thread has finished its current task.
    WindowRenderer *wr = get_window_renderer(draw_name, 0);
    RenderThread *thread = (RenderThread *)wr;
    MutexHolder holder2(thread->_cv_mutex);
      
    while (thread->_thread_state != TS_wait) {
      thread->_cv_done.wait();
    }

    // OK, now the draw thread is idle.  That's really good enough for
    // our purposes; we don't *actually* need to make the draw thread
    // do the work--it's sufficient that it's not doing anything else
    // while we access the GSG.
    return gsg->extract_texture_data(tex);
  }
}
 
////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::add_callback
//       Access: Public
//  Description: Adds the indicated C/C++ function and an arbitrary
//               associated data pointer to the list of functions that
//               will be called in the indicated thread at the
//               indicated point of the frame.
//
//               The thread name may be one of the cull or draw names
//               specified in set_threading_model(), or it may be the
//               empty string to indicated the app or main thread.
//
//               The return value is true if the function and data
//               pointer are successfully added to the appropriate
//               callback list, or false if the same function and data
//               pointer were already there.
////////////////////////////////////////////////////////////////////
bool GraphicsEngine::
add_callback(const string &thread_name, 
             GraphicsEngine::CallbackTime callback_time,
             GraphicsEngine::CallbackFunction *func, void *data) {
  ReMutexHolder holder(_lock);
  WindowRenderer *wr = get_window_renderer(thread_name, 0);
  return wr->add_callback(callback_time, Callback(func, data));
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::remove_callback
//       Access: Public
//  Description: Removes a callback added by a previous call to
//               add_callback().  All parameters must match the same
//               parameters passed to add_callback().  The return
//               value is true if the callback is successfully
//               removed, or false if it was not on the list (either
//               one or more of the parameters did not match the call
//               to add_callback(), or the callback has already been
//               removed).
////////////////////////////////////////////////////////////////////
bool GraphicsEngine::
remove_callback(const string &thread_name,
                GraphicsEngine::CallbackTime callback_time,
                GraphicsEngine::CallbackFunction *func, void *data) {
  ReMutexHolder holder(_lock);
  WindowRenderer *wr = get_window_renderer(thread_name, 0);
  return wr->remove_callback(callback_time, Callback(func, data));
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::set_window_sort
//       Access: Private
//  Description: Changes the sort value of a particular window (or
//               buffer) on the GraphicsEngine.  This requires
//               securing the mutex.
//
//               Users shouldn't call this directly; use
//               GraphicsOutput::set_sort() instead.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
set_window_sort(GraphicsOutput *window, int sort) {
  ReMutexHolder holder(_lock);
  window->_sort = sort;
  _windows_sorted = false;
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::cull_and_draw_together
//       Access: Private
//  Description: This is called in the cull+draw thread by individual
//               RenderThread objects during the frame rendering.  It
//               culls the geometry and immediately draws it, without
//               first collecting it into bins.  This is used when the
//               threading model begins with the "-" character.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
cull_and_draw_together(const GraphicsEngine::Windows &wlist,
                       Thread *current_thread) {
  PStatTimer timer(_cull_pcollector, current_thread);

  Windows::const_iterator wi;
  for (wi = wlist.begin(); wi != wlist.end(); ++wi) {
    GraphicsOutput *win = (*wi);
    if (win->is_active() && win->get_gsg()->is_active()) {
      if (win->flip_ready()) {
        {
          PStatTimer timer(GraphicsEngine::_flip_begin_pcollector, current_thread);
          win->begin_flip();
        }
        {
          PStatTimer timer(GraphicsEngine::_flip_end_pcollector, current_thread);
          win->end_flip();
        }
      }

      if (win->begin_frame(GraphicsOutput::FM_render, current_thread)) {
        win->clear(current_thread);
      
        int num_display_regions = win->get_num_active_display_regions();
        for (int i = 0; i < num_display_regions; i++) {
          DisplayRegion *dr = win->get_active_display_region(i);
          if (dr != (DisplayRegion *)NULL) {
            cull_and_draw_together(win, dr, current_thread);
          }
        }
        win->end_frame(GraphicsOutput::FM_render, current_thread);

        if (_auto_flip) {
          if (win->flip_ready()) {
            {
              PStatTimer timer(GraphicsEngine::_flip_begin_pcollector, current_thread);
              win->begin_flip();
            }
            {
              PStatTimer timer(GraphicsEngine::_flip_end_pcollector, current_thread);
              win->end_flip();
            }
          }
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::cull_and_draw_together
//       Access: Private
//  Description: Called only from within the inner loop in
//               cull_and_draw_together(), above.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
cull_and_draw_together(GraphicsOutput *win, DisplayRegion *dr,
                       Thread *current_thread) {
  GraphicsStateGuardian *gsg = win->get_gsg();
  nassertv(gsg != (GraphicsStateGuardian *)NULL);

  DisplayRegionPipelineReader *dr_reader = 
    new DisplayRegionPipelineReader(dr, current_thread);

  win->change_scenes(dr_reader);
  gsg->prepare_display_region(dr_reader, dr_reader->get_stereo_channel());

  PT(SceneSetup) scene_setup = setup_scene(gsg, dr_reader);
  if (scene_setup == (SceneSetup *)NULL) {
    // Never mind.

  } else if (!gsg->set_scene(scene_setup)) {
    // The scene or lens is inappropriate somehow.
    display_cat.error()
      << gsg->get_type() << " cannot render scene with specified lens.\n";

  } else {
    if (dr_reader->is_any_clear_active()) {
      gsg->clear(dr);
    }

    DrawCullHandler cull_handler(gsg);
    if (gsg->begin_scene()) {
      delete dr_reader;
      dr_reader = NULL;
      do_cull(&cull_handler, scene_setup, gsg, current_thread);
      gsg->end_scene();
    }
  }

  if (dr_reader != (DisplayRegionPipelineReader *)NULL) {
    delete dr_reader;
  }
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::cull_to_bins
//       Access: Private
//  Description: This is called in the cull thread by individual
//               RenderThread objects during the frame rendering.  It
//               collects the geometry into bins in preparation for
//               drawing.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
cull_to_bins(const GraphicsEngine::Windows &wlist, Thread *current_thread) {
  PStatTimer timer(_cull_pcollector, current_thread);

  _singular_warning_last_frame = _singular_warning_this_frame;
  _singular_warning_this_frame = false;

  // Keep track of the cameras we have already used in this thread to
  // render DisplayRegions.
  typedef pmap<NodePath, DisplayRegion *> AlreadyCulled;
  AlreadyCulled already_culled;

  Windows::const_iterator wi;
  for (wi = wlist.begin(); wi != wlist.end(); ++wi) {
    GraphicsOutput *win = (*wi);
    if (win->is_active() && win->get_gsg()->is_active()) {
      PStatTimer timer(win->get_cull_window_pcollector(), current_thread);
      int num_display_regions = win->get_num_active_display_regions();
      for (int i = 0; i < num_display_regions; ++i) {
        DisplayRegion *dr = win->get_active_display_region(i);
        if (dr != (DisplayRegion *)NULL) {
          DisplayRegionPipelineReader *dr_reader = 
            new DisplayRegionPipelineReader(dr, current_thread);
          NodePath camera = dr_reader->get_camera();
          AlreadyCulled::iterator aci = already_culled.insert(AlreadyCulled::value_type(camera, NULL)).first;
          if ((*aci).second == NULL) {
            // We have not used this camera already in this thread.
            // Perform the cull operation.
            delete dr_reader;
            dr_reader = NULL;
            (*aci).second = dr;
            cull_to_bins(win, dr, current_thread);

          } else {
            // We have already culled a scene using this camera in
            // this thread, and now we're being asked to cull another
            // scene using the same camera.  (Maybe this represents
            // two different DisplayRegions for the left and right
            // channels of a stereo image.)  Of course, the cull
            // result will be the same, so just use the result from
            // the other DisplayRegion.
            DisplayRegion *other_dr = (*aci).second;
            dr->set_cull_result(other_dr->get_cull_result(current_thread),
                                setup_scene(win->get_gsg(), dr_reader),
                                current_thread);
          }

          if (dr_reader != (DisplayRegionPipelineReader *)NULL) {
            delete dr_reader;
          }
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::cull_to_bins
//       Access: Private
//  Description: Called only within the inner loop of cull_to_bins(),
//               above.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
cull_to_bins(GraphicsOutput *win, DisplayRegion *dr, Thread *current_thread) {
  GraphicsStateGuardian *gsg = win->get_gsg();
  nassertv(gsg != (GraphicsStateGuardian *)NULL);

  PT(CullResult) cull_result;
  PT(SceneSetup) scene_setup;
  {
    PStatTimer timer(_cull_setup_pcollector, current_thread);
    DisplayRegionPipelineReader dr_reader(dr, current_thread);
    scene_setup = setup_scene(gsg, &dr_reader);
    cull_result = dr->get_cull_result(current_thread);

    if (cull_result != (CullResult *)NULL) {
      cull_result = cull_result->make_next();

    } else {
      // This DisplayRegion has no cull results; draw it.
      cull_result = new CullResult(gsg, dr->get_draw_region_pcollector());
    }
  }

  if (scene_setup != (SceneSetup *)NULL) {
    BinCullHandler cull_handler(cull_result);
    do_cull(&cull_handler, scene_setup, gsg, current_thread);

    PStatTimer timer(_cull_sort_pcollector, current_thread);
    cull_result->finish_cull(scene_setup, current_thread);
  }
  
  // Save the results for next frame.
  dr->set_cull_result(cull_result, scene_setup, current_thread);
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::draw_bins
//       Access: Private
//  Description: This is called in the draw thread by individual
//               RenderThread objects during the frame rendering.  It
//               issues the graphics commands to draw the objects that
//               have been collected into bins by a previous call to
//               cull_to_bins().
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
draw_bins(const GraphicsEngine::Windows &wlist, Thread *current_thread) {
  nassertv(wlist.verify_list());

  Windows::const_iterator wi;
  for (wi = wlist.begin(); wi != wlist.end(); ++wi) {
    GraphicsOutput *win = (*wi);
    if (win->is_active() && win->get_gsg()->is_active()) {
      if (win->flip_ready()) {
        {
          PStatTimer timer(GraphicsEngine::_flip_begin_pcollector, current_thread);
          win->begin_flip();
        }
        {
          PStatTimer timer(GraphicsEngine::_flip_end_pcollector, current_thread);
          win->end_flip();
        }
      }

      PStatTimer timer(win->get_draw_window_pcollector(), current_thread);
      if (win->begin_frame(GraphicsOutput::FM_render, current_thread)) {
        win->clear(current_thread);

        if (display_cat.is_spam()) {
          display_cat.spam()
            << "Drawing window " << win->get_name() << "\n";
        }
        int num_display_regions = win->get_num_active_display_regions();
        for (int i = 0; i < num_display_regions; ++i) {
          DisplayRegion *dr = win->get_active_display_region(i);
          if (dr != (DisplayRegion *)NULL) {
            draw_bins(win, dr, current_thread);
          }
        }
        win->end_frame(GraphicsOutput::FM_render, current_thread);

        if (_auto_flip) {
          if (win->flip_ready()) {
            {
              PStatTimer timer(GraphicsEngine::_flip_begin_pcollector, current_thread);
              win->begin_flip();
            }
            {
              PStatTimer timer(GraphicsEngine::_flip_end_pcollector, current_thread);
              win->end_flip();
            }
          }
        }
      } else {
        if (display_cat.is_spam()) {
          display_cat.spam()
            << "Not drawing window " << win->get_name() << "\n";
        }
      }
    } else {
      if (display_cat.is_spam()) {
        display_cat.spam()
          << "Window " << win->get_name() << " is inactive\n";
      }
    }
  }
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::draw_bins
//       Access: Private
//  Description: This variant on draw_bins() is only called from
//               draw_bins(), above.  It draws the cull result for a
//               particular DisplayRegion.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
draw_bins(GraphicsOutput *win, DisplayRegion *dr, Thread *current_thread) {
  GraphicsStateGuardian *gsg = win->get_gsg();
  nassertv(gsg != (GraphicsStateGuardian *)NULL);

  PT(CullResult) cull_result = dr->get_cull_result(current_thread);
  PT(SceneSetup) scene_setup = dr->get_scene_setup(current_thread);
  if (cull_result != (CullResult *)NULL && scene_setup != (SceneSetup *)NULL) {
    do_draw(cull_result, scene_setup, win, dr, current_thread);
  }
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::make_contexts
//       Access: Private
//  Description: Called in the draw thread, this calls make_context()
//               on each window on the list to guarantee its gsg and
//               graphics context both get created.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
make_contexts(const GraphicsEngine::Windows &wlist, Thread *current_thread) {
  Windows::const_iterator wi;
  for (wi = wlist.begin(); wi != wlist.end(); ++wi) {
    GraphicsOutput *win = (*wi);
    if (win->begin_frame(GraphicsOutput::FM_refresh, current_thread)) {
      win->end_frame(GraphicsOutput::FM_refresh, current_thread);
    }
  }
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::process_events
//       Access: Private
//  Description: This is called by the RenderThread object to process
//               all the windows events (resize, etc.) for the given
//               list of windows.  This is run in the window thread.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
process_events(const GraphicsEngine::Windows &wlist, Thread *current_thread) {
  Windows::const_iterator wi;
  for (wi = wlist.begin(); wi != wlist.end(); ++wi) {
    GraphicsOutput *win = (*wi);
    win->process_events();
  }
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::flip_windows
//       Access: Private
//  Description: This is called by the RenderThread object to flip the
//               buffers for all of the non-single-buffered windows in
//               the given list.  This is run in the draw thread.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
flip_windows(const GraphicsEngine::Windows &wlist, Thread *current_thread) {
  Windows::const_iterator wi;
  for (wi = wlist.begin(); wi != wlist.end(); ++wi) {
    GraphicsOutput *win = (*wi);
    if (win->flip_ready()) {
      PStatTimer timer(GraphicsEngine::_flip_begin_pcollector, current_thread);
      win->begin_flip();
    }
  }
  for (wi = wlist.begin(); wi != wlist.end(); ++wi) {
    GraphicsOutput *win = (*wi);
    if (win->flip_ready()) {
      PStatTimer timer(GraphicsEngine::_flip_end_pcollector, current_thread);
      win->end_flip();
    }
  }
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::do_sync_frame
//       Access: Private
//  Description: The implementation of sync_frame().  We assume _lock
//               is already held before this method is called.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
do_sync_frame(Thread *current_thread) {
  nassertv(_lock.debug_is_locked());

  // Statistics
  PStatTimer timer(_sync_pcollector, current_thread);

  nassertv(_flip_state == FS_draw);

  // Wait for all the threads to finish their current frame.  Grabbing
  // and releasing the mutex should achieve that.
  Threads::const_iterator ti;
  for (ti = _threads.begin(); ti != _threads.end(); ++ti) {
    RenderThread *thread = (*ti).second;
    thread->_cv_mutex.lock();
    thread->_cv_mutex.release();
  }

  _flip_state = FS_sync;
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::do_flip_frame
//       Access: Private
//  Description: The implementation of flip_frame().  We assume _lock
//               is already held before this method is called.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
do_flip_frame(Thread *current_thread) {
  nassertv(_lock.debug_is_locked());

  // Statistics
  PStatTimer timer(_flip_pcollector, current_thread);

  nassertv(_flip_state == FS_draw || _flip_state == FS_sync);

  // First, wait for all the threads to finish their current frame, if
  // necessary.  Grabbing the mutex (and waiting for TS_wait) should
  // achieve that.
  {
    PStatTimer timer(_wait_pcollector, current_thread);
    Threads::const_iterator ti;
    for (ti = _threads.begin(); ti != _threads.end(); ++ti) {
      RenderThread *thread = (*ti).second;
      thread->_cv_mutex.lock();

      while (thread->_thread_state != TS_wait) {
        thread->_cv_done.wait();
      }
    }
  }
  
  // Now signal all of our threads to flip the windows.
  _app.do_flip(this, current_thread);

  {
    Threads::const_iterator ti;
    for (ti = _threads.begin(); ti != _threads.end(); ++ti) {
      RenderThread *thread = (*ti).second;
      nassertv(thread->_thread_state == TS_wait);
      thread->_thread_state = TS_do_flip;
      thread->_cv_start.signal();
      thread->_cv_mutex.release();
    }
  }

  _flip_state = FS_flip;
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::setup_scene
//       Access: Private
//  Description: Returns a new SceneSetup object appropriate for
//               rendering the scene from the indicated camera, or
//               NULL if the scene should not be rendered for some
//               reason.
////////////////////////////////////////////////////////////////////
PT(SceneSetup) GraphicsEngine::
setup_scene(GraphicsStateGuardian *gsg, DisplayRegionPipelineReader *dr) {
  Thread *current_thread = dr->get_current_thread();
  PStatTimer timer(_cull_setup_pcollector, current_thread);

  GraphicsOutput *window = dr->get_window();
  // The window pointer shouldn't be NULL, since we presumably got to
  // this particular DisplayRegion by walking through a list on a
  // window.
  nassertr(window != (GraphicsOutput *)NULL, NULL);

  NodePath camera = dr->get_camera();
  if (camera.is_empty()) {
    // No camera, no draw.
    return NULL;
  }

  Camera *camera_node;
  DCAST_INTO_R(camera_node, camera.node(), NULL);

  if (!camera_node->is_active()) {
    // Camera inactive, no draw.
    return NULL;
  }
  camera_node->cleanup_aux_scene_data(current_thread);

  Lens *lens = camera_node->get_lens();
  if (lens == (Lens *)NULL) {
    // No lens, no draw.
    return NULL;
  }

  NodePath scene_root = camera_node->get_scene();
  if (scene_root.is_empty()) {
    // If there's no explicit scene specified, use whatever scene the
    // camera is parented within.  This is the normal and preferred
    // case; the use of an explicit scene is now deprecated.
    scene_root = camera.get_top(current_thread);
  }

  PT(SceneSetup) scene_setup = new SceneSetup;

  // We will need both the camera transform (the net transform to the
  // camera from the scene) and the world transform (the camera
  // transform inverse, or the net transform to the scene from the
  // camera).  These are actually defined from the parent of the
  // scene_root, because the scene_root's own transform is immediately
  // applied to these during rendering.  (Normally, the parent of the
  // scene_root is the empty NodePath, although it need not be.)
  NodePath scene_parent = scene_root.get_parent(current_thread);
  CPT(TransformState) camera_transform = camera.get_transform(scene_parent, current_thread);
  CPT(TransformState) world_transform = scene_parent.get_transform(camera, current_thread);

  if (camera_transform->is_invalid()) {
    // There must be a singular transform over the scene.
    if (!_singular_warning_last_frame) {
      display_cat.warning()
        << "Scene " << scene_root << " has net scale (" 
        << scene_root.get_scale(NodePath()) << "); cannot render.\n";
      _singular_warning_this_frame = true;
    }
    return NULL;
  }

  if (world_transform->is_invalid()) {
    // There must be a singular transform over the camera.
    if (!_singular_warning_last_frame) {
      display_cat.warning()
        << "Camera " << camera << " has net scale (" 
        << camera.get_scale(NodePath()) << "); cannot render.\n";
    }
    _singular_warning_this_frame = true;
    return NULL;
  }

  CPT(RenderState) initial_state = camera_node->get_initial_state();

  if (window->get_inverted()) {
    // If the window is to be inverted, we must set the inverted flag
    // on the SceneSetup object, so that the GSG will be able to
    // invert the projection matrix at the last minute.
    scene_setup->set_inverted(true);

    // This also means we need to globally invert the sense of polygon
    // vertex ordering.
    initial_state = initial_state->compose(get_invert_polygon_state());
  }

  scene_setup->set_display_region(dr->get_object());
  scene_setup->set_viewport_size(dr->get_pixel_width(), dr->get_pixel_height());
  scene_setup->set_scene_root(scene_root);
  scene_setup->set_camera_path(camera);
  scene_setup->set_camera_node(camera_node);
  scene_setup->set_lens(lens);
  scene_setup->set_initial_state(initial_state);
  scene_setup->set_camera_transform(camera_transform);
  scene_setup->set_world_transform(world_transform);

  return scene_setup;
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::do_cull
//       Access: Private
//  Description: Fires off a cull traversal using the indicated camera.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
do_cull(CullHandler *cull_handler, SceneSetup *scene_setup,
        GraphicsStateGuardian *gsg, Thread *current_thread) {
  DisplayRegion *dr = scene_setup->get_display_region();
  PStatTimer timer(dr->get_cull_region_pcollector(), current_thread);

  CullTraverser *trav = dr->get_cull_traverser();
  trav->set_cull_handler(cull_handler);
  trav->set_scene(scene_setup, gsg);

  trav->set_view_frustum(NULL);
  if (view_frustum_cull) {
    // If we're to be performing view-frustum culling, determine the
    // bounding volume associated with the current viewing frustum.

    // First, we have to get the current viewing frustum, which comes
    // from the lens.
    PT(BoundingVolume) bv = scene_setup->get_lens()->make_bounds();

    if (bv != (BoundingVolume *)NULL &&
        bv->is_of_type(GeometricBoundingVolume::get_class_type())) {
      // Transform it into the appropriate coordinate space.
      PT(GeometricBoundingVolume) local_frustum;
      local_frustum = DCAST(GeometricBoundingVolume, bv->make_copy());

      NodePath scene_parent = scene_setup->get_scene_root().get_parent(current_thread);
      CPT(TransformState) cull_center_transform = 
        scene_setup->get_cull_center().get_transform(scene_parent, current_thread);
      local_frustum->xform(cull_center_transform->get_mat());

      trav->set_view_frustum(local_frustum);
    }
  }

  trav->traverse(scene_setup->get_scene_root());
  trav->end_traverse();
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::do_draw
//       Access: Private
//  Description: Draws the previously-culled scene.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
do_draw(CullResult *cull_result, SceneSetup *scene_setup,
        GraphicsOutput *win, DisplayRegion *dr, Thread *current_thread) {
  // Statistics
  PStatTimer timer(dr->get_draw_region_pcollector(), current_thread);

  DisplayRegionPipelineReader *dr_reader = 
    new DisplayRegionPipelineReader(dr, current_thread);

  GraphicsStateGuardian *gsg = win->get_gsg();
  win->change_scenes(dr_reader);

  if (dr_reader->get_stereo_channel() == Lens::SC_stereo) {
    // A special case.  For a stereo DisplayRegion, we render the left
    // eye, followed by the right eye.
    if (dr_reader->is_any_clear_active()) {
      gsg->prepare_display_region(dr_reader, Lens::SC_stereo);
      gsg->clear(dr_reader->get_object());
    }
    gsg->prepare_display_region(dr_reader, Lens::SC_left);

    if (!gsg->set_scene(scene_setup)) {
      // The scene or lens is inappropriate somehow.
      display_cat.error()
        << gsg->get_type() << " cannot render scene with specified lens.\n";
    } else {
      if (gsg->begin_scene()) {
        delete dr_reader;
        dr_reader = NULL;
        cull_result->draw(current_thread);
        gsg->end_scene();
        dr_reader = new DisplayRegionPipelineReader(dr, current_thread);
      }
      if (dr_reader->get_clear_depth_between_eyes()) {
        DrawableRegion clear_region;
        clear_region.set_clear_depth_active(true);
        clear_region.set_clear_depth(dr->get_clear_depth());
        gsg->clear(&clear_region);
      }
      gsg->prepare_display_region(dr_reader, Lens::SC_right);
      gsg->set_scene(scene_setup);
      if (gsg->begin_scene()) {
        delete dr_reader;
        dr_reader = NULL;
        cull_result->draw(current_thread);
        gsg->end_scene();
      }
    }

  } else {
    // For a mono DisplayRegion, or a left/right eye only
    // DisplayRegion, we just render that.
    gsg->prepare_display_region(dr_reader, dr_reader->get_stereo_channel());

    if (!gsg->set_scene(scene_setup)) {
      // The scene or lens is inappropriate somehow.
      display_cat.error()
        << gsg->get_type() << " cannot render scene with specified lens.\n";
    } else {
      if (dr_reader->is_any_clear_active()) {
        gsg->clear(dr_reader->get_object());
      }
      if (gsg->begin_scene()) {
        delete dr_reader;
        dr_reader = NULL;
        cull_result->draw(current_thread);
        gsg->end_scene();
      }
    }
  }

  if (dr_reader != (DisplayRegionPipelineReader *)NULL) {
    delete dr_reader;
  }
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::do_add_window
//       Access: Private
//  Description: An internal function called by make_window() and
//               make_buffer() and similar functions to add the
//               newly-created GraphicsOutput object to the engine's
//               list of windows, and to request that the window be
//               opened.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
do_add_window(GraphicsOutput *window,
              const GraphicsThreadingModel &threading_model) {
  ReMutexHolder holder(_lock);

  // We have a special counter that is unique per window that allows
  // us to assure that recently-added windows end up on the end of the
  // list.
  window->_internal_sort_index = _window_sort_index;
  ++_window_sort_index;

  _windows_sorted = false;
  _windows.push_back(window);

  WindowRenderer *cull = 
    get_window_renderer(threading_model.get_cull_name(),
                        threading_model.get_cull_stage());
  WindowRenderer *draw = 
    get_window_renderer(threading_model.get_draw_name(),
                        threading_model.get_draw_stage());
  
  if (threading_model.get_cull_sorting()) {
    cull->add_window(cull->_cull, window);
    draw->add_window(draw->_draw, window);
  } else {
    cull->add_window(cull->_cdraw, window);
  }
  
  // Ask the pipe which thread it prefers to run its windowing
  // commands in (the "window thread").  This is the thread that
  // handles the commands to open, resize, etc. the window.  X
  // requires this to be done in the app thread (along with all the
  // other windows, since X is strictly single-threaded), but Windows
  // requires this to be done in draw (because once an OpenGL context
  // has been bound in a given thread, it cannot subsequently be bound
  // in any other thread, and we have to bind a context in
  // open_window()).
  
  switch (window->get_pipe()->get_preferred_window_thread()) {
  case GraphicsPipe::PWT_app:
    _app.add_window(_app._window, window);
    break;

  case GraphicsPipe::PWT_draw:
    draw->add_window(draw->_window, window);
    break;
  }

  if (display_cat.is_debug()) {
    display_cat.debug()
      << "Created " << window->get_type() << " " << (void *)window << "\n";
  }

  window->request_open();
  _needs_open_windows = true;
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::do_add_gsg
//       Access: Private
//  Description: An internal function called by make_output to add
//               the newly-created gsg object to the engine's
//               list of gsg's.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
do_add_gsg(GraphicsStateGuardian *gsg, GraphicsPipe *pipe,
           const GraphicsThreadingModel &threading_model) {
  ReMutexHolder holder(_lock);

  gsg->_threading_model = threading_model;
  gsg->_pipe = pipe;
  gsg->_engine = this;

  WindowRenderer *draw = 
    get_window_renderer(threading_model.get_draw_name(),
                        threading_model.get_draw_stage());

  draw->add_gsg(gsg);
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::do_remove_window
//       Access: Private
//  Description: An internal function called by remove_window() and
//               remove_all_windows() to actually remove the indicated
//               window from all relevant structures, except the
//               _windows list itself.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
do_remove_window(GraphicsOutput *window, Thread *current_thread) {
  PT(GraphicsPipe) pipe = window->get_pipe();
  window->_pipe = (GraphicsPipe *)NULL;

  if (!_windows_sorted) {
    do_resort_windows();
  }

  // Now remove the window from all threads that know about it.
  _app.remove_window(window);
  Threads::const_iterator ti;
  for (ti = _threads.begin(); ti != _threads.end(); ++ti) {
    RenderThread *thread = (*ti).second;
    thread->remove_window(window);
  }

  // If the window happened to be controlled by the app thread, we
  // might as well close it now rather than waiting for next frame.
  _app.do_pending(this, current_thread);

  if (display_cat.is_debug()) {
    display_cat.debug()
      << "Removed " << window->get_type() << " " << (void *)window << "\n";
  }
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::do_resort_windows
//       Access: Private
//  Description: Resorts all of the Windows lists.  This may need to
//               be done if one or more of the windows' sort
//               properties has changed.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
do_resort_windows() {
  _windows_sorted = true;

  _app.resort_windows();
  Threads::const_iterator ti;
  for (ti = _threads.begin(); ti != _threads.end(); ++ti) {
    RenderThread *thread = (*ti).second;
    thread->resort_windows();
  }

  _windows.sort();
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::terminate_threads
//       Access: Private
//  Description: Signals our child threads to terminate and waits for
//               them to clean up.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
terminate_threads(Thread *current_thread) {
  ReMutexHolder holder(_lock, current_thread);

  // We spend almost our entire time in this method just waiting for
  // threads.  Time it appropriately.
  PStatTimer timer(_wait_pcollector, current_thread);
  
  // First, wait for all the threads to finish their current frame.
  // Grabbing the mutex should achieve that.
  Threads::const_iterator ti;
  for (ti = _threads.begin(); ti != _threads.end(); ++ti) {
    RenderThread *thread = (*ti).second;
    thread->_cv_mutex.lock();
  }
  
  // Now tell them to close their windows and terminate.
  for (ti = _threads.begin(); ti != _threads.end(); ++ti) {
    RenderThread *thread = (*ti).second;
    thread->_thread_state = TS_terminate;
    thread->_cv_start.signal();
    thread->_cv_mutex.release();
  }

  // Finally, wait for them all to finish cleaning up.
  for (ti = _threads.begin(); ti != _threads.end(); ++ti) {
    RenderThread *thread = (*ti).second;
    thread->join();
  }
  
  _threads.clear();
}


#ifdef DO_PSTATS
////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::pstats_count_cycler_type
//       Access: Private, Static
//  Description: A callback function for
//               Pipeline::iterate_all_cycler_types() to report the
//               cycler types to PStats.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
pstats_count_cycler_type(TypeHandle type, int count, void *data) {
  GraphicsEngine *self = (GraphicsEngine *)data;
  CyclerTypeCounters::iterator ci = self->_all_cycler_types.find(type);
  if (ci == self->_all_cycler_types.end()) {
    PStatCollector collector(_cyclers_pcollector, type.get_name());
    ci = self->_all_cycler_types.insert(CyclerTypeCounters::value_type(type, collector)).first;
  }
  (*ci).second.set_level(count);
}
#endif // DO_PSTATS

#ifdef DO_PSTATS
////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::pstats_count_dirty_cycler_type
//       Access: Private, Static
//  Description: A callback function for
//               Pipeline::iterate_dirty_cycler_types() to report the
//               cycler types to PStats.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::
pstats_count_dirty_cycler_type(TypeHandle type, int count, void *data) {
  GraphicsEngine *self = (GraphicsEngine *)data;
  CyclerTypeCounters::iterator ci = self->_dirty_cycler_types.find(type);
  if (ci == self->_dirty_cycler_types.end()) {
    PStatCollector collector(_dirty_cyclers_pcollector, type.get_name());
    ci = self->_dirty_cycler_types.insert(CyclerTypeCounters::value_type(type, collector)).first;
  }
  (*ci).second.set_level(count);
}
#endif // DO_PSTATS

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::get_invert_polygon_state
//       Access: Protected, Static
//  Description: Returns a RenderState for inverting the sense of
//               polygon vertex ordering: if the scene graph specifies
//               a clockwise ordering, this changes it to
//               counterclockwise, and vice-versa.
////////////////////////////////////////////////////////////////////
const RenderState *GraphicsEngine::
get_invert_polygon_state() {
  // Once someone asks for this pointer, we hold its reference count
  // and never free it.
  static CPT(RenderState) state = (const RenderState *)NULL;
  if (state == (const RenderState *)NULL) {
    state = RenderState::make(CullFaceAttrib::make_reverse());
  }

  return state;
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::get_window_renderer
//       Access: Private
//  Description: Returns the WindowRenderer with the given name.
//               Creates a new RenderThread if there is no such thread
//               already.  The pipeline_stage number specifies the
//               pipeline stage that will be assigned to the thread
//               (unless was previously given a higher stage).
//
//               You must already be holding the lock before calling
//               this method.
////////////////////////////////////////////////////////////////////
GraphicsEngine::WindowRenderer *GraphicsEngine::
get_window_renderer(const string &name, int pipeline_stage) {
  nassertr(_lock.debug_is_locked(), NULL);

  if (name.empty()) {
    return &_app;
  }

  Threads::iterator ti = _threads.find(name);
  if (ti != _threads.end()) {
    return (*ti).second.p();
  }

  PT(RenderThread) thread = new RenderThread(name, this);
  thread->set_min_pipeline_stage(pipeline_stage);
  _pipeline->set_min_stages(pipeline_stage + 1);

  bool started = thread->start(TP_normal, true);
  nassertr(started, thread.p());
  _threads[name] = thread;

  nassertr(thread->get_pipeline_stage() < _pipeline->get_num_stages(), thread.p());

  return thread.p();
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::WindowRenderer::Constructor
//       Access: Public
//  Description: 
////////////////////////////////////////////////////////////////////
GraphicsEngine::WindowRenderer::
WindowRenderer(const string &name) :
  _wl_lock(string("GraphicsEngine::WindowRenderer::_wl_lock ") + name)
{
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::WindowRenderer::add_gsg
//       Access: Public
//  Description: Adds a new GSG to the _gsg list, if it is not already
//               there.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::WindowRenderer::
add_gsg(GraphicsStateGuardian *gsg) {
  ReMutexHolder holder(_wl_lock);
  _gsgs.insert(gsg);
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::WindowRenderer::add_window
//       Access: Public
//  Description: Adds a new window to the indicated list, which should
//               be a member of the WindowRenderer.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::WindowRenderer::
add_window(Windows &wlist, GraphicsOutput *window) {
  ReMutexHolder holder(_wl_lock);
  wlist.insert(window);
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::WindowRenderer::remove_window
//       Access: Public
//  Description: Immediately removes the indicated window from all
//               lists.  If the window is currently open and is
//               already on the _window list, moves it to the _pending_close
//               list for later closure.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::WindowRenderer::
remove_window(GraphicsOutput *window) {
  ReMutexHolder holder(_wl_lock);
  PT(GraphicsOutput) ptwin = window;

  _cull.erase(ptwin);
  _cdraw.erase(ptwin);
  _draw.erase(ptwin);

  Windows::iterator wi;

  wi = _window.find(ptwin);
  if (wi != _window.end()) {
    // The window is on our _window list, meaning its open/close
    // operations (among other window ops) are serviced by this
    // thread.

    // Make sure the window isn't about to request itself open.
    ptwin->request_close();

    // If the window is already open, move it to the _pending_close list so
    // it can be closed later.  We can't close it immediately, because
    // we might not have been called from the subthread.
    if (ptwin->is_valid()) {
      _pending_close.push_back(ptwin);
    }

    _window.erase(wi);
  }
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::WindowRenderer::resort_windows
//       Access: Public
//  Description: Resorts all the lists of windows, assuming they may
//               have become unsorted.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::WindowRenderer::
resort_windows() {
  ReMutexHolder holder(_wl_lock);

  _cull.sort();
  _cdraw.sort();
  _draw.sort();
  _window.sort();

  if (display_cat.is_debug()) {
    display_cat.debug()
      << "Windows resorted:";
    Windows::const_iterator wi;
    for (wi = _window.begin(); wi != _window.end(); ++wi) {
      GraphicsOutput *win = (*wi);
      display_cat.debug(false)
        << " " << win->get_name() << "(" << win->get_sort() << ")";
    }
    display_cat.debug(false)
      << "\n";

    for (wi = _draw.begin(); wi != _draw.end(); ++wi) {
      GraphicsOutput *win = (*wi);
      display_cat.debug(false)
        << " " << win->get_name() << "(" << win->get_sort() << ")";
    }
    display_cat.debug(false)
      << "\n";
  }
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::WindowRenderer::do_frame
//       Access: Public
//  Description: Executes one stage of the pipeline for the current
//               thread: calls cull on all windows that are on the
//               cull list for this thread, draw on all the windows on
//               the draw list, etc.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::WindowRenderer::
do_frame(GraphicsEngine *engine, Thread *current_thread) {
  PStatTimer timer(engine->_do_frame_pcollector, current_thread);
  ReMutexHolder holder(_wl_lock);

  do_callbacks(CB_pre_frame);

  engine->cull_to_bins(_cull, current_thread);
  engine->cull_and_draw_together(_cdraw, current_thread);
  engine->draw_bins(_draw, current_thread);
  engine->process_events(_window, current_thread);

  // If any GSG's on the list have no more outstanding pointers, clean
  // them up.  (We are in the draw thread for all of these GSG's.)
  if (any_done_gsgs()) {
    GSGs new_gsgs;
    GSGs::iterator gi;
    for (gi = _gsgs.begin(); gi != _gsgs.end(); ++gi) {
      GraphicsStateGuardian *gsg = (*gi);
      if (gsg->get_ref_count() == 1) {
        // This one has no outstanding pointers; clean it up.
        GraphicsPipe *pipe = gsg->get_pipe();
        engine->close_gsg(pipe, gsg);
      } else { 
        // This one is ok; preserve it.
        new_gsgs.insert(gsg);
      }
    }

    _gsgs.swap(new_gsgs);
  }

  do_callbacks(CB_post_frame);
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::WindowRenderer::do_windows
//       Access: Public
//  Description: Attempts to fully open or close any windows or
//               buffers associated with this thread, but does not
//               otherwise perform any rendering.  (Normally, this
//               step is handled during do_frame(); call this method
//               only if you want these things to open immediately.)
////////////////////////////////////////////////////////////////////
void GraphicsEngine::WindowRenderer::
do_windows(GraphicsEngine *engine, Thread *current_thread) {
  ReMutexHolder holder(_wl_lock);

  engine->process_events(_window, current_thread);

  engine->make_contexts(_cdraw, current_thread);
  engine->make_contexts(_draw, current_thread);
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::WindowRenderer::do_flip
//       Access: Public
//  Description: Flips the windows as appropriate for the current
//               thread.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::WindowRenderer::
do_flip(GraphicsEngine *engine, Thread *current_thread) {
  ReMutexHolder holder(_wl_lock);
  engine->flip_windows(_cdraw, current_thread);
  engine->flip_windows(_draw, current_thread);
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::WindowRenderer::do_close
//       Access: Public
//  Description: Closes all the windows on the _window list.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::WindowRenderer::
do_close(GraphicsEngine *engine, Thread *current_thread) {
  ReMutexHolder holder(_wl_lock);
  Windows::iterator wi;
  for (wi = _window.begin(); wi != _window.end(); ++wi) {
    GraphicsOutput *win = (*wi);
    win->set_close_now();
  }

  // Also close all of the GSG's.
  GSGs new_gsgs;
  GSGs::iterator gi;
  for (gi = _gsgs.begin(); gi != _gsgs.end(); ++gi) {
    GraphicsStateGuardian *gsg = (*gi);
    if (gsg->get_ref_count() == 1) {
      // This one has no outstanding pointers; clean it up.
      GraphicsPipe *pipe = gsg->get_pipe();
      engine->close_gsg(pipe, gsg);
    } else { 
      // This one is ok; preserve it.
      new_gsgs.insert(gsg);
    }
  }
  
  _gsgs.swap(new_gsgs);
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::WindowRenderer::do_pending
//       Access: Public
//  Description: Actually closes any windows that were recently
//               removed from the WindowRenderer.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::WindowRenderer::
do_pending(GraphicsEngine *engine, Thread *current_thread) {
  ReMutexHolder holder(_wl_lock);

  if (!_pending_close.empty()) {
    if (display_cat.is_debug()) {
      display_cat.debug()
        << "_pending_close.size() = " << _pending_close.size() << "\n";
    }

    // Close any windows that were pending closure.
    Windows::iterator wi;
    for (wi = _pending_close.begin(); wi != _pending_close.end(); ++wi) {
      GraphicsOutput *win = (*wi);
      win->set_close_now();
    }
    _pending_close.clear();
  }
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::WindowRenderer::any_done_gsgs
//       Access: Public
//  Description: Returns true if any of the GSG's on this thread's
//               draw list are done (they have no outstanding pointers
//               other than this one), or false if all of them are
//               still good.
////////////////////////////////////////////////////////////////////
bool GraphicsEngine::WindowRenderer::
any_done_gsgs() const {
  GSGs::const_iterator gi;
  for (gi = _gsgs.begin(); gi != _gsgs.end(); ++gi) {
    if ((*gi)->get_ref_count() == 1) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::WindowRenderer::add_callback
//       Access: Public
//  Description: Adds the indicated callback to this renderer's list
//               for the indicated callback time.  Returns true if it
//               is successfully added, false if it was already there.
////////////////////////////////////////////////////////////////////
bool GraphicsEngine::WindowRenderer::
add_callback(GraphicsEngine::CallbackTime callback_time, 
             const GraphicsEngine::Callback &callback) {
  nassertr(callback_time >= 0 && callback_time < CB_len, false);
  ReMutexHolder holder(_wl_lock);
  return _callbacks[callback_time].insert(callback).second;
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::WindowRenderer::remove_callback
//       Access: Public
//  Description: Removes the indicated callback from this renderer's
//               list for the indicated callback time.  Returns true
//               if it is successfully removed, or false if it was not
//               on the list.
////////////////////////////////////////////////////////////////////
bool GraphicsEngine::WindowRenderer::
remove_callback(GraphicsEngine::CallbackTime callback_time, 
                const GraphicsEngine::Callback &callback) {
  nassertr(callback_time >= 0 && callback_time < CB_len, false);
  ReMutexHolder holder(_wl_lock);
  Callbacks::iterator cbi = _callbacks[callback_time].find(callback);
  if (cbi != _callbacks[callback_time].end()) {
    _callbacks[callback_time].erase(cbi);
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::WindowRenderer::do_callbacks
//       Access: Private
//  Description: Calls all of the callback functions on the indicated
//               list.  Intended to be called internally when we have
//               reached the indicated point on the frame.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::WindowRenderer::
do_callbacks(GraphicsEngine::CallbackTime callback_time) {
  nassertv(callback_time >= 0 && callback_time < CB_len);
  Callbacks::const_iterator cbi;
  for (cbi = _callbacks[callback_time].begin();
       cbi != _callbacks[callback_time].end();
       ++cbi) {
    (*cbi).do_callback();
  }
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::RenderThread::Constructor
//       Access: Public
//  Description: 
////////////////////////////////////////////////////////////////////
GraphicsEngine::RenderThread::
RenderThread(const string &name, GraphicsEngine *engine) : 
  Thread(name, "Main"),
  WindowRenderer(name),
  _engine(engine),
  _cv_mutex(string("GraphicsEngine::RenderThread ") + name),
  _cv_start(_cv_mutex),
  _cv_done(_cv_mutex)
{
  _thread_state = TS_wait;
}

////////////////////////////////////////////////////////////////////
//     Function: GraphicsEngine::RenderThread::thread_main
//       Access: Public, Virtual
//  Description: The main loop for a particular render thread.  The
//               thread will process whatever cull or draw windows it
//               has assigned to it.
////////////////////////////////////////////////////////////////////
void GraphicsEngine::RenderThread::
thread_main() {
  Thread *current_thread = Thread::get_current_thread();

  MutexHolder holder(_cv_mutex);
  while (true) {
    nassertv(_cv_mutex.debug_is_locked());

    switch (_thread_state) {
    case TS_wait:
      break;

    case TS_do_frame:
      do_pending(_engine, current_thread);
      do_frame(_engine, current_thread);
      break;

    case TS_do_flip:
      do_flip(_engine, current_thread);
      break;

    case TS_do_release:
      do_pending(_engine, current_thread);
      break;

    case TS_do_windows:
      do_windows(_engine, current_thread);
      do_pending(_engine, current_thread);
      break;

    case TS_terminate:
      do_pending(_engine, current_thread);
      do_close(_engine, current_thread);
      _thread_state = TS_done;
      _cv_done.signal();
      return;

    case TS_done:
      // Shouldn't be possible to get here.
      nassertv(false);
      return;
    }

    _thread_state = TS_wait;
    _cv_done.signal();

    {
      PStatTimer timer(_wait_pcollector, current_thread);
      _cv_start.wait();
    }
  }
}
