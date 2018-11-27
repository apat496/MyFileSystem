#ifndef MAP_H
#define MAP_H

typedef struct map_entry {
    int  inode_num;
    char name[89];
} entry;

typedef struct map {
    int size;
    entry entries[44];
} map;

int map_get(map* m, char* key);

void map_add(map* m, char* name, int num);

void map_remove(map* m, char* name);

void map_print(map* m);

#endif
