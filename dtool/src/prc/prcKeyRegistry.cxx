// Filename: prcKeyRegistry.cxx
// Created by:  drose (19Oct04)
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

#include "prcKeyRegistry.h"

// This file requires OpenSSL to compile, because we use routines in
// the OpenSSL library to manage keys and to sign and validate
// signatures.

#ifdef HAVE_SSL

#include <openssl/pem.h>

PrcKeyRegistry *PrcKeyRegistry::_global_ptr = NULL;

////////////////////////////////////////////////////////////////////
//     Function: PrcKeyRegistry::Constructor
//       Access: Protected
//  Description: There is only one PrcKeyRegistry in the world; use
//               get_global_ptr() to get it.
////////////////////////////////////////////////////////////////////
PrcKeyRegistry::
PrcKeyRegistry() {
}

////////////////////////////////////////////////////////////////////
//     Function: PrcKeyRegistry::Destructor
//       Access: Protected
//  Description: 
////////////////////////////////////////////////////////////////////
PrcKeyRegistry::
~PrcKeyRegistry() {
  cerr << "Internal error--PrcKeyRegistry destructor called!\n";
}

////////////////////////////////////////////////////////////////////
//     Function: PrcKeyRegistry::record_keys
//       Access: Public
//  Description: Records the list of public keys that are compiled
//               into this executable.  The pointer is assumed to be
//               to an area of static memory that will not be
//               destructed, so the data is not copied, but only the
//               pointer is assigned.
//
//               This method is normally called after including the
//               code generated by the make-prc-key utility.
////////////////////////////////////////////////////////////////////
void PrcKeyRegistry::
record_keys(const KeyDef *key_def, int num_keys) {
  for (int i = 0; i < num_keys; i++) {
    const KeyDef *def = &key_def[i];
    if (def->_data != NULL) {
      // Clear the ith key.
      while ((int)_keys.size() <= i) {
        Key key;
        key._def = NULL;
        key._pkey = NULL;
        key._generated_time = 0;
        _keys.push_back(key);
      }
      if (_keys[i]._def != def) {
        if (_keys[i]._pkey != (EVP_PKEY *)NULL) {
          EVP_PKEY_free(_keys[i]._pkey);
          _keys[i]._pkey = NULL;
        }
        _keys[i]._def = def;
        _keys[i]._generated_time = def->_generated_time;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////
//     Function: PrcKeyRegistry::set_key
//       Access: Public
//  Description: Sets the nth public key in the registry to the given
//               value.  The EVP_PKEY structure must have been
//               properly allocated view EVP_PKEY_new(); its ownership
//               is transferred to the registry and it will eventually
//               be freed via EVP_PKEY_free().
////////////////////////////////////////////////////////////////////
void PrcKeyRegistry::
set_key(int n, EVP_PKEY *pkey, time_t generated_time) {
  // Clear the nth key.
  while ((int)_keys.size() <= n) {
    Key key;
    key._def = NULL;
    key._pkey = NULL;
    key._generated_time = 0;
    _keys.push_back(key);
  }
  _keys[n]._def = NULL;
  if (_keys[n]._pkey != (EVP_PKEY *)NULL) {
    EVP_PKEY_free(_keys[n]._pkey);
    _keys[n]._pkey = NULL;
  }
  _keys[n]._pkey = pkey;
  _keys[n]._generated_time = generated_time;
}

////////////////////////////////////////////////////////////////////
//     Function: PrcKeyRegistry::get_num_keys
//       Access: Public
//  Description: Returns the number of public keys in the registry.
//               This is actually the highest index number + 1, which
//               might not strictly be the number of keys, since there
//               may be holes in the list.
////////////////////////////////////////////////////////////////////
int PrcKeyRegistry::
get_num_keys() const {
  return _keys.size();
}

////////////////////////////////////////////////////////////////////
//     Function: PrcKeyRegistry::get_key
//       Access: Public
//  Description: Returns the nth public key, or NULL if the nth key is
//               not defined.
////////////////////////////////////////////////////////////////////
EVP_PKEY *PrcKeyRegistry::
get_key(int n) const {
  if (n < 0 || n >= (int)_keys.size()) {
    return NULL;
  }

  if (_keys[n]._def != (KeyDef *)NULL) {
    if (_keys[n]._pkey == (EVP_PKEY *)NULL) {
      // Convert the def to a EVP_PKEY structure.
      const KeyDef *def = _keys[n]._def;
      BIO *mbio = BIO_new_mem_buf((void *)def->_data, def->_length);
      EVP_PKEY *pkey = PEM_read_bio_PUBKEY(mbio, NULL, NULL, NULL);
      ((PrcKeyRegistry *)this)->_keys[n]._pkey = pkey;
      BIO_free(mbio);

      if (pkey == (EVP_PKEY *)NULL) {
        // Couldn't read the bio for some reason.
        ((PrcKeyRegistry *)this)->_keys[n]._def = NULL;
      }
    }
  }

  return _keys[n]._pkey;
}

////////////////////////////////////////////////////////////////////
//     Function: PrcKeyRegistry::get_generated_time
//       Access: Public
//  Description: Returns the timestamp at which the indicated key was
//               generated, or 0 if the key is not defined.
////////////////////////////////////////////////////////////////////
time_t PrcKeyRegistry::
get_generated_time(int n) const {
  if (n < 0 || n >= (int)_keys.size()) {
    return 0;
  }

  return _keys[n]._generated_time;
}

////////////////////////////////////////////////////////////////////
//     Function: PrcKeyRegistry::get_global_ptr
//       Access: Public, Static
//  Description: 
////////////////////////////////////////////////////////////////////
PrcKeyRegistry *PrcKeyRegistry::
get_global_ptr() {
  if (_global_ptr == (PrcKeyRegistry *)NULL) {
    _global_ptr = new PrcKeyRegistry;
  }
  return _global_ptr;
}

#endif  // HAVE_SSL
