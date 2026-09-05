#include "gbln.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

const char* GBLN_typename(uint8_t type){
	switch (type) {
		case GBLN_CHAR_T: return "char";
		case GBLN_FLOAT_T: return "float";
		case GBLN_INT_T: return "float";
		case GBLN_STRUCT_T: return "struct";
		default: return "null";
	}
}

size_t GBLN_sizeof(GBLN_var* var){
	switch (var->type) {
		case GBLN_CHAR_T: return sizeof(GBLN_CHAR);
		case GBLN_FLOAT_T: return sizeof(GBLN_FLOAT);
		case GBLN_INT_T: return sizeof(GBLN_INT);
		case GBLN_STRUCT_T: return var->struct_val->def->count*sizeof(GBLN_var);
		default: return 0;

	}
}
GBLN_STRUCT_FIELD GBLN_string_fields[] ={
	{"length", 0,GBLN_INT_T},
	{"data", (void*)GBLN_CHAR_T,GBLN_ARRAY_T},

};
GBLN_STRUCT_DEF GBLN_string_def = {2,GBLN_string_fields,"string"};


GBLN_STRUCT* AllocGblnStruct(GBLN_STRUCT_DEF* def){
	GBLN_STRUCT* str = calloc(1,sizeof(GBLN_STRUCT)+(sizeof(GBLN_var) * def->count));
	str->def = def;	
	for (int i = 0; i < def->count; i++) {
		str->data[i].type = def->vars[i].type;

		if(def->vars[i].type == GBLN_STRUCT_T){
			str->data[i].struct_val = NULL;
		}
	}
	return str;

}

GBLN_ARRAY* AllocGblnArray(int type, void* extra, uint32_t length){
	size_t size;
	switch (type) {
		case GBLN_CHAR_T: size = sizeof(GBLN_CHAR); break;
		case GBLN_FLOAT_T: size = sizeof(GBLN_FLOAT); break;
		case GBLN_INT_T: size = sizeof(GBLN_INT); break;
		case GBLN_ARRAY_T:
		case GBLN_STRUCT_T: size = sizeof(void*); break;
		default: return NULL;

	}

	GBLN_ARRAY* arr = calloc(1, sizeof(GBLN_ARRAY) + (size*length));
	arr->type = type;
	arr->len = length;
	arr->extra = extra;

	return arr;
}

GBLN_var* GetGblnStructVar(GBLN_var* str, const char* name){
	if(str->type != GBLN_STRUCT_T) return NULL; //ERROR
	for (int i = 0; i < str->struct_val->def->count; i++) {
		if(strcmp(str->struct_val->def[i].name, name) == 0){
			return &str->struct_val->data[i];
		}
	}
	return NULL;
}

GBLN_STRUCT* AllocGblnStr(const char* cstr){

	GBLN_STRUCT* gstr = AllocGblnStruct(&GBLN_string_def);

	uint32_t len = strlen(cstr);
	gstr->data[0].type = GBLN_INT_T;
	gstr->data[0].int_val = len;

	gstr->data[1].type = GBLN_ARRAY_T;

	gstr->data[1].array_val = AllocGblnArray(GBLN_CHAR_T, NULL, len+1);

	memcpy((void*)gstr->data[1].array_val->data, cstr, len+1);
	return gstr;

}

void FreeGblnStr(GBLN_STRUCT* gstr){
	free(gstr);
}

const char* tmpStructName = NULL;
GBLN_STRUCT_FIELD* tmpStructVars = NULL;
uint32_t tmpStructCount;
uint32_t tmpStructCapacity;




void StartGblnStructDef(const char* name){
	tmpStructName = malloc(strlen(name)+1);
	strcpy((char*)tmpStructName,name);
	if(tmpStructVars){
		free(tmpStructVars);
	}
	tmpStructVars = malloc(sizeof(GBLN_STRUCT_FIELD)*2);
	tmpStructCapacity = 2;
	tmpStructCount = 0;
}

void AddStructDefField(const char* name, int type){
	if(tmpStructCapacity == tmpStructCount){
		tmpStructCapacity += 5;
		tmpStructVars = realloc(tmpStructVars,sizeof(GBLN_STRUCT_FIELD)*tmpStructCapacity);
	}
	tmpStructVars[tmpStructCount].name = malloc(strlen(name)+1);
	strcpy(tmpStructVars[tmpStructCount].name,name);
	tmpStructVars[tmpStructCount].type = type;
	tmpStructCount++;
}
void AddStructDefFieldStruct(const char* name, GBLN_STRUCT_DEF* def){
	AddStructDefField(name, GBLN_STRUCT_T);
	tmpStructVars[tmpStructCount-1].extra = def;
}
void AddStructDefFieldArray(const char* name, int type){
	AddStructDefField(name, GBLN_ARRAY_T);
	tmpStructVars[tmpStructCount-1].extra = 0;
	memcpy(&tmpStructVars[tmpStructCount-1].extra,&type,sizeof(int));
}

GBLN_STRUCT_DEF* EndGblnStructDef(){
	GBLN_STRUCT_DEF* def = malloc(sizeof(GBLN_STRUCT_DEF));
	def->name = (char*)tmpStructName;
	tmpStructName = NULL;
	def->count = tmpStructCount;
	tmpStructCount = 0;

	def->vars = tmpStructVars;
	tmpStructVars = NULL;

	tmpStructCapacity = 0;


	return def;
}

void StartVM();

int main(){

/*	
	StartGblnStructDef("ExampleStruct");

	AddStructDefField("age", GBLN_INT_T);
	AddStructDefField("name", GBLN_STRING_T);

	GBLN_STRUCT_DEF* example_struct = EndGblnStructDef();
	
	puts(example_struct->name->str);
	for (int i = 0; i < example_struct->count; i++) {
		printf("%s : %s\n",example_struct->vars[i].name->str,GBLN_typename(example_struct->vars[i].type));
	}
*/

	StartVM();

	return 0;
}
