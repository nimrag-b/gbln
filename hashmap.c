#include "hashmap.h"
#include "gbln.h"
#include <stdlib.h>
#include <string.h>

static unsigned long
hash(const char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = (*str++)))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

static void FreeNode(GBLN_STRUCT_hashmap_node* node){
	if(node->next){
		FreeNode(node->next);
	}
	free(node);
}

GBLN_STRUCT_hashmap* AllocGBLN_STRUCT_hashmap(){
	GBLN_STRUCT_hashmap* map = malloc(sizeof(GBLN_STRUCT_hashmap));
	for (int i = 0; i < GBLN_BUCKET_COUNT; i++) {
		map->nodes[i].next = NULL;
	}
	return map;
}

void FreeGBLN_STRUCT_hashmap(GBLN_STRUCT_hashmap *map){
	
	for (int i = 0; i < GBLN_BUCKET_COUNT; i++) {
		FreeNode(&map->nodes[i]);
	}
	free(map);
}

static GBLN_STRUCT_hashmap_node* AllocNode(){
	GBLN_STRUCT_hashmap_node* node = malloc(sizeof(GBLN_STRUCT_hashmap_node));
	node->next = NULL;
	return node;
}
	
int AddGBLN_STRUCT_hashmap(GBLN_STRUCT_hashmap* map, const char *key, struct GBLN_STRUCT *value){

	unsigned long hashval = hash(key);
	GBLN_STRUCT_hashmap_node* node = &map->nodes[hashval % GBLN_BUCKET_COUNT];
	while(node->next != NULL){
		if(strcmp(key, node->key) == 0){
			return 0;
		}
		node = node->next;
	}
	node->next = AllocNode();
	node->key = key;
	node->value = value;
	return 1;
}


struct GBLN_STRUCT* GetGBLN_STRUCT_hashmap(GBLN_STRUCT_hashmap* map, const char* key){
	unsigned long hashval = hash(key);
	GBLN_STRUCT_hashmap_node* node = &map->nodes[hashval % GBLN_BUCKET_COUNT];
	while(node != NULL){
		if(strcmp(key, node->key) == 0){
			return node->value;
		}
	}
	return NULL;

}
