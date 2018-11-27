#ifndef NUFS_STORAGE_H
#define NUFS_STORAGE_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef struct inode {
    int mode;
    int uid;
    int size;
    time_t mtime;
    int gid;
    int refs;
    int blocks;
    int isdir;
    int block;
    int indirect;
} inode;

void   storage_init(const char* path);
void*  get_block_num(int block_num);
inode* get_inode_num(int inode_num);
inode* get_inode(const char* path);
int    make_inode(const char* path, mode_t mode);
int    unlink_inode(const char* path, int directory);
int    link_inode(const char* path, const char* new);
int    get_stat(inode* inode, struct stat* st);
void*  get_data(inode* inode);
int    write_data(inode* inode, const void* buf, size_t size, off_t offset);

#endif
