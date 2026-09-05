#include "gbln.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#define GBLN_STACK_SIZE 1000000

//#define DEBUGPRINT


#define BINARY_OP(type,bits,operand) \
	type a,b;\
	POP(vm,a);\
	POP(vm,b);\
	a = b operand a;\
	PUSH(vm,a);

#define STATIC_VARS_COUNT 4
typedef struct{
	union{
		GBLN_var* vars;
		GBLN_var static_vars[STATIC_VARS_COUNT];
	};
	uint16_t count; //if count > STATIC_VARS_COUNT then dont allocate
} GBLN_locals;

typedef struct{
	GBLN_func* func;
	GBLN_locals locals;
	size_t return_addr;
} GBLN_frame;

typedef struct GBLN_vm {
	size_t stack_size; //stack size in bytes
	size_t stack_max; //maximum size of the stack
	size_t cur_func;
	size_t entry_func;
	size_t func_count;
	size_t program_length;
	uint8_t* stack;
	size_t pc;
	uint8_t* program;
	GBLN_func* func_table;

	GBLN_data* data;
	size_t data_size;
	
	GBLN_frame* frames;
	size_t frame_count;
	size_t frame_cap;
	
}GBLN_vm;

#define NEXT_OP(vm) (vm->program[vm->pc++])

void printstack(GBLN_vm* vm){
	printf("stack (len=%zu):\n",vm->stack_size);
	for(int i = 0; i < vm->stack_size; i++){
		printf("%d:%c\n", vm->stack[i], vm->stack[i]);
	}
}

void pop(GBLN_vm* vm, void* dst, size_t bytes){
	vm->stack_size -= bytes;
	memcpy(dst, vm->stack + vm->stack_size, bytes);
#ifdef DEBUGPRINT
	//printf(" pop %d\n",*(int*)dst);
	//printstack(vm);
	for (int i = 0; i < bytes; i++) {
		char ch = ((char*)dst)[i];
		printf("    %d", ch);
		if(ch >= '!' && ch <= '~'){
			printf("(%c)",ch);
		}
		putchar('\n');
	}
#endif
}

void push(GBLN_vm* vm, void* src, size_t bytes){
	memcpy(vm->stack + vm->stack_size, src, bytes);	
	vm->stack_size += bytes;
#ifdef DEBUGPRINT
	//printf(" push %d\n",*(int*)src);
	//printstack(vm);
	for (int i = 0; i < bytes; i++) {
		char ch = ((char*)src)[i];
		printf("    %d", ch);
		if(ch >= '!' && ch <= '~'){
			printf("(%c)",ch);
		}
		putchar('\n');
	}
#endif
}

	
#define POP(vm, var) pop(vm, &var, sizeof(var))
#define PUSH(vm, var) push(vm, &var, sizeof(var))

void readnext(GBLN_vm* vm, void* dst, size_t bytes){
	memcpy(dst, vm->program + vm->pc, bytes);
	vm->pc += bytes;
}

void InitGBLN_vm(GBLN_vm* vm){
	vm->stack = malloc(GBLN_STACK_SIZE);
	vm->stack_size = 0; 
	vm->stack_max = GBLN_STACK_SIZE;

	vm->frame_cap = 64;
	vm->frame_count = 0;
	vm->frames = malloc(sizeof(GBLN_frame) * vm->frame_cap);

	vm->pc = 0;
}

void printvar(GBLN_var* var){
	switch (var->type) {
		case GBLN_CHAR_T: printf("char:%c",var->char_val); break;
		case GBLN_FLOAT_T: printf("float:%f", var->float_val); break;
		case GBLN_INT_T: printf("int:%d",var->int_val); break;
		case GBLN_STRUCT_T:
			 printf("struct %s:",var->struct_val->def->name);
			 for(int i = 0; i < var->struct_val->def->count; i++){
			 	printf("\n    %s - ",var->struct_val->def->vars[i].name);					printvar(&var->struct_val->data[i]);
			}
		break;
		case GBLN_ARRAY_T:
			printf("array: { ");
			for(int i = 0; i < var->array_val->len; i++){
				switch (var->array_val->type) {
					case GBLN_CHAR_T: 
						printf("%c, ", ((char*)var->array_val->data)[i]); 
						break;
					default:
						printf("none, ");
				}
			}
			printf("}");
		break;
		default:
			printf("unknown"); return;//error
		break;
	}

}
int pushvar(GBLN_vm* vm, GBLN_var* var){

	switch (var->type) {
		case GBLN_CHAR_T: PUSH(vm, var->char_val); break;
		case GBLN_FLOAT_T: PUSH(vm, var->float_val); break;
		case GBLN_INT_T: PUSH(vm, var->int_val); break;
		case GBLN_STRUCT_T: push(vm,&var->struct_val, sizeof(void*)); break;
		case GBLN_ARRAY_T: push(vm,&var->array_val, sizeof(void*)); break;
		default:
			return -1;//error
		break;
	}
#ifdef DEBUGPRINT
	printf("pushing  ");
	printvar(var);
	putchar('\n');
#endif
	return 0;
}

int popvar(GBLN_vm* vm, GBLN_var* var){

	switch (var->type) {
		case GBLN_CHAR_T: POP(vm,var->char_val); break;
		case GBLN_FLOAT_T: POP(vm, var->float_val); break;
		case GBLN_INT_T: POP(vm, var->int_val); break;
		case GBLN_STRUCT_T: pop(vm,&var->struct_val, sizeof(void*)); break;
		case GBLN_ARRAY_T: pop(vm,&var->array_val, sizeof(void*)); break;
		default:
			return -1;//error
		break;
	}
#ifdef DEBUGPRINT
	printf("popping  ");
	printvar(var);
	putchar('\n');
#endif
	return 0;
}

size_t array_elem_size(GBLN_ARRAY* arr){
	size_t size = 0;
	switch (arr->type) {
		case GBLN_CHAR_T: size = sizeof(GBLN_CHAR); break;
		case GBLN_FLOAT_T: size = sizeof(GBLN_FLOAT); break;
		case GBLN_INT_T: size = sizeof(GBLN_INT); break;
		case GBLN_STRUCT_T:
		case GBLN_ARRAY_T: size = sizeof(void*); break;

	}
	return size;

}

void call_internal(GBLN_vm* vm, GBLN_func* func){

	if(vm->frame_count == vm->frame_cap){
		vm->frame_cap *= 2;
		vm->frames = realloc(vm->frames, sizeof(GBLN_frame) * vm->frame_cap);
	}

	GBLN_frame* frame = &vm->frames[vm->frame_count++];
	frame->func = func;
	frame->locals.count = func->locals;
	if(frame->locals.count > STATIC_VARS_COUNT){
		frame->locals.vars = malloc(sizeof(GBLN_var) * frame->locals.count);
	}
	frame->return_addr = vm->pc;
			

	//push args as well
	vm->pc = func->memaddr;
	vm->cur_func = func->entry;
}


int execute_next(GBLN_vm* vm){

	uint8_t opcode = NEXT_OP(vm);
#ifdef DEBUGPRINT
	printf("%zu:%d | %s\n", vm->pc,opcode, GBLN_op_name(opcode));

	sleep(1);
#endif
	switch (opcode) {
		case 0: //NOP 
		break;
		case 1: //iADD
		{
			BINARY_OP(GBLN_INT, 32, +);
		}break;
		case 2: //fADD
		{
			BINARY_OP(GBLN_FLOAT, 32, +);
		}break;
		case 3: //iSUB
		{
			BINARY_OP(GBLN_INT, 32, -);
		}break;
		case 4: //fSUB
		{
			BINARY_OP(GBLN_FLOAT, 32, -);
		}break;
		case 5: //iMUL
		{
			BINARY_OP(GBLN_INT, 32, *);
		}break;
		case 6: //fMUL
		{
			BINARY_OP(GBLN_FLOAT, 32, *);
		}break;
		case 7: //iDIV
		{
			BINARY_OP(GBLN_INT, 32, /);
		}break;
		case 8: //fDIV
		{
			BINARY_OP(GBLN_FLOAT, 32, /);
		}break;
		case 9: //iMOD
		{
			BINARY_OP(GBLN_INT, 32, %);
		}break;
		case 10: //bPUSH
		{
			uint8_t tmp = NEXT_OP(vm);
			PUSH(vm,tmp);
		}break;
		case 11: //bPOP 
		{
			vm->stack_size--;

		}break;
		case 12: //wPUSH
		{
			memcpy(vm->stack + vm->stack_size, vm->program + vm->pc, 2);
			vm->stack_size += 2;
			vm->pc += 2;
			
		}break;
		case 13: //wPOP 
		{
			vm->stack_size -= 2;

		}break;
		case 14: //dPUSH
		{
			memcpy(vm->stack + vm->stack_size, vm->program + vm->pc, 4);
			//printf(" push %d\n", vm->stack[vm->stack_size]);
			vm->stack_size += 4;
			vm->pc += 4;
		}break;
		case 15: //dPOP 
		{
			vm->stack_size -= 4;

		}break;
		case 16: //qPUSH
		{
			memcpy(vm->stack + vm->stack_size, vm->program + vm->pc, 8);
			vm->stack_size += 8;
			vm->pc += 8;
		}break;
		case 17: //qPOP 
		{
			vm->stack_size -= 8;

		}break;
		case 18: //CALL
		{
			uint32_t addr;
			readnext(vm, &addr, sizeof(addr));

			if(addr >= vm->func_count){
				return -1;
			}

			GBLN_func* func = &vm->func_table[addr];
			if(func->flags & GF_EXTERNAL){
				func->native_function(vm);
				break;
			}
			call_internal(vm, func);


		}break;
		case 19: //JMP
		{
			uint32_t addr;
			readnext(vm, &addr, sizeof(addr));
			vm->pc = addr;
		}break;
		case 20: //RETURN
		{
			GBLN_frame* frame = &vm->frames[--vm->frame_count];
			vm->pc = frame->return_addr;
			vm->cur_func = frame->func->entry;

			if(frame->locals.count > STATIC_VARS_COUNT){
				free(frame->locals.vars);
			}
			if(vm->cur_func == vm->entry_func){ //quit if returning out of entry
				puts("exiting vm");
				if(vm->stack_size != 0){
					printf("WARNING: stack is not empty (%zu bytes)\n",vm->stack_size);
				}
				return 1;
			}
			
		}break;
		case 21: //iDUP
		{
			uint32_t tmp;
			POP(vm,tmp);
			PUSH(vm,tmp);
			PUSH(vm,tmp);

		}break;
		case 22: //ifz
		{
			uint32_t tmp;
			POP(vm,tmp);
			uint32_t addr;
			readnext(vm, &addr, sizeof(addr));

			if(tmp == 0){
				vm->pc = addr;
			}

		}break;
		case 23: //ifnz
		{
			uint32_t tmp;
			POP(vm,tmp);
			uint32_t addr;
			readnext(vm, &addr, sizeof(addr));

			if(tmp != 0){
				vm->pc = addr;
			}

		}break;
		case 24: //ifg
		{
			uint32_t tmp;
			POP(vm,tmp);
			uint32_t addr;
			readnext(vm, &addr, sizeof(addr));

			if(tmp > 0){
				vm->pc = addr;
			}

		}break;
		case 25: //ifl
		{
			uint32_t tmp;
			POP(vm,tmp);
			uint32_t addr;
			readnext(vm, &addr, sizeof(addr));

			if(tmp < 0){
				vm->pc = addr;
			}

		}break;
		case 26: //ifge
		{
			uint32_t tmp;
			POP(vm,tmp);
			uint32_t addr;
			readnext(vm, &addr, sizeof(addr));

			if(tmp >= 0){
				vm->pc = addr;
			}

		}break;
		case 27: //ifle
		{
			uint32_t tmp;
			POP(vm,tmp);
			uint32_t addr;
			readnext(vm, &addr, sizeof(addr));

			if(tmp <= 0){
				vm->pc = addr;
			}

		}break;
		case 28://lddata
		{
			uint16_t addr;
			readnext(vm, &addr, sizeof(addr));

			GBLN_data* var = &vm->data[addr];
			pushvar(vm, &var->data);
			
		}break;
		case 29://ldfield
		{
			GBLN_var var;
			var.type = GBLN_STRUCT_T;
			popvar(vm, &var);

			uint16_t arg;
			readnext(vm, &arg, sizeof(arg));

			GBLN_var* var1 = &var.struct_val->data[arg];


			pushvar(vm, var1);


		}break;
		case 30://strfield
		{
			GBLN_var var;
			var.type = GBLN_STRUCT_T;
			popvar(vm, &var);
			uint16_t arg;
			readnext(vm, &arg, sizeof(arg));

			GBLN_var* var1 = &var.struct_val->data[arg];

			popvar(vm, var1);

		}break;
		case 31://ldarr
		{
			int arg;
			POP(vm,arg);

			GBLN_var arr;
			arr.type = GBLN_ARRAY_T;
			popvar(vm, &arr);

			if(arg >= arr.array_val->len) {
				printf("ERROR : index out of range '%d':'%d'\n",arg, arr.array_val->len);
				return -1;
			}
			size_t size = array_elem_size(arr.array_val);

			void* ptr = arr.array_val->data + (size * arg);
#ifdef DEBUGPRINT
			printf("loaded arg %d: ",arg);
			GBLN_var tmp;
			tmp.type = arr.array_val->type;
			memcpy(&tmp.struct_val,ptr,size);
			printvar(&tmp);
			putchar('\n');
#endif
			push(vm,arr.array_val->data +(size * arg),size);

		}break;
		case 32://strarr
		{
			int arg;
			POP(vm,arg);
			
			GBLN_var arr;
			arr.type = GBLN_ARRAY_T;
			popvar(vm, &arr);

			if(arg >= arr.array_val->len) return -1;
			
			size_t size = array_elem_size(arr.array_val);

			pop(vm,arr.array_val->data +(size * arg),size);

		}break;
		case 33://load
		{
			uint16_t idx;
			readnext(vm, &idx, sizeof(idx));
			GBLN_var* var;
			GBLN_locals* locals = &vm->frames[vm->frame_count - 1].locals;
			if(locals->count > STATIC_VARS_COUNT){
				var = &locals->vars[idx];
			}
			else{
				var = &locals->static_vars[idx];
			}
			pushvar(vm, var);
		}break;
		case 34://store
		{
			uint16_t idx;
			readnext(vm, &idx, sizeof(idx));
			GBLN_var* var;
			GBLN_locals* locals = &vm->frames[vm->frame_count - 1].locals;
			if(locals->count > STATIC_VARS_COUNT){
				var = &locals->vars[idx];
			}
			else{
				var = &locals->static_vars[idx];
			}
			//WARNIGN!!
			//TODO FIX!!!!! THIS WILL CAUSE PROBLEMS!!!!
			var->type = GBLN_STRUCT_T;
			//WARNINY!!

			popvar(vm, var);

		}break;
		case 35://istore
		{
			uint16_t idx;
			readnext(vm, &idx, sizeof(idx));
			GBLN_var* var;
			GBLN_locals* locals = &vm->frames[vm->frame_count - 1].locals;
			if(locals->count > STATIC_VARS_COUNT){
				var = &locals->vars[idx];
			}
			else{
				var = &locals->static_vars[idx];
			}
			//WARNIGN!!
			//TODO FIX!!!!! THIS WILL CAUSE PROBLEMS!!!!
			var->type = GBLN_INT_T;
			//WARNINY!!

			popvar(vm, var);

		}break;

	default:
		return -1;



	}

	return 0;

}



void nativeprint_int(GBLN_vm* vm){
	GBLN_INT var;
	POP(vm,var);
	printf("%d",var);
}
void nativeprint_char(GBLN_vm* vm){
	GBLN_CHAR var;
	POP(vm,var);
	putchar(var);
}
void nativeprint_float(GBLN_vm* vm){
	GBLN_FLOAT var;
	POP(vm,var);
	printf("%f",var);
}

GBLN_func MakeNativeFunc(GBLN_nativefunc native, char* name){
	GBLN_func func;
	func.name = name;
	func.native_function = native;
	func.flags = GF_RESOLVED | GF_EXTERNAL;
	return func;
}


void tryresolve(GBLN_object* gobject, GBLN_nativefunc native, const char* name){
	for (int i = 0; i < gobject->funcslen; i++) {
		if(strcmp(name, gobject->func_table[i].name) == 0){
			gobject->func_table[i].native_function = native;
			gobject->func_table[i].flags  |= GF_RESOLVED | GF_EXTERNAL;
			return;
		}
	}
}



void InitInbuiltFuncs(GBLN_object* gobject){
	
	tryresolve(gobject,&nativeprint_int, "print_int"); 
	tryresolve(gobject,&nativeprint_float, "print_float"); 
	tryresolve(gobject,&nativeprint_char, "print_char"); 

}

void StartVM(){
	GBLN_vm vm;
	InitGBLN_vm(&vm);
	GblnCompilerStart();

	if(GblnCompileGasm("test.gasm") != 0){
		return;
	}
	
	GBLN_object gobject;
	int status = GblnCompilerExtract(&gobject);

	if(status != 0){
		return;
	}

	puts("compiled");

	OutputGblno("a.gblno", &gobject);
	InitInbuiltFuncs(&gobject);


	for (int i = 0; i < gobject.funcslen; i++) {
		if((gobject.func_table[i].flags & GF_RESOLVED) == 0){
			status = -1;
			printf("LINK ERROR: unresolved function '%s'\n",gobject.func_table[i].name);
		}	
	}

	if(status != 0){
		return;
	}


	vm.program = gobject.code;
	vm.func_table = gobject.func_table;
	vm.entry_func = gobject.entryfunc;
	
	vm.func_count = gobject.funcslen;
	vm.program_length = gobject.codelen;
	
	vm.data = gobject.data;
	vm.data_size = gobject.datalen;

	call_internal(&vm, &vm.func_table [vm.entry_func]);

	puts("ready");
	
	while(execute_next(&vm) == 0){

	}
}
