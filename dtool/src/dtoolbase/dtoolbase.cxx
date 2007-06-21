// Filename: dtoolbase.cxx
// Created by:  drose (12Sep00)
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

#include "dtoolbase.h"

#if defined(USE_TAU) && defined(WIN32)
// Hack around tau's lack of DLL export declarations for Profiler class.
bool __tau_shutdown = false;
#endif


/////////////////////////////////////////////////////////////////////
//
// Memory manager: DLMALLOC
//
// This is Doug Lea's memory manager.  It is very fast,
// but it is not thread-safe.
//
/////////////////////////////////////////////////////////////////////

#if defined(USE_MEMORY_DLMALLOC)

#if defined(HAVE_THREADS) && !defined(SIMPLE_THREADS)
#error Cannot use dlmalloc library with threading enabled!
#endif

#define USE_DL_PREFIX 1
#define NO_MALLINFO 1
#include "dlmalloc.h"

void *default_operator_new(size_t size) {
  void *ptr = dlmalloc(size);
  if (ptr == (void *)NULL) {
    cerr << "Out of memory!\n";
    abort();
  }
  return ptr;
}

void default_operator_delete(void *ptr) {
  dlfree(ptr);
}

void default_mark_pointer(void *, size_t, ReferenceCount *) {
}

void *(*global_operator_new)(size_t size) = &default_operator_new;
void (*global_operator_delete)(void *ptr) = &default_operator_delete;
void (*global_mark_pointer)(void *ptr, size_t size, ReferenceCount *ref_ptr) = &default_mark_pointer;

/////////////////////////////////////////////////////////////////////
//
// Memory manager: PTMALLOC2
//
// Ptmalloc2 is a derivative of Doug Lea's memory manager that was 
// made thread-safe by Wolfram Gloger, then was ported to windows by
// Niall Douglas.  It is not quite as fast as dlmalloc (because the
// thread-safety constructs take a certain amount of CPU time), but
// it's still much faster than the windows allocator.
//
/////////////////////////////////////////////////////////////////////

#elif defined(USE_MEMORY_PTMALLOC2) && !defined(linux)
// This doesn't appear to work in Linux; perhaps it is clashing with
// the system library.  On Linux, fall through to the next case
// instead.

#define USE_DL_PREFIX 1
#define NO_MALLINFO 1
#include "ptmalloc2_smp.c"

void *default_operator_new(size_t size) {
  void *ptr = dlmalloc(size);
  if (ptr == (void *)NULL) {
    cerr << "Out of memory!\n";
    abort();
  }
  return ptr;
}

void default_operator_delete(void *ptr) {
  dlfree(ptr);
}

void default_mark_pointer(void *, size_t, ReferenceCount *) {
}

void *(*global_operator_new)(size_t size) = &default_operator_new;
void (*global_operator_delete)(void *ptr) = &default_operator_delete;
void (*global_mark_pointer)(void *ptr, size_t size, ReferenceCount *ref_ptr) = &default_mark_pointer;

/////////////////////////////////////////////////////////////////////
//
// Memory manager: MALLOC
//
// This option uses the built-in system allocator.  This is a good
// choice on linux, but it's a terrible choice on windows.
//
/////////////////////////////////////////////////////////////////////

#elif defined(USE_MEMORY_MALLOC) || defined(USE_MEMORY_PTMALLOC2)

void *default_operator_new(size_t size) {
  void *ptr = malloc(size);
  if (ptr == (void *)NULL) {
    cerr << "Out of memory!\n";
    abort();
  }
  return ptr;
}

void default_operator_delete(void *ptr) {
  free(ptr);
}

void default_mark_pointer(void *, size_t, ReferenceCount *) {
}

void *(*global_operator_new)(size_t size) = &default_operator_new;
void (*global_operator_delete)(void *ptr) = &default_operator_delete;
void (*global_mark_pointer)(void *ptr, size_t size, ReferenceCount *ref_ptr) = &default_mark_pointer;

/////////////////////////////////////////////////////////////////////
//
// Memory manager: NOWRAPPERS
//
// Not only do we use the built-in system definitions for new and
// delete, but we don't even wrap them.  This removes a tiny bit of
// extra overhead from the above option, but prevents Panda's
// MemoryUsage class from being able to track memory allocations.
//
/////////////////////////////////////////////////////////////////////

#else

#endif
