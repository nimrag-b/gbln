#include "gbln.h"
#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct{
	char bytes;
	char data[8];
} GBLN_compilearg;

typedef struct {
	char* name;
	size_t addr;
	size_t* refs;
	size_t ref_count;
	size_t rep_cap;
} GBLN_label;




#define SET_ERR(fmt, ...) {\
	char _errfmt_[] = fmt; \
	int _errlen_ = snprintf(NULL,0,_errfmt_,__VA_ARGS__) + 1; \
	gbln_err = malloc(_errlen_); \
	snprintf(gbln_err,_errlen_,_errfmt_,__VA_ARGS__);\
}

#define TEXT_SECTION 1
#define DATA_SECTION 2

static int cur_section;

static char* curfile;
static int curline;

static char* gbln_err = NULL;

static unsigned char* mem;
static size_t len;
static size_t cap;

static GBLN_func* cur_sym = NULL;
static GBLN_func* symbols;
static size_t symbol_len;
static size_t symbol_cap;

static size_t entryfunc = 0;

static GBLN_label* labels;
static size_t label_len;
static size_t label_cap;

static GBLN_label* data_labels;
static size_t data_label_len;
static size_t data_label_cap;

static GBLN_data* datas;
static size_t data_len;
static size_t data_cap;


int isfuncname(char ch){
	return (isalnum(ch) || (ch=='_'));
}

static void resize_buf(size_t bytes){
	while((len + bytes) >= cap){
		cap *= 2;
	}
	mem = realloc(mem, cap);
}

static void eat_empty(char* line){
	while(*line != '\n' && *line != 0){
		if(!isspace(*line)){
			SET_ERR("unexpected token (%d)'%c'",*line, *line);
		}
		line++;
	}
}

static void out_bin(uint8_t op, int argc, GBLN_compilearg* argv){

	resize_buf(1 + (8*argc)); //ensure that always have enough space

	mem[len++] = op;
	//printf("%d\n",op);

	for (int i = 0; i < argc; i++) {
		memcpy(mem+len,argv[i].data,argv[i].bytes);
		len += argv[i].bytes;
	}	
}



#define NAME_MAX 12
struct Op{
	char opcode;
	char name[NAME_MAX];
	char args;
	void (*special)(uint8_t opc, char* args);
};

GBLN_label* get_label(const char* name){


	//already exista
	for(int i = 0; i < label_len; i++){
		if(strcmp(name, labels[i].name) == 0){
			return &labels[i];
		}
	}

	//new label
	if(label_len >= label_cap){
		label_cap *= 2;
		labels = realloc(labels, sizeof(GBLN_label)*label_cap);
	}
	GBLN_label* label = &labels[label_len++];

	
	label->name = malloc(strlen(name) + 1);
	strcpy((char*)label->name, name);

	label->ref_count = 0;
	label->rep_cap = 2;
	label->refs = malloc(sizeof(size_t) * label->rep_cap);
	label->addr = -1;

	return label;
}

void resolve_and_reset_labels(){

	for(int i = 0; i < label_len; i++){
		GBLN_label* label = &labels[i];
		for(int j = 0; j < label->ref_count; j++){
			if(label->addr == -1){
				SET_ERR("unresolved label '%s'",label->name);
				return;
			}
			uint32_t addr = label->addr;
			memcpy(mem + label->refs[j],&addr,sizeof(uint32_t));
		}
		free(label->refs);
		free(label->name);

	}
	free(labels);
	label_len = 0;
	label_cap = 2;
	labels = malloc(sizeof(GBLN_label)* label_cap);
}

void define_label(const char* name, size_t pos){
	
	GBLN_label* label = get_label(name);

	label->addr = pos;
}

void ref_label(const char* name, size_t dst){

	GBLN_label* label = get_label(name);

	if(label->ref_count >= labels->rep_cap){
		label->rep_cap *= 2;
		label->refs = realloc(label->refs, sizeof(size_t) * label->rep_cap);
				
	}

	label->refs[label->ref_count++] = dst;

}


GBLN_label* get_data_label(const char* name){


	//already exista
	for(int i = 0; i < data_label_len; i++){
		if(strcmp(name, data_labels[i].name) == 0){
			return &data_labels[i];
		}
	}

	//new label
	if(data_label_len >= data_label_cap){
		data_label_cap *= 2;
		data_labels = realloc(data_labels, sizeof(GBLN_label)*data_label_cap);
	}
	GBLN_label* label = &data_labels[data_label_len++];

	
	label->name = malloc(strlen(name) + 1);
	strcpy((char*)label->name, name);

	label->ref_count = 0;
	label->rep_cap = 2;
	label->refs = malloc(sizeof(size_t) * label->rep_cap);
	label->addr = -1;

	return label;
}

void resolve_data_labels(){

	for(int i = 0; i < data_label_len; i++){
		GBLN_label* label = &data_labels[i];
		for(int j = 0; j < label->ref_count; j++){
			if(label->addr == -1){
				SET_ERR("unresolved label '%s'",label->name);
				return;
			}
			uint32_t addr = label->addr;
			memcpy(mem + label->refs[j],&addr,sizeof(uint32_t));
		}
		free(label->refs);
		free(label->name);

	}
	free(labels);
}

void define_data_label(const char* name, size_t pos){
	
	GBLN_label* label = get_data_label(name);

	label->addr = pos;
}

void ref_data_label(const char* name, size_t dst){

	GBLN_label* label = get_data_label(name);

	if(label->ref_count >= labels->rep_cap){
		label->rep_cap *= 2;
		label->refs = realloc(label->refs, sizeof(size_t) * label->rep_cap);
				
	}

	label->refs[label->ref_count++] = dst;

}

GBLN_func* next_symbol(const char* name){

	if(symbol_len >= symbol_cap){
		symbol_cap *= 2;
		symbols = realloc(symbols, sizeof(GBLN_func)*symbol_cap);
	}
	GBLN_func* sym = &symbols[symbol_len++];

	size_t symlen = strlen(name)+1;

	sym->name = malloc(symlen);
	strcpy(sym->name,name);
	sym->flags = 0;
	sym->memaddr = 0;
	sym->entry = symbol_len - 1;
	cur_sym = sym;
	return sym;
}

size_t get_symbol(char* name){
	for(size_t i = 0; i < symbol_len; i++){
		if(strcmp(name,symbols[i].name) == 0){
		
			return i;
		}
	}

	next_symbol(name);

	return symbol_len - 1;

}

void add_function(char* name, size_t offset){
	GBLN_func* sym;
	for(int i = 0; i < symbol_len; i++){
		if(strcmp(name,symbols[i].name) == 0){
			sym = symbols + i;
			goto set;
		}
	}
	sym = next_symbol(name);
set:
	sym->memaddr = offset;
	sym->flags |= GF_RESOLVED;


}


void push_to_bin(uint8_t opc, char* args){
	while(isspace(*args)){args++;}
	int i = 0;
	char* type = args;
	while(isalpha(type[i])){i++;}
	args += i;
	while(isspace(*args)){args++;}
	GBLN_compilearg arg;
	if(strncmp(type, "byte",i) == 0){
		arg.bytes = 1;
		int dat = atoi(args);
		while(isdigit(*args)) args++;
		if(dat > 0XFF){
			//error
		}
		memcpy(arg.data,&dat,1);
		out_bin(10, 1, &arg);

		eat_empty(args);
	}
	else if(strncmp(type, "char",i) == 0){
		arg.bytes = 1;
		char ch;
		args++; //skip first '
		if(*args == '\\'){
			args++;
			switch (*args) {
				case 'n': ch = '\n'; break;
				default: 
					  SET_ERR("invalid escape character '\%c'",*args);
				return;
			}
		} else{
			ch = *args;
		}
		args += 2; //skip closing '
		memcpy(arg.data,&ch,1);
		out_bin(10, 1, &arg);

		eat_empty(args);
	}
	else if(strncmp(type, "short",i) == 0){
		arg.bytes = 2;
		int dat = atoi(args);
		while(isdigit(*args)) args++;
		if(dat > 0xFFFF){
			//error
		}
		memcpy(arg.data,&dat,2);
		out_bin(12, 1, &arg);
		eat_empty(args);
	}
	else if(strncmp(type, "int",i) == 0){
		arg.bytes = 4;
		int dat = atoi(args);
		while(isdigit(*args)) args++;

		memcpy(arg.data,&dat,4);
		out_bin(14, 1, &arg);
		eat_empty(args);
	}
	else if(strncmp(type, "long",i) == 0){
		arg.bytes = 8;
		int dat = atoll(args);
		while(isdigit(*args)) args++;
		memcpy(arg.data,&dat,8);
		out_bin(16, 1, &arg);
		eat_empty(args);
	}
	else{
		SET_ERR("invalid type '%s'",type);
	}
}

void pop_to_bin(uint8_t opc, char* args){
	while(isspace(*args)){args++;}
	int i = 0;
	char* type = args;
	while(isalpha(type[i])){i++;}
	if(strncmp(type, "byte",i) == 0){
		out_bin(11, 0, NULL);
		eat_empty(args + i);
	}
	else if(strncmp(type, "short",i) == 0){
		
		out_bin(13, 0, NULL);
		eat_empty(args + i);
	}
	else if(strncmp(type, "int",i) == 0){
		
		out_bin(15, 0, NULL);
		eat_empty(args + i);
	}
	else if(strncmp(type, "long",i) == 0){
		
		out_bin(17, 0, NULL);
		eat_empty(args + i);
	}
	else{
		SET_ERR("invalid type '%s'",type);

	}

}
void call_to_bin(uint8_t opc, char* args){

	while(isspace(*args)){args++;}
	GBLN_compilearg arg;
	if(isdigit(*args)){
		int dat = atoi(args);
		while(isdigit(*args)) args++;
		memcpy(arg.data,&dat,sizeof(dat));
		arg.bytes = 4;
		out_bin(18, 1, &arg);
	}
	else if(*args == '"'){
		args++;
		char* sym = args;
		while(*args != '"'){
			args++;
		}
		*args = 0;
		
		int dat = get_symbol(sym);
		memcpy(arg.data,&dat,sizeof(dat));
		arg.bytes = 4;
		out_bin(18, 1, &arg);
		eat_empty(args);

	}
	else SET_ERR("invalid target '%s'",args);
	//error
}
void jmp_to_bin(uint8_t opc, char* args){

	while(isspace(*args)){args++;}
	GBLN_compilearg arg;
	if(isdigit(*args)){
		int dat = atoi(args);
		while(isdigit(*args)) args++;
		memcpy(arg.data,&dat,sizeof(dat));
		arg.bytes = 4;
		out_bin(opc, 1, &arg);
		eat_empty(args);
	}
	else if(isfuncname(*args)){
		char* label = args;
		while(isfuncname(*args)){args++;}
		*args = 0;
		
		arg.bytes = 4;
		out_bin(opc, 1, &arg);

		ref_label(label, len-4);
		eat_empty(args);
	}
	else SET_ERR("invalid target '%s'",args);
	//error
}



void imm16_to_bin(uint8_t opc, char* args){
	while(isspace(*args)){args++;}
	GBLN_compilearg arg;
	if(isdigit(*args)){
		int dat = atoi(args);
		if(dat < 0 || dat >= 0xFFFF){
			SET_ERR("data index '%d' out of range",dat);
			return;
		}
		while(isdigit(*args)) args++;
		uint16_t offset = (uint16_t)dat;
		memcpy(arg.data,&offset,sizeof(offset));
		arg.bytes = 2;
		out_bin(opc, 1, &arg);
		eat_empty(args);
	}	
	else SET_ERR("invalid target '%s'",args);
	//error
}

void imm32_to_bin(uint8_t opc, char* args){
	while(isspace(*args)){args++;}
	GBLN_compilearg arg;
	if(isdigit(*args)){
		int dat = atoi(args);
		
		while(isdigit(*args)) args++;
		
		memcpy(arg.data,&dat,sizeof(dat));
		arg.bytes = 4;
		out_bin(opc, 1, &arg);
		eat_empty(args);
	}	
	else SET_ERR("invalid target '%s'",args);
	//error

}

void lddata_to_bin(uint8_t opc, char* args){

	while(isspace(*args)){args++;}
	GBLN_compilearg arg;
	if(isdigit(*args)){
		int dat = atoi(args);
		while(isdigit(*args)) args++;
		memcpy(arg.data,&dat,sizeof(dat));
		arg.bytes = 4;
		out_bin(opc, 1, &arg);
		eat_empty(args);
	}
	else if(isfuncname(*args)){
		char* label = args;
		while(isfuncname(*args)){args++;}
		*args = 0;
		
		arg.bytes = 4;
		out_bin(opc, 1, &arg);

		ref_data_label(label, len-4);
		eat_empty(args);
	}
	else SET_ERR("invalid target '%s'",args);
	//error
}



struct Op ops[] = {
	{0,"nop",0,NULL},
	{1,"iadd",0,NULL},
	{2,"fadd",0,NULL},
	{3,"isub",0,NULL},
	{4,"fsub",0,NULL},
	{5,"imul",0,NULL},
	{6,"fmul",0,NULL},
	{7,"idiv",0,NULL},
	{8,"fdiv",0,NULL},
	{9,"imod",0,NULL},
	{10,"push",1,&push_to_bin},
	{11,"pop",1,&pop_to_bin},
	{18,"call",1,&call_to_bin},
	{19,"jmp",1,&jmp_to_bin},
	{20,"return",0,NULL},
	{21,"idup",0,NULL},
	{22,"ifz",1,&jmp_to_bin},
	{23,"ifnz",1,&jmp_to_bin},
	{24,"ifg",1,&jmp_to_bin},
	{25,"ifl",1,&jmp_to_bin},
	{26,"ifge",1,&jmp_to_bin},
	{27,"ifle",1,&jmp_to_bin},
	{28,"lddata",1,&lddata_to_bin},
	{29,"ldfield",1,&imm16_to_bin},
	{30,"strfield",1,&imm16_to_bin},
	{31,"ldarr",0,NULL},
	{32,"strarr",0,NULL},
	{33,"load",1,&imm16_to_bin},
	{34,"store",1,&imm16_to_bin},
	{35,"istore",1,&imm16_to_bin},
};
#define OPCOUNT (sizeof(ops)/sizeof(ops[0]))

char* GBLN_op_name(int opcode){
	for(int i = 0; i < OPCOUNT; i++){
		if(ops[i].opcode == opcode){
			return ops[i].name;
		}
	}
	static char* unknown = "unknown";
	return unknown;
}

struct OpMap{
	struct Op* cur;
	struct OpMap* children['z'-'a'  + 1];
};
struct OpMap root = {0};
struct OpMap opmaps[512] = {0};
int opmap_filled = 0;

void FillOpMap(){
	int opc = 0;
	for(int i = 0; i < OPCOUNT; i++){
		char* name = ops[i].name;
		struct OpMap* cur = &root;
		while(*name){
			int off = *name - 'a';
			if(!cur->children[off]){
				cur->children[off] = &opmaps[opc++];
			}
			cur = cur->children[off];
			name++;
		}
		cur->cur = &ops[i];
	}
	opmap_filled = 1;
}

static void incompatable_flags_err(const char* flaga, const char* flagb){
	SET_ERR("flag '%s' is incompatable with flag '%s'",flaga,flagb);
}

static GBLN_data* next_data(char* name){
	if(data_len >= data_cap){
		data_cap *= 2;
		datas = realloc(datas, sizeof(GBLN_data) * data_cap);
	}
	GBLN_data* data = &datas[data_len++];
	data->name = malloc(strlen(name)+1);
	strcpy(data->name,name);
	return data;
}

static int asm_to_bin(char* data){
	if(data[0] == '\n'){
		curline++;
		return 0;
	}
	char* rawline = strtok(data, "\n");
	
	while(rawline){

		int linelength = strlen(rawline);
		static char line[512];
		if(linelength >= 511){
			SET_ERR("line too long",0);
			goto next;
		}

		strcpy(line,rawline);
		line[strlen(line)] = '\n';

		char* ptr = line;
		char ch = *ptr;
		char* op = line;



		while(ch && !isspace(ch)){
			ch = *++ptr;
		}
		*ptr = 0;
		ptr++;
	
		if(line[0] == '.'){ //directive
			op++;
			//printf("%s\n",op);
			
			if(strcmp(op,"section") == 0){

				while(isspace(*ptr)){ptr++;}
				char* section_name = ptr;
				while(isfuncname(*ptr)) ptr++;
				*ptr = 0;

				if(strcmp(section_name,"text") == 0){
					cur_section = TEXT_SECTION;
				}
				else if(strcmp(section_name,"data") == 0){
					cur_section = DATA_SECTION;
				}

				goto next;

			}

			if(cur_section == DATA_SECTION){
				if(strcmp(op, "string") == 0){

					while(isspace(*ptr)){ptr++;}
					char* data_name = ptr;
					while(isfuncname(*ptr)){ptr++;}
					*ptr = 0;
					ptr++;
					int scrcap = 128;
					int scrlen = 0;
					char* scratchbuf = malloc(scrcap);
strdirstart:
					while(isspace(*ptr)){ptr++;}
					

					switch (*ptr) {
					case '"':
					if(*ptr != '"'){
						SET_ERR("expected '%c' found '%c'",'"',*ptr);
						goto next;
					}
					ptr++;
					while(*ptr != '"'){
						if(scrlen == scrcap){

							scrcap*=2;
							scratchbuf = realloc(scratchbuf,scrcap);
						}
						scratchbuf[scrlen++] = *ptr;
						if(*ptr == 0){
							SET_ERR("unclosed string '%s'",scratchbuf);
							goto next;
						}
						ptr++;
					}
					ptr++;
					break;

					default:
					if(isdigit(*ptr)){
						int l = atoi(ptr);
						if (l > 0xFF){
							SET_ERR("char too big '%d'",l);
						}
						char cl = l;
						if(scrlen == scrcap){

							scrcap*=2;
							scratchbuf = realloc(scratchbuf,scrcap);
						}
						scratchbuf[scrlen++] = cl;
					}
					break;
					}

					while(isspace(*ptr)){ptr++;}
					if(*ptr == ','){
						ptr++;
						goto strdirstart;
					}

					scratchbuf[scrlen] = 0;
				

					GBLN_data* data = next_data(data_name);
					data->data.type = GBLN_STRUCT_T;
					data->data.struct_val = AllocGblnStr(scratchbuf);
					define_data_label(data->name, data_len - 1);
					free(scratchbuf);
				}
				else{
					SET_ERR("invalid data directive '%s'",rawline);
				}
				goto next;
			}
			
			if(strcmp(op, "function") == 0){
				if(cur_sym){
					SET_ERR("cannot declare a function within a function",0);
					goto next;
				}
				while(isspace(*ptr)){ptr++;}
				char* func_name = ptr;
				while(isfuncname(*ptr)) ptr++;
				char* closing = ptr;
				while(isspace(*closing)){closing++;}
				if(*closing != '{'){
					SET_ERR("function directive must end with '{'",0);
					goto next;
				}
				*ptr = 0;
				ptr++;
				closing++;
				//printf("    %s\n",func_name);
				add_function(func_name, len);
				eat_empty(closing);

			}
			else{
				if(cur_sym == NULL){
					SET_ERR("'%s' must be inside function block",op);
					goto next;
				}

				if(strcmp(op,"internal") == 0){
					if(cur_sym->flags & GF_EXTERNAL){
						incompatable_flags_err("internal","entrypoint");
					}

					cur_sym->flags |= GF_EXTERNAL;
					eat_empty(op + strlen(op));
				}
				else if(strcmp(op,"entrypoint") == 0){
					if(cur_sym->flags & GF_EXTERNAL){
						incompatable_flags_err("entrypoint","internal");
					}
					entryfunc = cur_sym->entry;		
					eat_empty(op + strlen(op));
				}
			}
			goto next; //ignore for now
		}

		if(cur_section == 0){
			SET_ERR("no section set - must be section text or section data",0);
			goto next;
		}

		if(cur_section != TEXT_SECTION){
			SET_ERR("code must be defined in a text section '%s'",rawline);
			goto next;
		}


		if(line[0] == '}'){ //end function
			if(cur_sym == NULL){
				SET_ERR("unexpected '}'",0);
				goto next;
			}

			resolve_and_reset_labels();

			cur_sym = NULL;
			goto next; 
		}


		if(line[strlen(line) - 1] == ':'){
			line[strlen(line)-1] = 0;
			define_label(line, len);
			eat_empty(line + strlen(line));
			goto next;	
		}


		struct OpMap* map = &root;
		
		while(map && *op){
			if(!isfuncname(*op)){
					
				SET_ERR("invalid opcode '%s'",line);
				goto next;
			}
			map = map->children[*op-'a'];
			op++;
		}
		if(map == NULL || map->cur == NULL){
			SET_ERR("unknown opcode '%s'",line);
			goto next;
		}
		
		if(map->cur->special){
			map->cur->special(map->cur->opcode, ptr);
		}
		else{
			out_bin(map->cur->opcode,0,NULL);
			eat_empty(op);
		}

		
next:
		if(gbln_err != NULL){
			printf("ERROR at %s:%d (%s): %s\n",curfile,curline, rawline,gbln_err);
			return -1;
		}
#ifdef GBLN_PRIBT_COMPILE
		printf("%s:%d : %s\n",curfile,curline, rawline);
#endif
		rawline = strtok(NULL, "\n");
		curline++;
	}
	return 0;
}

void GblnCompilerStart(){
	if(mem){
		free(mem);
	}
	cap = 512;
	mem = malloc(cap);
	len = 0;
	if(symbols){
		free(symbols);
	}
	if(gbln_err){
		free(gbln_err);
	}
	gbln_err = NULL;
	symbol_cap = 8;
	symbols = malloc(sizeof(GBLN_func)* symbol_cap);
	symbol_len = 0;
	if(!opmap_filled){
		FillOpMap();
	}

	if(datas){
		free(datas);
	}

	label_len = 0;
	label_cap = 2;
	labels = malloc(sizeof(GBLN_label)* label_cap);
	
	data_label_len = 0;
	data_label_cap = 2;
	data_labels = malloc(sizeof(GBLN_label)* label_cap);

	data_cap = 4;
	data_len = 0;
	datas = malloc(sizeof(GBLN_data) * data_cap);
}



int GblnCompilerExtract(GBLN_object* gobject){


	resolve_data_labels();

	gobject->code = mem;
	gobject->codelen = len;

	gobject->func_table = symbols;
	gobject->funcslen = symbol_len;

	gobject->entryfunc = entryfunc;

	gobject->data = datas;
	gobject->datalen = data_len;

	//free(labels);
	free(data_labels);



	mem = NULL;
	cap = 0;
	len = 0;
	symbols = NULL;
	symbol_cap = 0;
	symbol_len = 0;
	datas = NULL;
	data_cap = 0;
	data_len = 0;
	entryfunc = 0;

	return 0;
}
int OutputGblno(const char* filename, GBLN_object* gobject){

	puts("WARNING: OutputGblno is not fully implemented");
	FILE* fptr = fopen(filename, "wb");

	fputs("GBLNO",fptr);
	fputc(0,fptr);
	fputs(".section symbols\0", fptr);
	fwrite(&gobject->funcslen,  sizeof(gobject->funcslen),1 , fptr);

	for(int i = 0; i < gobject->funcslen; i++){
		GBLN_func* func = &gobject->func_table[i];
		fputs(func->name,fptr);
		fputc(0,fptr);
		fwrite(&func->flags,sizeof(func->flags),1,fptr);
		fwrite(&func->memaddr,sizeof(func->memaddr),1,fptr);
	}

	fputs(".section code\0", fptr);

	fwrite(&gobject->codelen,  sizeof(gobject->codelen),1 , fptr);
	fwrite(gobject->code, 1, gobject->codelen, fptr);

	fputs(".section data\0", fptr);


	fclose(fptr);
	for(int i = 0; i < gobject->datalen; i++){
		GBLN_data* dat = &gobject->data[i];
		fputc(dat->data.type,fptr);
		switch (dat->data.type) {
			case GBLN_CHAR_T: fputc(dat->data.char_val,fptr); break;
			case GBLN_INT_T: fwrite(&dat->data.int_val,1,sizeof(dat->data.int_val),fptr); break;
			case GBLN_FLOAT_T: fwrite(&dat->data.float_val,1,sizeof(dat->data.float_val),fptr); break;
			case GBLN_ARRAY_T: 
				fwrite(&dat->data.array_val->len,1,sizeof(dat->data.array_val->len),fptr);
			break;
		}

	}

	return 0;
}

int GblnCompileGasm(const char* filename){
	FILE* fptr = fopen(filename, "r");
	if(fptr == NULL){
		return -1;
	}
	curline = 1;
	curfile = (char*)filename;
	char buf[512];
	while(fgets(buf,512,fptr)){
		//puts(buf);
		if(asm_to_bin(buf) == -1){
			fclose(fptr);
			return -1;
		}
	}
	fclose(fptr);
	return 0;

}


