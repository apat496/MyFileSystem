#include "vector.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

vector* new_vector()
{
    vector* v = malloc(sizeof(vector));
    v->size = 0;
    v->capacity = 4;
    v->data = calloc(4, sizeof(void*));
    return v;
}

void delete_vector(vector* v)
{
    for (int index = 0; index < v->size; index++)
    {
        if (v->data[index])
        {
            free(v->data[index]);
        }
    }

    free(v->data);
    free(v);
}

void* vector_get(vector* v, int index)
{
    assert(index > -1 && index < v->size);
    return v->data[index];
}

void vector_add(vector* v, void* item)
{
    if (v->size == v->capacity) {
        v->capacity *= 2;
        v->data = realloc(v->data, v->capacity * sizeof(void*));
    }

    v->data[v->size++] = strdup(item);
}

void vector_print(vector* v)
{
    for (int index = 0; index < v->size; index++)
    {
        printf("%s\n", (char*)v->data[index]);
    }
}

vector* str_split(const char* in, char del)
{
    char* it = strdup(in);
    char* start = it;
    vector* split = new_vector();

    if (*it == del)
    {
        start++;
        it++;
    }

    while (*it)
    {
        if (*it == del)
        {
            *it = 0;
            vector_add(split, start);
            start = it + 1;
        }
        it++;
    }

    if (*start != del)
    {
        vector_add(split, start);
    }

    return split;
}
