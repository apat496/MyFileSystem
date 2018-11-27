#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include <sys/mman.h>

#include "storage.h"
#include "vector.h"
#include "map.h"

// Constants
const int NUFS_SIZE      = 1024 * 1024; // 1MB
const int INODE_COUNT    = 112;
const int BLOCK_COUNT    = 254;
const int BLOCK_SIZE     = 4096;
const int INDIRECT_COUNT = 4096 / 4;

// Global Pointers for Future Retrievals  
static int* inode_map_base = 0;
static int* block_map_base = 0;
static inode* inode_base   = 0;
static void* block_base    = 0;

// Check bitmap for freeness of given inode number
static int
check_inode_free(int inode_num)
{
    return !inode_map_base[inode_num];
}

// Check bit for freeness of given block number
static int
check_block_free(int block_num)
{
    return !block_map_base[block_num];
}

// Allocate a new inode and return its number
static int
allocate_inode()
{
    for (int i = 0; i < INODE_COUNT; i++)
    {
        if (check_inode_free(i))
        {
            inode_map_base[i] = 1;
            memset(inode_base + i, 0, sizeof(inode));
            return i;
        }
    }
    return -1;
}

// Allocate a new block and return its number
static int
allocate_block()
{
    for (int i = 0; i < BLOCK_COUNT; i++)
    {
        if (check_block_free(i))
        {
            block_map_base[i] = 1;
            memset(block_base + i * BLOCK_SIZE, 0, BLOCK_SIZE);
            return i;
        }
    }
    return -1;
}

// Initialize Filesystem
void
storage_init(const char* path)
{
    // Are we setting up for first time?
    int setup = 0;

    // Only create new file
    int nufs_fd = open(path, O_CREAT | O_EXCL | O_RDWR, 0644);

    // File already exists(probably)
    if (nufs_fd == -1)
    {
        // Open file 
        nufs_fd = open(path, O_RDWR);
        assert(nufs_fd != -1);
    }
    else
    {
        // File created let's zero it out
        char buff[NUFS_SIZE];
        memset(buff, 0, NUFS_SIZE);
        write(nufs_fd, buff, NUFS_SIZE);

        // Setting up for first time
        setup = 1;
    }

    // Truncate to write size
    int rv = ftruncate(nufs_fd, NUFS_SIZE);
    assert(rv == 0);

    // Map file into memory
    inode_map_base = mmap(0, NUFS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, nufs_fd, 0);
    assert(inode_map_base != MAP_FAILED);

    // Set Pointers for future retrievals
    block_map_base = inode_map_base + INODE_COUNT;
    inode_base = (inode*)(block_map_base + BLOCK_COUNT);
    block_base = (void*)(inode_base + INODE_COUNT);

    // Set up root directory
    if (setup)
    {
        assert(allocate_inode() == 0);
        inode_base->mode     = S_IFDIR | 0755;
        inode_base->uid      = getuid();
        inode_base->size     = 4;
        inode_base->mtime    = time(0);
        inode_base->gid      = getgid();
        inode_base->refs     = 2;
        inode_base->blocks   = 1;
        inode_base->isdir    = 1;
        inode_base->block    = allocate_block();
        inode_base->indirect = -1;
    }
}

// Get pointer for block of given number
void*
get_block_num(int block_num)
{
    return block_base + block_num * BLOCK_SIZE;
}

// Get pointer for inode of given number
inode*
get_inode_num(int inode_num)
{
    return inode_base + inode_num;
}

// Get inode pointer for given path
inode*
get_inode(const char* path)
{
    // Split Path by directory delimiters
    vector* dirs = str_split(path, '/');

    // Start at root directory
    inode* it = inode_base;

    // Return root directory if asked for
    if (strcmp("/", path) == 0)
    {
        // Clean Up
        delete_vector(dirs);

        return it;
    }

    // Loop through path until found
    for (int path_iter = 0; path_iter < dirs->size; path_iter++)
    {
        // Get next directory/filename
        char* name = vector_get(dirs, path_iter);

        // We are in a directory
        if (it->isdir)
        {
            // Find block containing directory map
            map* dirmap = get_block_num(it->block);

            // Get inode number for next directory/filename
            int inode_num = map_get(dirmap, name);

            // Check for Failure
            if (inode_num == -1)
            {
                // Clean Up
                delete_vector(dirs);

                return NULL;
            }

            // Move to next directory
            it = get_inode_num(inode_num);
        }

    }

    // Clean Up
    delete_vector(dirs);

    // Return where we got out of iteration loop
    return it;
}

// Make an inode at the given path and return its number
int
make_inode(const char* path, mode_t mode)
{
    int inode_num = -1;

    // Split Path by directory delimiters
    vector* dirs = str_split(path, '/');

    // Start at root directory
    inode* it = inode_base;

    // Loop through path until found
    for (int path_iter = 0; path_iter < dirs->size; path_iter++)
    {
        // Get next directory/filename
        char* name = vector_get(dirs, path_iter);

        // We are in a directory
        if (it->isdir)
        {
            // Find block containing directory map
            map* dirmap = get_block_num(it->block);

            // Get inode number for next directory/filename
            int inode_num = map_get(dirmap, name);

            // Last piece of path
            if (path_iter == dirs->size - 1)
            {
                // Already Exists
                if (inode_num != -1)
                {
                    // Clean Up
                    delete_vector(dirs);

                    return -EEXIST;
                }

                // Make node
                inode_num = allocate_inode();

                if (inode_num == -1)
                {
                    // Clean Up
                    delete_vector(dirs);

                    return -EDQUOT;
                }

                inode* inode = get_inode_num(inode_num);
                inode->mode  = mode;
                inode->uid   = getuid();
                inode->size  = S_ISDIR(mode) ? 4 : 0;
                inode->mtime = time(0);
                inode->gid   = getgid();
                inode->refs  = S_ISDIR(mode) ? 2 : 1;
                inode->isdir = S_ISDIR(mode);
                inode->block = allocate_block();
                inode->indirect = -1;

                if (inode->block == -1)
                {
                    // Clean Up
                    delete_vector(dirs);

                    return -EDQUOT;
                }

                inode->blocks = 1;

                map_add(dirmap, name, inode_num);
                it->refs++;
                
                // Clean Up
                delete_vector(dirs);

                return 0;
            }

            // Check for Failure
            if (inode_num == -1)
            {
                return -ENOENT;
            }

            // Move to next directory
            it = get_inode_num(inode_num);
        }
        else
        {
            // Clean Up
            delete_vector(dirs);

            return -ENOTDIR;
        }

    }

    // Clean Up
    delete_vector(dirs);

    // Return inode number
    return inode_num;
}

// Unlink the given path from its inode and delete the inode if necessary
int
unlink_inode(const char* path, int directory)
{
    // Split Path by directory delimiters
    vector* dirs = str_split(path, '/');

    // Start at root directory
    inode* it = inode_base;

    // Loop through path until found
    for (int path_iter = 0; path_iter < dirs->size; path_iter++)
    {
        // Get next directory/filename
        char* name = vector_get(dirs, path_iter);

        // We are in a directory
        if (it->isdir)
        {
            // Find block containing directory map
            map* dirmap = get_block_num(it->block);

            // Get inode number for next directory/filename
            int inode_num = map_get(dirmap, name);

            // Last piece of path
            if (path_iter == dirs->size - 1)
            {
                // Doesn't Exist
                if (inode_num == -1)
                {
                    // Clean Up
                    delete_vector(dirs);

                    return -ENOENT;
                }

                // Deal with directories
                if (get_inode_num(inode_num)->isdir && !directory)
                {
                    return -EISDIR;
                }

                // Deal with non-directories
                if (!get_inode_num(inode_num)->isdir && directory)
                {
                    return -ENOTDIR;
                }

                // Remove from Directory Map
                map_remove(dirmap, name);
                it->refs--;

                // Clean up if last reference
                if (!it->refs)
                {
                    block_map_base[it->block] = 0;
                    int* block_nums = (it->indirect != -1) ? get_block_num(it->indirect) : NULL;
                    for (int i = 0; i < it->blocks - 1; i++)
                    {
                        block_map_base[block_nums[i]] = 0;
                    }
                }

                // Clean Up
                delete_vector(dirs);

                return 0;
            }

            // Move to next directory
            it = get_inode_num(inode_num);
        }
        else
        {
            // Clean Up
            delete_vector(dirs);

            return -ENOTDIR;
        }

    }

    // Clean Up
    delete_vector(dirs);

    return 0;
}

// Create a hard link from given path to new one
int
link_inode(const char* path, const char* new)
{
    // Split Path by directory delimiters
    vector* dirs = str_split(new, '/');

    // Start at root directory
    inode* it = inode_base;

    inode* node = get_inode(path);
    int inode_num = ((long long)node - (long long)inode_base) / sizeof(inode);

    // Loop through path until found
    for (int path_iter = 0; path_iter < dirs->size; path_iter++)
    {
        // Get next directory/filename
        char* name = vector_get(dirs, path_iter);

        // We are in a directory
        if (it->isdir)
        {
            // Find block containing directory map
            map* dirmap = get_block_num(it->block);

            // Get inode number for next directory/filename
            int dnode_num = map_get(dirmap, name);

            // Last piece of path
            if (path_iter == dirs->size - 1)
            {
                // Check for File Exists
                if (dnode_num != -1)
                {
                    // Clean Up
                    delete_vector(dirs);

                    return -EEXIST;
                }

	        map_add(dirmap, name, inode_num);
                node->refs++;
                
                // Clean Up
                delete_vector(dirs);

                return 0;
            }

            // Check for directory Exists
            if (inode_num == -1)
            {
                // Clean Up
                delete_vector(dirs);

                return -ENOENT;
            }

            // Move to next directory
            it = get_inode_num(inode_num);
        }

    }

    // Clean Up
    delete_vector(dirs);

    // Return where we got out of iteration loop
    return 0;
}

// Get inode info for given path
int
get_stat(inode* inode, struct stat* st)
{
    // Clear stat structure
    memset(st, 0, sizeof(struct stat));

    // Assign values
    st->st_mode    = inode->mode;
    st->st_nlink   = inode->refs;
    st->st_uid     = inode->uid;
    st->st_gid     = inode->gid;
    st->st_size    = inode->size;
    st->st_blocks  = inode->blocks;
    st->st_blksize = BLOCK_SIZE;
    st->st_mtim.tv_sec = inode->mtime;

    // Success
    return 0;
}

// Get data from given inode
void*
get_data(inode* inode)
{
    // Allocate some data
    void* data = malloc(inode->size);

    // Only has direct block
    if (inode->size <= BLOCK_SIZE)
    {
        memcpy(data, get_block_num(inode->block), inode->size);
        return data;
    }

    // Prepare to iterate indirect block
    void* data_iter = data;
    int size_remaining = inode->size;

    // Get data from direct block first
    memcpy(data_iter, get_block_num(inode->block), BLOCK_SIZE);
    size_remaining -= BLOCK_SIZE;
    data_iter += BLOCK_SIZE;

    // Loop through indirect blocks
    int* block_nums = get_block_num(inode->indirect);
    for (int i = 0; i < INDIRECT_COUNT; i++)
    {
        // Get data block
        void* block = get_block_num(block_nums[i]);

        // Final Block
        if (size_remaining <= BLOCK_SIZE)
        {
            memcpy(data_iter, block, size_remaining);
            return data;
        }

        // Copy whole block
        memcpy(data_iter, block, BLOCK_SIZE);
        size_remaining -= BLOCK_SIZE;
        data_iter += BLOCK_SIZE;
    }

    assert(0);
}

// Write data into given inode
int
write_data(inode* inode, const void* buf, size_t size, off_t offset)
{
    // Set Size accordingly
    if (inode->size < size + offset)
    {
        inode->size = size + offset;
    }

    // Data will fit in direct block
    if (size + offset <= BLOCK_SIZE)
    {
        memcpy(get_block_num(inode->block) + offset, buf, size);
        char* data = get_data(inode); 
        return size;
    }

    // Prepare to iterate indirect block
    if (inode->indirect == -1)
    {
        inode->indirect = allocate_block();
        if (inode->indirect == -1)
        {
            return ENOSPC;
        }
    }
    const void* data_iter = buf;
    int size_remaining = size;

    // Write data to direct block first
    if (offset < BLOCK_SIZE)
    {
        memcpy(get_block_num(inode->block) + offset, data_iter, BLOCK_SIZE - offset);
        size_remaining -= BLOCK_SIZE - offset;
        data_iter += BLOCK_SIZE - offset;
        offset = 0;
    }
    else
    {
        offset -= BLOCK_SIZE;
    }

    // Loop through indirect blocks
    int* block_nums = get_block_num(inode->indirect);
    for (int i = 0; i < INDIRECT_COUNT; i++)
    {
        // Allocate block if necessary
        if (block_nums[i] == 0)
        {
            block_nums[i] = allocate_block();
            if (block_nums[i] == -1)
            {
                return -ENOSPC;
            }
            inode->blocks++;
        }

        // Get data block
        void* block = get_block_num(block_nums[i]);

        // Final Block
        if (size_remaining + offset <= BLOCK_SIZE)
        {
            memcpy(block + offset, data_iter, size_remaining);
            char* data = get_data(inode); 
            return size;
        }

        // Copy whole block
        if (offset < BLOCK_SIZE)
        {
            memcpy(block + offset, data_iter, BLOCK_SIZE - offset);
            size_remaining -= BLOCK_SIZE - offset;
            data_iter += BLOCK_SIZE - offset;
            offset = 0;
        }
        else
        {
            offset -= BLOCK_SIZE;
        }
    }

    return -EFBIG;
}
