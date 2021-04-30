#include "hashset.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

void HashSetNew(hashset *h, int elemSize, int numBuckets,
		HashSetHashFunction hashfn, HashSetCompareFunction comparefn, HashSetFreeFunction freefn)
{
	assert(elemSize > 0);
	assert(numBuckets > 0);
	assert(hashfn != NULL);
	assert(comparefn != NULL);
	h->elem_size = elemSize;
	h->num_buckets = numBuckets;
	h->num_elems = 0;
	h->freefn = freefn;
	h->comparefn = comparefn;
	h->hashfn = hashfn;
	h->buckets = (vector*)malloc(numBuckets * sizeof(vector));
	assert(h->buckets != NULL);
	for (int i = 0; i < numBuckets; i++) {
		VectorNew(h->buckets + i, elemSize, freefn, 0);
	}
}

void HashSetDispose(hashset *h)
{
	if (h->freefn != NULL) {
		for (int i = 0; i < h->num_buckets; i++) {
			VectorDispose(h->buckets + i);
		}
	}
	free(h->buckets);
}

int HashSetCount(const hashset *h)
{ return h->num_elems; }

void HashSetMap(hashset *h, HashSetMapFunction mapfn, void *auxData)
{
	assert(mapfn != NULL);
	for (int i = 0; i < h->num_buckets; i++) {
		VectorMap(h->buckets + i, mapfn, auxData);
	}
}

void HashSetEnter(hashset *h, const void *elemAddr)
{
	assert(elemAddr != NULL);
	int bucket_index = h->hashfn(elemAddr, h->num_buckets);
	assert(bucket_index >= 0 && bucket_index < h->num_buckets);
	
	vector* v = h->buckets + bucket_index;
	int index = VectorSearch(v, elemAddr, h->comparefn, 0, false);
	

	if (index == -1) {
		VectorAppend(v, elemAddr);
		h->num_elems++;
	} else {
		VectorReplace(v, elemAddr, index);
	}
}

void *HashSetLookup(const hashset *h, const void *elemAddr)
{ 
	assert(elemAddr != NULL);
	int bucket_index = h->hashfn(elemAddr, h->num_buckets);
	assert(bucket_index >= 0 && bucket_index < h->num_buckets);

	vector* v = h->buckets + bucket_index;
	int index = VectorSearch(v, elemAddr, h->comparefn, 0, false);
	return index == -1 ? NULL : VectorNth(v, index);
}
