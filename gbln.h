#ifndef _GBLN_H_
#define _GBLN_H_
	
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
enum GBLN_type{
	GBLN_INT_T,
	GBLN_FLOAT_T,
	GBLN_CHAR_T,
	GBLN_STRUCT_T,
	GBLN_ARRAY_T
};
typedef int32_t GBLN_INT;
typedef float GBLN_FLOAT;
typedef int8_t GBLN_CHAR;


struct GBLN_var;
struct GBLN_STRUCT;
struct GBLN_ARRAY;

typedef struct{
	char* name;
	void* extra;
	uint8_t type;

} GBLN_STRUCT_FIELD;

typedef struct{
	uint32_t count; //number of elements
	GBLN_STRUCT_FIELD* vars;
	char* name;
}GBLN_STRUCT_DEF;



typedef struct GBLN_var{
	uint8_t type;
	union{
		GBLN_INT int_val;
		GBLN_FLOAT float_val;
		GBLN_CHAR char_val;
		struct GBLN_STRUCT* struct_val;
		struct GBLN_ARRAY* array_val;
	};
} GBLN_var;


typedef struct GBLN_STRUCT{
	GBLN_STRUCT_DEF* def;
	GBLN_var data[1];
}GBLN_STRUCT;

typedef struct GBLN_ARRAY{
	uint8_t type;
	void* extra;
	GBLN_INT len;
	char data[1];

} GBLN_ARRAY;

enum GBLN_FUNC_FLAGS{
	GF_NONE = 0,
	GF_RESOLVED = 1,
	GF_EXTERNAL = 1 << 1
};

struct GBLN_vm;

typedef void(*GBLN_nativefunc)(struct GBLN_vm*);

typedef struct{
	size_t entry;
	union{
		size_t memaddr;
		GBLN_nativefunc native_function;
	};
	char* name;
	uint8_t flags;
	uint16_t locals;
}GBLN_func;	

typedef struct {
	char* name;
	GBLN_var data;
} GBLN_data;

typedef struct {
	uint8_t* code;
	GBLN_func* func_table;
	GBLN_data* data;
	size_t codelen;
	size_t funcslen;
	size_t datalen;
	size_t entryfunc;
} GBLN_object;



GBLN_STRUCT* AllocGblnStr(const char* cstr);
void FreeGblnStr(GBLN_STRUCT* gstr);

void GblnCompilerStart();

int GblnCompilerExtract(GBLN_object* gobject);


int OutputGblno(const char* filename, GBLN_object* gobject);


int GblnCompileGasm(const char* filename);

char* GBLN_op_name(int opcode);

#endif
