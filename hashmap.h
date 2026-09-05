#ifndef _GBLN_HASHMAP_H_
#define _GBLN_HASHMAP_H_

struct GBLN_STRUCT;

#define GBLN_BUCKET_COUNT 16

typedef struct GBLN_STRUCT_hashmap_node{
	const char* key;
	struct GBLN_STRUCT* value;
	struct GBLN_STRUCT_hashmap_node* next;
} GBLN_STRUCT_hashmap_node;

typedef struct{
	GBLN_STRUCT_hashmap_node nodes[GBLN_BUCKET_COUNT];
} GBLN_STRUCT_hashmap;

GBLN_STRUCT_hashmap* AllocGBLN_STRUCT_hashmap();
void FreeGBLN_STRUCT_hashmap(GBLN_STRUCT_hashmap* map);

struct GBLN_STRUCT* GetGBLN_STRUCT_hashmap(GBLN_STRUCT_hashmap* map, const char* key);

int AddGBLN_STRUCT_hashmap(GBLN_STRUCT_hashmap* map, const char* key, struct GBLN_STRUCT* value);
#endif

