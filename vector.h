#ifndef VECTOR_H
#define VECTOR_H

typedef struct vector {
    int size;
    int capacity;
    void** data;
} vector;

vector* new_vector();

void delete_vector(vector* v);

void* vector_get(vector* v, int index);

void vector_add(vector* v, void* item);

void vector_print(vector* v);

vector* str_split(const char* in, char del);

#endif
