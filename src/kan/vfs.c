/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/vfs.h>
#include <kan/symbol.h>
#include <kan/kmalloc.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

vfs_super_t vfs_register_fs(char fs_name[VFS_FILENAME_LENGTH], size_t block_size, void *base){
  vfs_super_t return_super;
  strncpy(return_super.fs_name, fs_name, VFS_FILENAME_LENGTH);
  return_super.fs_block_size = block_size;
  return_super.nblocks = 8;
  return_super.ninodes = 1;
  return_super.base = base;
  return_super.inodes = kmalloc(sizeof(vfs_node_t));
  
  return return_super; 
}
EXPORT_SYMBOL(vfs_register_fs);

int vfs_mount_fs(const char *rootpath, vfs_super_t fs, bool read_only){
  // set inode[0] of superblock to root filepath
  fs.inodes[0].size = fs.fs_block_size;
  fs.inodes[0].nblocks = 1;
  fs.inodes[0].direct = fs.base;
  fs.inodes[0].parent = NULL;
  strncpy(fs.inodes[0].name, rootpath, VFS_FILENAME_LENGTH);
    
  if (read_only){
    for (uint64_t i = 0 ; i < fs.ninodes ; i++){
      fs.inodes[i].read_only = true;
    }
  }
    
  /*need to copy all inodes in superblock other than inode 0 to be children of inode 0
  for (uint64_t i = 0 ; i < fs.ninodes ; i++){
    // insert code here
  }
  */

  return 0;
}
EXPORT_SYMBOL(vfs_mount_fs);
