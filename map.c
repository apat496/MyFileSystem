#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "map.h"

const int NAME_SIZE_LIMIT = 89;
const int MAP_ENTRY_LIMIT = 44;

int
map_get(map* m, char* key)
{
    for (int i = 0; i < m->size; i++)
    {
        entry e = m->entries[i];
        if (strcmp(e.name, key) == 0)
        {
            return e.inode_num;
        }
    }
    return -1;
}

void
map_add(map* m, char* name, int num)
{
    assert(m->size < MAP_ENTRY_LIMIT);
    strncpy(m->entries[m->size].name, name, NAME_SIZE_LIMIT);
    m->entries[m->size].inode_num = num;
    m->size++;
}

void
map_remove(map* m, char* key)
{
    int found = 0;

    for (int i = 0; i < m->size; i++)
    {
        entry e = m->entries[i];
        if (strcmp(key, e.name) == 0)
        {
            found = 1;
            m->size--;
        }

        if (found)
        {
            m->entries[i] = m->entries[i + 1];
        }
    }
}

void
map_print(map* m)
{
    printf("PRINTING MAP\n");
    int maxlen = 0;
    for (int i = 0; i < m->size; i++)
    {
        entry e = m->entries[i];
        if (strlen(e.name) > maxlen)
        {
            maxlen = strlen(e.name);
        }
    }

    for (int i = 0; i < m->size; i++)
    {
        entry e = m->entries[i];
        printf("%-*s %d\n", maxlen, e.name, e.inode_num); 
    }
}

