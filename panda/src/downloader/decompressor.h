// Filename: decompressor.h
// Created by:  mike (09Jan97)
//
////////////////////////////////////////////////////////////////////
//
#ifndef DECOMPRESSOR_H
#define DECOMPRESSOR_H
//
////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////
#include <pandabase.h>
#include <filename.h>
#include <buffer.h>
#include <pointerTo.h>
#include "zcompressor.h"

////////////////////////////////////////////////////////////////////
//       Class : Decompressor 
// Description :
////////////////////////////////////////////////////////////////////
class EXPCL_PANDAEXPRESS Decompressor {
PUBLISHED:
  Decompressor(void);
  Decompressor(PT(Buffer) buffer);
  virtual ~Decompressor(void);

  int initiate(Filename &source_file);
  int initiate(Filename &source_file, Filename &dest_file);
  int initiate(Ramfile &source_file);
  int run(void);

  bool decompress(Filename &source_file);
  bool decompress(Ramfile &source_file);

  INLINE float get_progress(void) const;

private:
  void init(PT(Buffer) buffer);
  void cleanup(void);

  bool _initiated;
  PT(Buffer) _buffer;
  int _half_buffer_length;
  Filename _temp_file_name;

  Filename _source_file;
  ifstream _read_stream;
  ofstream _write_stream;
  istringstream *_read_string_stream;
  ostringstream *_write_string_stream;
  int _source_file_length;
  int _total_bytes_read;
  bool _read_all_input;
  bool _handled_all_input;
  int _source_buffer_length;
  ZDecompressor *_decompressor;
  bool _decompress_to_ram;
};

#include "decompressor.I"

#endif
