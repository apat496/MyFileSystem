#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <bsd/string.h>
#include <assert.h>
#include <stdlib.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "storage.h"
#include "map.h"

// implementation for: man 2 access
// Checks if a file exists.
int
nufs_access(const char *path, int mask)
{
    printf("access(%s)\n", path);
    return get_inode(path) ? 0 : -ENOENT;
}

// implementation for: man 2 stat
// gets an object's attributes (type, permissions, size, etc)
int
nufs_getattr(const char *path, struct stat *st)
{
    printf("getattr(%s)\n", path);
    inode* inode = get_inode(path);
    return inode ? get_stat(inode, st) : -ENOENT;
}

// implementation for: man 2 readdir
// lists the contents of a directory
int
nufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
             off_t offset, struct fuse_file_info *fi)
{
    printf("readdir(%s)\n", path);

    struct stat st;
    inode* dir = get_inode(path);
    
    if (!dir)
    {
        return -ENOENT;
    }

    get_stat(dir, &st);

    // filler is a callback that adds one item to the result
    // it will return non-zero when the buffer is full
    filler(buf, ".", &st, 0);

    map* dirmap = get_block_num(dir->block);
    for (int i = 0; i < dirmap->size; i++)
    {
        entry e = dirmap->entries[i];
        get_stat(get_inode_num(e.inode_num), &st);
        filler(buf, e.name, &st, 0);
    }

    return 0;
}

// mknod makes a filesystem object like a file or directory
// called for: man 2 open, man 2 link
int
nufs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    printf("mknod(%s, %04o)\n", path, mode);
    return make_inode(path, mode);
}

// most of the following callbacks implement
// another system call; see section 2 of the manual
int
nufs_mkdir(const char *path, mode_t mode)
{
    printf("mkdir(%s, %04o)\n", path, mode);
    return make_inode(path, S_IFDIR | mode);
}

int
nufs_unlink(const char *path)
{
    printf("unlink(%s)\n", path);
    return unlink_inode(path, 0);
}

int
nufs_rmdir(const char *path)
{
    printf("rmdir(%s)\n", path);
    if (strcmp(path + strlen(path) - 2, "..") == 0)
    {
        return -ENOTEMPTY;
    }

    if (path[strlen(path) - 1] == '.')
    {
        return -EINVAL;
    }
    return unlink_inode(path, 1);
}

// implements: man 2 rename
// called to move a file within the same filesystem
int
nufs_rename(const char *from, const char *to)
{
    printf("rename(%s => %s)\n", from, to);
    int rv = link_inode(from, to);
    if (rv != 0)
        return rv;

    rv = unlink_inode(from, 0);
    return rv;
}

int
nufs_chmod(const char *path, mode_t mode)
{
    printf("chmod(%s, %04o)\n", path, mode);
    inode* inode = get_inode(path);

    if (!inode)
    {
        return -ENOENT;
    }

    inode->mode = mode;

    return 0;
}

int
nufs_truncate(const char *path, off_t size)
{
    printf("truncate(%s, %ld bytes)\n", path, size);
    return -1;
}

// this is called on open, but doesn't need to do much
// since FUSE doesn't assume you maintain state for
// open files.
int
nufs_open(const char *path, struct fuse_file_info *fi)
{
    printf("open(%s)\n", path);
    return get_inode(path) ? 0 : -ENOENT;
}

// Actually read data
int
nufs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("read(%s, %ld bytes, @%ld)\n", path, size, offset);
    inode* inode = get_inode(path);

    if (!inode)
    {
        return -ENOENT;
    }

    if (inode->size < offset)
    {
        return 0;
    }

    char* data = get_data(inode);
    data += offset;
    
    strlcpy(buf, data, size + 1);
    free(data - offset);
    return size;
}

// Actually write data
int
nufs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("write(%s, %ld bytes, @%ld)\n", path, size, offset);
    inode* inode = get_inode(path);

    if (!inode)
    {
        return -ENOENT;
    }

    return write_data(inode, buf, size, offset);
}

// Update the timestamps on a file or directory.
int
nufs_utimens(const char* path, const struct timespec ts[2])
{
    printf("utimens(%s, [%ld, %ld; %ld %ld])\n",
           path, ts[0].tv_sec, ts[0].tv_nsec, ts[1].tv_sec, ts[1].tv_nsec);

    if (!ts)
    {
        return -EACCES;
    }

    inode* inode = get_inode(path);

    if (!inode)
    {
        return -ENOENT;
    }

    inode->mtime = ts[1].tv_sec;
    return 0;
}

int
nufs_link(const char* from, const char* to)
{
    return link_inode(from, to);
}

void
nufs_init_ops(struct fuse_operations* ops)
{
    memset(ops, 0, sizeof(struct fuse_operations));
    ops->access   = nufs_access;
    ops->getattr  = nufs_getattr;
    ops->readdir  = nufs_readdir;
    ops->mknod    = nufs_mknod;
    ops->mkdir    = nufs_mkdir;
    ops->unlink   = nufs_unlink;
    ops->rmdir    = nufs_rmdir;
    ops->rename   = nufs_rename;
    ops->chmod    = nufs_chmod;
    ops->truncate = nufs_truncate;
    ops->open     = nufs_open;
    ops->read     = nufs_read;
    ops->write    = nufs_write;
    ops->utimens  = nufs_utimens;
    ops->link     = nufs_link;
};

struct fuse_operations nufs_ops;

int
main(int argc, char *argv[])
{
    assert(argc > 2 && argc < 6);
    storage_init(argv[--argc]);
    nufs_init_ops(&nufs_ops);
    return fuse_main(argc, argv, &nufs_ops, NULL);
}

