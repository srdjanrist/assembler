#include <stdio.h>
#include <stdlib.h>

#include "definitions.h"

static int addSymbol(char * , int , int );

static void finishSection() {
	section_table[current_section]->length = locationCounter;
}

static int addSection(char * name) {
	if (section_table == 0) {
		section_table = (section_entry**)calloc(20, sizeof(section_entry*));
		if (sym_table == 0) {
			current_section = addSection("UND");
		}
	}
	section_entry * temp_sec_entry = (section_entry*)calloc(1, sizeof(section_entry));
	temp_sec_entry->base = 0;
	temp_sec_entry->length = 0;

	if (current_section != -1) {
		finishSection();
	}
	current_section = sectionCounter;
	locationCounter = 0;
	temp_sec_entry->symbol_number = addSymbol(name, 1, LOCAL);

	section_table[sectionCounter] = temp_sec_entry;
	
	
	return sectionCounter++;
}



static int addSymbol(char * name, int defined, int binding) {
	if (sym_table == 0) {
		sym_table = (symbol_entry**)calloc(100, sizeof(symbol_entry*));
		if (section_table == 0) {
			current_section = addSection("UND");
		}
	}
	symbol_entry* temp_entry = (symbol_entry*)calloc(1, sizeof(symbol_entry));
	int len = strlen(name);
	
	int i = 0;
	for (; i < symbolCounter; i++) {
		if (strcmp(name, sym_table[i]->name) == 0) {
			if (sym_table[i]->defined == 1 || sym_table[i]->binding == GLOBAL) {
				printf("Error: Redefinicija simbola '%s'\n", name);
				free(temp_entry);
				abortAssembly();
			}
			else {
				sym_table[i]->defined = defined; //should be 1 always
				sym_table[i]->section = current_section;
				sym_table[i]->offset = locationCounter;
				return -1;
			}
		}
	}

	sym_table[symbolCounter] = temp_entry;

	sym_table[symbolCounter]->section = current_section;
	if (current_section != -1)
		sym_table[symbolCounter]->offset = locationCounter;
	else
		sym_table[symbolCounter]->offset = 0;
	sym_table[symbolCounter]->binding = binding;
	strcpy(sym_table[symbolCounter]->name, name);

	sym_table[symbolCounter]->ord = symbolCounter;
	sym_table[symbolCounter]->defined = defined;
	return symbolCounter++;
}

int getSectionFromName(char * readLine) {
	if (strstr(readLine, ".text") - readLine == 0)
		return 0;
	else if (strstr(readLine, ".data") - readLine == 0)
		return 1;
	else if (strstr(readLine, ".bss") - readLine == 0)
		return 2;

	return -1;
}

int getDirectiveFromName(char * readLine) {
	if (strstr(readLine, ".char") - readLine == 0)
		return DIR_CHAR;
	else if (strstr(readLine, ".word") - readLine == 0)
		return DIR_WORD;
	else if (strstr(readLine, ".long") - readLine == 0)
		return DIR_LONG;
	else if (strstr(readLine, ".align") - readLine == 0)
		return DIR_ALIGN;
	else if (strstr(readLine, ".skip") - readLine == 0)
		return DIR_SKIP;
	else if (strstr(readLine, ".extern") - readLine == 0)
		return DIR_EXTERN;
	else if (strstr(readLine, ".public") - readLine == 0)
		return DIR_PUBLIC;
	else if (strstr(readLine, ".section") - readLine == 0)
		return DIR_SECTION;
	else return -1;
}

void align() {
	locationCounter += ((~(locationCounter & 0x3) + 1) & 0x3);
}
static void processDirective(char * readLine) {
	char * ptr = strchr(readLine, ':');
	char * label = 0;
	if (ptr != 0) {
		label = readLine;
		*ptr = 0;
		readLine = ptr + 2;
		if (strstr(readLine, ".public ") != 0 || strstr(readLine, ".extern ") != 0 || strstr(readLine, ".section ") != 0) {
			printf("Error: labela uz direktivu .public, .extern ili .section\n");
			abortAssembly();
		}
		addSymbol(label, 1, LOCAL);
	}

	if (strstr(readLine, ".section ") != 0) {
		readLine += strlen(".section ");
		int sec = getSectionFromName(readLine);
		if (sec == -1) {
			printf("Error: Nepostojeca sekcija '%s'. Koristiti .text, .data, .bss\n", readLine);
			abortAssembly();
		}
		sec = addSection(readLine);
		
		return;
	}
	else if (strstr(readLine, ".public ") - readLine == 0) {
		readLine += strlen(".public ");
		if (strchr(readLine, '.') != 0) {
			printf("Error: Simbol ne sme sadrzati '.'");
			abortAssembly();
		}
		char * token = strtok(readLine, ", ");
		while (token) {
			addSymbol(token, 0, LOCAL);
			token = strtok(0, ", ");
		}

	}
	else if (strstr(readLine, ".extern ") - readLine == 0) {
		readLine += strlen(".extern ");
		if (strchr(readLine, '.') != 0) {
			printf("Error: Simbol ne sme sadrzati '.'");
			abortAssembly();
		}
		char * token = strtok(readLine, ", ");
		while (token) {
			addSymbol(token, 0, GLOBAL);
			token = strtok(0, ", ");
		}
	}
	else {
		int dir = getDirectiveFromName(readLine);

		while (*readLine != ' ' && *readLine != 0) {
			readLine++;
		}
		while (*readLine == ' ') readLine++;
		int arg = 0;
		if (*readLine != 0) {
			int i = 0;
			for (; i < strlen(readLine); i++)
				if (readLine[i] == ',')
					arg++;
			arg++;
		}

		switch (dir) {
		case DIR_CHAR:
			locationCounter += arg;
			align();
			break;
		case DIR_WORD:
			locationCounter += 2 * arg;
			align();
			break;
		case DIR_LONG:
			locationCounter += 4 * arg;
			align();
			break;
		case DIR_ALIGN:
			if (arg > 0) {
				printf("Warning: Direktivi .align nisu potrebni argumenti\n");
			}
			align();
			break;
		case DIR_SKIP:
			if (arg != 1) {
				printf("Error: Direktivi .skip je potreban tacno jedan argument\n");
				abortAssembly();
			}
			locationCounter += atoi(readLine);
			break;
		default:
			printf("Error: Nepoznata direktiva\n");
			abortAssembly();

		}
	}

}

void firstPass(FILE* f) {
	char readLine[LINE_MAX_LENGTH];
	int lineLen;

	do {
		readLine[0] = 0;
		fgets(readLine, LINE_MAX_LENGTH, f);
		lineLen = strlen(readLine);
		if (readLine[lineLen - 1] == '\n')
			readLine[lineLen - 1] = '\0';
		if (strcmp(DIRECTIVE_END, readLine) == 0) {
			finishSection();
			break;
		}
		if (lineLen == 1) continue;

		if (strchr(readLine, '.') != 0) {
			processDirective(readLine);

		}
		else {
			if (strchr(readLine, ':') == 0) {
				if (current_section == 0 || current_section == -1) {
					printf("Error: instrukcija '%s' izvan sekcije\n", readLine);
					abortAssembly();
				}
				if (strstr(readLine, "ldc") == readLine && (!strstr(readLine, "ldcl") && !strstr(readLine, "ldch")))
					locationCounter += 4;
				locationCounter += 4;
				continue;
			}
			else {
				char * token = strtok(readLine, ":");
				addSymbol(token, 1, LOCAL);
				token = strtok(0, ":");
				if (token != 0) {
					while (*token == ' ')
						token++;
					if (*token != 0) {
						locationCounter += 4;
						if (strstr(token, "ldc") == token && (!strstr(token, "ldcl") && !strstr(token, "ldch")))
							locationCounter += 4;
					}
				}
			}
		}
	} while (strcmp(DIRECTIVE_END, readLine) != 0);



	int i;
	for (i = 1; i < symbolCounter; i++) {
		if (sym_table[i]->binding == LOCAL && sym_table[i]->defined == 0) {
			printf("Error: Nije definisan simbol za izvoz '%s'\n", sym_table[i]->name);
			abortAssembly();
		}
	}

	
}
