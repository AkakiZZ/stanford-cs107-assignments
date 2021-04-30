#include "vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <search.h>

#define DEFAULT_ALLOC_LEN 4

void VectorNew(vector *v, int elemSize, VectorFreeFunction freeFn, int initialAllocation) 
{
    assert(elemSize > 0);
    assert(initialAllocation >= 0);
    v->elem_size = elemSize;
    v->alloc_len = (initialAllocation == 0) ? DEFAULT_ALLOC_LEN : initialAllocation;;
    v->log_len = 0;
    v->initial_allocation = initialAllocation;
    v->elems = malloc(v->alloc_len * elemSize);
    v->free_fn = freeFn;
    assert(v->elems != NULL);
}

void VectorDispose(vector *v)
{
    if (v->free_fn != NULL) {
        for (int i = 0; i < v->log_len; i++) {
            void *tmp = (char *)v->elems + i * v->elem_size;
            v->free_fn(tmp);
        }
    }
  free(v->elems);
}

int VectorLength(const vector *v) 
{ 
    return v->log_len; 
}

void *VectorNth(const vector *v, int position) 
{ 
    assert(position >= 0 && position < v->log_len);
    return (void*)((char*)v->elems + position * v->elem_size); 
}

void VectorReplace(vector *v, const void *elemAddr, int position)
{
    assert(position >= 0 && position < v->log_len);
    void* dest = (char*)v->elems + position * v->elem_size;
    if (v->free_fn != NULL) v->free_fn(dest);
    memcpy(dest, elemAddr, v->elem_size);
}

void VectorInsert(vector *v, const void *elemAddr, int position)
{
    assert(position >= 0 && position <= v->log_len);
    if (v->log_len == v->alloc_len) {
        v->alloc_len *= 2;
        v->elems = realloc(v->elems, v->alloc_len * v->elem_size);
        assert(v->elems != NULL);
    } 
    void* from = (char*)v->elems + position * v->elem_size;
    void* to = (char*)v->elems + (position + 1) * v->elem_size;
    memmove(to, from, (v->log_len - position) * v->elem_size);
    memcpy(from, elemAddr, v->elem_size);
    v->log_len++;
}

void VectorAppend(vector *v, const void *elemAddr)
{
    if (v->log_len == v->alloc_len) {
        v->alloc_len *= 2;
        v->elems = realloc(v->elems, v->alloc_len * v->elem_size);
        assert(v->elems != NULL);
    } 
    void* dest = (char*)v->elems + v->log_len * v->elem_size;
    memcpy(dest, elemAddr, v->elem_size);
    v->log_len++;
}

void VectorDelete(vector *v, int position)
{
    assert(position >= 0 && position < v->log_len);
    void* to = (char*)v->elems + position * v->elem_size;
    void* from = (char*)v->elems + (position + 1) * v->elem_size;
    if (v->free_fn != NULL) v->free_fn(to);
    memmove(to, from, (v->log_len - position - 1) * v->elem_size);
    v->log_len--;
}

void VectorSort(vector *v, VectorCompareFunction compare)
{
    assert(compare != NULL);
    qsort(v->elems, v->log_len, v->elem_size, compare);
}

void VectorMap(vector *v, VectorMapFunction mapFn, void *auxData)
{
    assert(mapFn != NULL);
    for (int i = 0; i < v->log_len; i++) {
        mapFn((char*)v->elems + v->elem_size * i, auxData);
    }
}

static const int kNotFound = -1;
int VectorSearch(const vector *v, const void *key, VectorCompareFunction searchFn, int startIndex, bool isSorted)
{ 
    assert(searchFn != NULL);
    assert(startIndex >= 0 && startIndex <= v->log_len);
    void* base = (char*)v->elems + startIndex * v->elem_size;
    void* res;
    if(isSorted) {
        res = bsearch(key, base, v->log_len - startIndex, v->elem_size, searchFn);
    } else {
        size_t size = v->log_len - startIndex;
        res = lfind(key, base, &size, v->elem_size, searchFn);
    }
    return res == NULL ? kNotFound : (int)((char*)res - (char*)v->elems) / v->elem_size; 
} 
