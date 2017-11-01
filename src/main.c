#include <stdio.h>
#include <stdlib.h>

#include "definitions.h"
#include "bin_file_manip.h"


extern int bin_file_sections[] = {
	0x23FFFFFF,
	0x23CA00AA, 0x23DB00AA, 0x23EF00AA, 0x23FA00AA,
	0x23CA00FF, 0x23DB00FF, 0x23EF00FF, 0x23FA00FF
};


extern void firstPass(FILE*);
extern void secondPass(FILE*);
int getSectionFromName(char *);

int symbolCounter = 0;


symbol_entry** sym_table = 0;

int locationCounter = 0;
int current_section = -1;

section_entry** section_table = 0;
int sectionCounter = 0;
static FILE* f = 0;

void freeMemory() {
	int i = 0;
	for (; i < symbolCounter; i++) {
		free(sym_table[i]);
	}
	if (sym_table != 0)
		free(sym_table);

	for (i = 0; i < relocationCounter; i++)
		free(rel_table[i]);
	if (rel_table != 0)
		free(rel_table);

	for (i = 0; i < sectionCounter; i++) {
		free(section_table[i]);
		if (section_code != 0) {
			free(section_code[i]);
		}
	}
	if (section_code != 0)
		free(section_code);
	if (section_table != 0)
		free(section_table);
}

void finishAssembly() {
	freeMemory();
	return;
}

void abortAssembly() {
	freeMemory();
	if (f != 0)
		fclose(f);
	exit(1);
}

void sortSymbolTable() {
	int i;
	for (i = 1; i < sectionCounter; i++) {
		if (i == section_table[i]->symbol_number)
			continue;
		symbol_entry* temp = sym_table[section_table[i]->symbol_number];
		sym_table[section_table[i]->symbol_number] = sym_table[i];
		sym_table[i] = temp;
		section_table[i]->symbol_number = i;

	}
	for (i = 0; i < symbolCounter; i++)
		sym_table[i]->ord = i;
}

int hasRelocation(int i) {
	int k;
	for (k = 0; k < relocationCounter; k++)
		if (rel_table[k]->value == i)
			return 1;
	return 0;
}

int main(int argv, char* argc[]) {
	if (argv != 3) {
		printf("Ulaz je oblika asm [input] [output].elf\n");
		return 1;
	}
	
	FILE * f = fopen(argc[1], "r");

	if (f == 0) {
		printf("Ulazni fajl ne postoji\n");
		return 1;
	}

	int i;

	printf("Uspesno otvoren ulazni fajl\n");

	firstPass(f);

	/*for (i = 0; i < symbolCounter; i++)
		printf("%14s\t%d\n", sym_name_table[i]->name, sym_name_table[i]->symbol_table_entry);*/

		/*printf("*********************************\n\t\tAFTER FIRST PASS\n*********************************\n");
		printf("#Tablea simbola\n");
		printf("%16s\t%s\t%s\t%s\t%s\t%s\n", "name", "section", "offset", "bind", "def", "ord");
		for (i=0; i < symbolCounter; i++) {
			printf("%12s\t%12s\t%d\t%d\t%d\t%d\n", sym_name_table[sym_table[i]->name]->name,
				sym_table[i]->section != -1 ? sym_name_table[section_table[sym_table[i]->section]->name]->name : "UND",
				sym_table[i]->offset, sym_table[i]->binding, sym_table[i]->defined, sym_table[i]->ord);
		}*/


	sortSymbolTable();
	/*printf("*********************************\n\t\tAFTER SORTING\n*********************************\n");
	printf("#Tablea simbola\n");
	printf("%16s\t%s\t%s\t%s\t%s\t%s\n", "name", "section", "offset", "bind", "def", "ord");
	for (i=0; i < symbolCounter; i++) {
		printf("%12s\t%12s\t%d\t%d\t%d\t%d\n", sym_name_table[sym_table[i]->name]->name,
			sym_table[i]->section != -1 ? sym_name_table[section_table[sym_table[i]->section]->name]->name : "UND",
			sym_table[i]->offset, sym_table[i]->binding, sym_table[i]->defined, sym_table[i]->ord);
	}*/


	/*for (i = 0; i < symbolCounter; i++)
		printf("%14s\t%d\n", sym_name_table[i]->name, sym_name_table[i]->symbol_table_entry);*/


	printf("Obavestenje: Prvi prolaz uspesno zavrsen\n");




	rewind(f);
	secondPass(f);
	printf("Obavestenje: Drugi prolaz uspesno zavrsen\n");

	fclose(f);

	f = fopen(argc[2], "w");
	

	int len = strlen(argc[2]);
	char * binStr = (char*)calloc(len + 2, sizeof(char));
	strcpy(binStr, argc[2]);
	binStr[len] = 'b';
	binStr[len + 1] = 0;

	FILE * fbin = fopen(binStr, "wb");

	free(binStr);

	fwrite(&bin_file_sections[SYMBOL_TABLE], 1, sizeof(int), fbin);

	int exportSectionCounter = sectionCounter - 1;
	int exportSymbolCounter = symbolCounter-1;
	/*for (i = 1; i < symbolCounter; i++) {
		/*if (i < sectionCounter || sym_table[i]->binding == GLOBAL || (sym_table[i]->binding == LOCAL &&
			hasRelocation(i))) {
			exportSymbolCounter++;
		//}
	}*/

	
	fwrite(&exportSectionCounter, sizeof(int), 1, fbin);
	fwrite(&exportSymbolCounter, sizeof(int), 1, fbin);
	fwrite(&relocationCounter, sizeof(int), 1, fbin);

	fprintf(f, "#Tabela simbola\n");
	fprintf(f, "%3s %12s %8s %8s %7s %5s %4s\n", "ord", "name", "section", "offset", "length", "bind", "def");
	for (i = 1; i < sectionCounter; i++) {
		fwrite(section_table[i],sizeof(section_entry),  1, fbin);
	}
	for (i = 1; i < symbolCounter; i++) {
		/*if (i < sectionCounter || sym_table[i]->binding == GLOBAL || (sym_table[i]->binding == LOCAL &&
			hasRelocation(i))) {*/
			fwrite(sym_table[i], sizeof(symbol_entry), 1, fbin);
		//}
	}
	
	fwrite(&bin_file_sections[SYMBOL_TABLE + 4], 1, sizeof(int), fbin);
	if (relocationCounter > 0) {
		fwrite(&bin_file_sections[RELOCATION_TABLE], sizeof(int), 1, fbin);
		for (i = 0; i < relocationCounter; i++)
			fwrite(rel_table[i], sizeof(rel_entry), 1, fbin);
		fwrite(&bin_file_sections[RELOCATION_TABLE+4], sizeof(int), 1, fbin);
	}
	for (i = 1; i < sectionCounter; i++) {
		if (getSectionFromName(sym_table[section_table[i]->symbol_number]->name) == BSS) {
			continue;
		}
		fwrite(&bin_file_sections[NEW_SECTION], sizeof(int), 1, fbin);
		fwrite(&i, sizeof(int), 1, fbin);
		int secLen = section_table[i]->length;
		fwrite(&secLen, sizeof(int), 1, fbin);
		fwrite(section_code[i]->code, sizeof(char), secLen, fbin);
	}
	fwrite(&bin_file_sections[BIN_EOF], sizeof(int), 1, fbin);

	for (i = 0; i < symbolCounter; i++) {
		if (i < sectionCounter) {
			fprintf(f, "%3d %12s %8d %8d %7d %5c %4d\n", sym_table[i]->ord, sym_table[i]->name,
				sym_table[i]->section /*!= -1 ? sym_name_table[section_table[sym_table[i]->section]->name]->name : "UND"*/,
				sym_table[i]->offset,
				section_table[i]->length,
				sym_table[i]->binding ? 'G' : 'L',
				sym_table[i]->defined);
		}
		else {
			fprintf(f, "%3d %12s %8d %8d %7s %5c %4d\n", sym_table[i]->ord, sym_table[i]->name,
				sym_table[i]->section /*!= -1 ? sym_name_table[section_table[sym_table[i]->section]->name]->name : "UND"*/,
				sym_table[i]->offset,
				"",
				sym_table[i]->binding ? 'G' : 'L',
				sym_table[i]->defined);
		}

	}

	fprintf(f, "\n\n");

	fprintf(f, "#RELOCATIONS");
	fprintf(f, "\n");

	int curSec = -1;
	for (i = 0; i < relocationCounter; i++) {
		if (curSec != rel_table[i]->section) {
			fprintf(f, "\n");
			curSec = rel_table[i]->section;
			fprintf(f, "#.rel%s\n", sym_table[section_table[rel_table[i]->section]->symbol_number]->name);
			fprintf(f, "%s %9s %9s %7s\n", "offset", "type", "bytes", "value");
		}
		fprintf(f, "%3d %12s %9s %7d\n", rel_table[i]->offset, rel_types[rel_table[i]->type], rel_byte_choice[rel_table[i]->bytes], rel_table[i]->value);

		char typeAndBytes = (rel_table[i]->type << 4) | (rel_table[i]->bytes);
	}

	fprintf(f, "\n\n");
	for (i = 1; i < sectionCounter; i++) {
		if (getSectionFromName(sym_table[section_table[i]->symbol_number]->name) == BSS) {
			continue;
		}
	/*	fwrite(&bin_file_sections[NEW_SECTION], 1, sizeof(int), fbin);
		fwrite(&i, 1, sizeof(int), fbin);*/
		int secLen = section_table[i]->length;
	/*	fwrite(&secLen, 1, sizeof(int), fbin);
		fwrite(section_code[i]->code, secLen, sizeof(char), fbin);*/

		int j = 0;
		fprintf(f, "#%s\n", sym_table[section_table[i]->symbol_number]->name);
		int k;
		for (k = 0; k < section_table[i]->length; k++) {
			char c = section_code[i]->code[k];
			fprintf(f, "%X%X ", (c & 0xF0) >> 4, c & 0x0F);
			j++;
			if (j % 8 == 0) {
				j = 0;
				fprintf(f, "\n");
			}
		}
		fprintf(f, "\n");
		if (j % 8 != 0)
			fprintf(f, "\n");
	}
//	fwrite(&bin_file_sections[CODE + 4], 1, sizeof(int), fbin);


	/*printf("*********************************\n\t\tAFTER SECOND PASS\n*********************************\n");
	printf("#Tablea simbola\n");
	printf("%16s\t%s\t%s\t%s\t%s\t%s\n", "name", "section", "offset", "bind", "def", "ord");
	for (i = 0; i < symbolCounter; i++) {
		printf("%12s\t%12s\t%d\t%d\t%d\t%d\n", sym_table[i]->name,
			sym_table[i]->section != -1 ? sym_table[section_table[sym_table[i]->section]->symbol_number]->name : "UND",
			sym_table[i]->offset, sym_table[i]->binding, sym_table[i]->defined, sym_table[i]->ord);
	}*/


		i = 0;
		printf("#Tablea simbola\n");
		printf("%16s\t%s\t%s\t%s\t%s\t%s\n", "name", "section", "offset", "bind", "def", "ord");
		for (; i < symbolCounter; i++) {
			printf("%12s\t%12s\t%d\t%d\t%d\t%d\n", sym_table[i]->name,
				sym_table[i]->section != -1 ? sym_table[section_table[sym_table[i]->section]->symbol_number]->name : "UND",
				sym_table[i]->offset, sym_table[i]->binding, sym_table[i]->defined, sym_table[i]->ord);
		}

		printf("#Tabela sekcija\n");
		for (i = 0; i < sectionCounter; i++) {
			printf("%16s\t%d\t%d\n", sym_table[section_table[i]->symbol_number]->name, 
				section_table[i]->base, section_table[i]->length);
		}


	finishAssembly();


	fclose(f);
	fclose(fbin);
	return 0;

}
