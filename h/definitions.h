#ifndef _DEFINITIONS_H_
#define _DEFINITIONS_H_


#define DIRECTIVE_END ".end"
#define LABEL_MAX_LENGTH 16
#define SYMBOL_MAX_NUMBER 64
#define LINE_MAX_LENGTH 128
#define FIELD_MAX_LENGTH 16
#define SYMBOLTABLE_MAX_SIZE 128
#define SECTION_MAX_NUMBER 32
#define SECTION_MAX_NAME_LENGTH 32

enum {
	TEXT, DATA, BSS
};

enum{
	LOCAL, GLOBAL
};
enum { 
	DIR_CHAR, DIR_WORD, DIR_LONG, DIR_ALIGN, 
	DIR_SKIP, DIR_EXTERN, DIR_PUBLIC, DIR_SECTION 
};

enum { 
	INS_INT, INS_ADD, INS_SUB, INS_MUL, INS_DIV,
	INS_CMP, INS_AND, INS_OR, INS_NOT, INS_TEST,
	INS_LDR, INS_STR, INS_CALL, INS_IN, INS_OUT,
	INS_MOV, INS_SHR, INS_SHL, INS_LDCH, INS_LDCL,
	INS_LDC
};

enum {
	REG_PC=16, REG_LR, REG_SP, REG_PSW
};

enum {
	R_386_32, R_386_PC32
};

enum {
	REL_W, REL_NW, REL_HW0, REL_HW1, REL_B0, REL_B1, REL_B2, REL_B3
};




extern char* rel_types[];
extern char* rel_byte_choice[];


typedef struct {
	int ord;
	int section;
	int offset;
	char binding;
	int defined;
	char name[LABEL_MAX_LENGTH];
}symbol_entry;


typedef struct {
	int base;
	int length;
	int symbol_number;
}section_entry;




typedef struct {
	int offset;
	int type;
	int bytes;
	int value;
	int section;
}rel_entry;


typedef struct {
	int section;
	char* code;
}code;

extern code** section_code;

extern rel_entry** rel_table;
extern int relocationCounter;


extern int symbolCounter;

extern symbol_entry** sym_table;

extern int locationCounter;
extern int current_section;

extern section_entry** section_table;
extern int sectionCounter;

extern void abortAssembly();




#endif