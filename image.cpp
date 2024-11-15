/*
  Flopgen: a tool for automatic creation of FAT-formatted floppy disk images
  Copyright (C) 2020 Maksymilian Graczyk.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "image.hpp"
#include "filediskio.h"

using namespace std;

Image::Image(string filename, FloppySize size, int code_page) {
  file_disk_setup(filename.c_str(), size * BYTES_IN_KB);

  FRESULT res = f_setcp(code_page);

  if (res != 0) {
    goto error;
  }

  MKFS_PARM options;
  options.fmt = FM_SFD | FM_FAT;
  options.align = 0;
  options.n_fat = 2;
  options.n_heads = 2;
  options.d_num = 0x00;
  
  switch (size) {
  case _360K:
    options.au_size = 1024;
    options.n_root = 112;
    options.mdt = 0xFD;
    options.sec_per_track = 9;
    break;

  case _720K:
    options.au_size = 1024;
    options.n_root = 112;
    options.mdt = 0xF9;
    options.sec_per_track = 9;
    break;

  case _1200K:
    options.au_size = 512;
    options.n_root = 224;
    options.mdt = 0xF9;
    options.sec_per_track = 15;
    break;

  case _1440K:
    options.au_size = 512;
    options.n_root = 224;
    options.mdt = 0xF0;
    options.sec_per_track = 18;
    break;

  case _2880K:
    options.au_size = 1024;
    options.n_root = 224;
    options.mdt = 0xF0;
    options.sec_per_track = 36;
    break;
  }

  BYTE buf[FF_MAX_SS];

  res = f_mkfs("", &options, buf, sizeof(buf));

  if (res != 0) {
    goto error;
  }

  fs = new FATFS;

  if (fs == NULL) {
    goto error;
  }

  res = f_mount(fs, "", 1);

  if (res != 0) {
    delete fs;
    goto error;
  }

  open = true;
  return;

 error:
  open = false;
  file_disk_free();
}

Image::~Image() {
  if (open) {
    f_mount(NULL, "", 0);
    delete fs;
    file_disk_free();
  }
}

bool Image::is_open() {
  return open;
}

bool Image::add_file(File *file_object, bool path_with_dirs) {
  fstream *stream = file_object->get_stream();
  UINT size = file_object->get_size();

  FIL file;

  FRESULT res = f_open(&file,
                       file_object->get_path_str(path_with_dirs).c_str(),
                       FA_WRITE | FA_CREATE_ALWAYS);

  if (res != 0) {
    return false;
  }

  char *buffer = new char[size];

  if (buffer == NULL) {
    return false;
  }

  stream->seekg(0, ios::beg);
  stream->read(buffer, size);

  if (stream->rdstate() && !stream->eof()) {
    delete [] buffer;
    return false;
  }

  UINT bytes_written = 0;
  res = f_write(&file, buffer, size, &bytes_written);

  delete [] buffer;
    
  if (res != 0 || bytes_written != size) {
    return false;
  }

  res = f_close(&file);

  if (res != 0) {
    return false;
  }

  return true;
}

bool Image::add_directory(Directory *dir) {
  FRESULT res = f_mkdir(dir->get_path_str().c_str());

  if (res != 0 && res != FR_EXIST) {
    return false;
  }

  for (int i = 0; i < dir->get_file_count(); i++) {
    File *file = (*dir)[i];

    if (file->is_directory()) {
      if (!add_directory((Directory *) file)) {
        return false;
      }
    } else if (!add_file(file)) {
      return false;
    }
  }

  return true;
}

bool Image::operator<<(File *file_object) {
  if (file_object->is_directory()) {
    return add_directory((Directory *) file_object);
  } else {
    return add_file(file_object, false);
  }
}
