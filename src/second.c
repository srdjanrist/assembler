#include <stdio.h>

#include "definitions.h"

extern int getDirectiveFromName(char *);
extern void align();

int getImmediateOperand(char*);
int getSymbolFromTable(char*);
void addRelocation(int, int, int, int);

int relocationCounter = 0;
rel_entry** rel_table = 0;
code** section_code = 0;

typedef struct instr {
	char opcode;
	char mnemonic[5];
}instr;
instr allInstr[] = {
	{ 0x00, "int"},
	{ 0x01, "add" },
	{ 0x02, "sub" },
	{ 0x03, "mul" },
	{ 0x04, "div" },
	{ 0x05, "cmp" },
	{ 0x06, "and" },
	{ 0x07, "or" },
	{ 0x08, "not" },
	{ 0x09, "test" },
	{ 0x0A, "ldr" },
	{ 0x0A, "str" },
	{ 0x0C, "call" },
	{ 0x0D, "in" },
	{ 0x0D, "out" },
	{ 0x0E, "mov" },
	{ 0x0E, "shr" },
	{ 0x0E, "shl" },
	{ 0x0F, "ldch" },
	{ 0x0F, "ldcl" },
	{ 0x0F, "ldc" },
};
int instructionNumber = sizeof(allInstr) / sizeof(instr);

char* rel_types[] = {
	"R_386_32",	"R_386_PC32"
};

char* rel_byte_choice[] = {
	"REL_W", "REL_NW", "REL_HW0", "REL_HW1", "REL_B0", "REL_B1", "REL_B2", "REL_B3"
};

char allCond[][3] = {
	{ "eq" },
	{ "ne" },
	{ "gt" },
	{ "ge" },
	{ "lt" },
	{ "le" },
	{ "" },
	{ "al" },
};
int condNumber = sizeof(allCond) / sizeof(char[3]);
int getInstructionByMnemonic(char * mnemonic) {
	int i = 0;
	for (; i < instructionNumber; i++) {
		if (strstr(mnemonic, allInstr[i].mnemonic) == mnemonic)
			return i;
	}
	return -1;
}

char getCondAndS(char * mnemonic, int instr) {
	char retVal = 0b00001100;
	char * condition = mnemonic + strlen(allInstr[instr].mnemonic);
	if (*condition == 0)
		return retVal;
	int len = strlen(condition);
	if (len > 3) {
		printf("Error: Greska kod instrukcije '%s'\n", mnemonic);
		abortAssembly();
	}
	if (len >= 2) {
		int i;
		for (i = 0; i < condNumber; i++) {
			if (i != 6) {
				if (strstr(condition, allCond[i]) == condition) {
					retVal = i << 1;
					break;
				}
			}
		}
		condition += 2;
		if (i == condNumber) {
			printf("Error: Pogresan mnemonik '%s' za uslov u instrukciji '%s'\n", condition, mnemonic);
			abortAssembly();
		}
	}
	if (*condition != 0) {
		if (*condition != 's') {
			printf("Error: Greska kod instrukcije '%s'\n", mnemonic);
			abortAssembly();
		}
		else {
			retVal |= 0b00000001;
		}
	}
	return retVal;
}

void makeSymbolGlobal(char * symbol) {
	int i;
	for (i = 0; i < symbolCounter; i++) {
		if (strcmp(sym_table[i]->name, symbol) == 0) {
			sym_table[i]->binding = GLOBAL;
			return;
		}
	}
}

int getSectionByName(char * readLine) {
	int i;
	for (i = 0; i < sectionCounter; i++) {
		if (strcmp(sym_table[section_table[i]->symbol_number]->name, readLine) == 0)
			return i;
	}
}

int getValueFromDirective(char * strval) {
	enum { DEC, BIN, HEX, CHR };
	int retVal = 0;
	int radix = DEC;
	int i = 0;
	if (*strval != '\'') {
		for (; i < strlen(strval); i++)
			if (strval[i] >= 'A' && strval[i] <= 'Z')
				strval[i] = strval[i] - 'A' + 'a';
	}

	if (*strval == '\'')
		radix = CHR;
	else if (strstr(strval, "0x") == strval)
		radix = HEX;
	else if (strstr(strval, "0b") == strval)
		radix = BIN;
	else
		radix = DEC;

	switch (radix) {
	case CHR: {
		if (strlen(strval) > 3) {
			printf("Error: Neuspesna konverzija '%s' u direktivi\n", strval);
			abortAssembly();
		}
		retVal = strval[1];
		break;
	}
	case HEX: {
		char * initValue = strval;
		strval += 2;
		while (*strval) {
			if (*strval >= '0' && *strval <= '9')
				retVal = (retVal << 4) | (*strval - '0');
			else if (*strval >= 'a' && *strval <= 'f')
				retVal = (retVal << 4) | (*strval - 'a' + 10);
			else {
				printf("Error: Greska pri konverziji '%s' u direktivi\n", initValue);
				abortAssembly();
			}
			strval++;
		}
		break;
	}
	case BIN: {
		char * initValue = strval;
		strval += 2;
		while (*strval) {
			if (*strval >= '0' && *strval <= '1')
				retVal = (retVal << 1) | (*strval - '0');
			else {
				printf("Error: Greska pri konverziji '%s' u direktivi\n", initValue);
				abortAssembly();
			}
			strval++;
		}
		break;
	}
	case DEC: {
		retVal = getImmediateOperand(strval);
		break;
	}
	}

	return retVal;
}

int isExpression(char * strval) {
	if (strchr(strval, '-') != 0 || strchr(strval, '+') != 0)
		return 1;
	return 0;
}

int isNumber(char * value) {
	while (*value) {
		if (*value >= '0' && *value <= '9') {
			value++;
			continue;		
		}
		return 0;
	}
	return 1;
}

int processExpression(char * strexpr) {
	char label[LABEL_MAX_LENGTH];
	char sign = 0;
	char oldSign = 0;
	int value = 0;

	while (*strexpr) {
		int i = 0;
		while (*strexpr && *strexpr != '+' && *strexpr != '-') {
			label[i++] = *strexpr;
			strexpr++;
		}
		if (*strexpr == '-')
			sign = 1;
		else if (*strexpr == '+')
			sign = 0;

		label[i] = 0;
		if (i == 0) {
			oldSign = sign;
			if (*strexpr != 0)
				strexpr++;
			continue;
		}
		int labValue = getSymbolFromTable(label);
		if (labValue == -1) {
			if (isNumber(label)) {
				value += atoi(label);
			}
			else {
				printf("Error: Nedefinisan simbol '%s' u direktivi '.long'\n", label);
				abortAssembly();
			}
		}
		else {
			if (sym_table[labValue]->binding == LOCAL) {
				if (oldSign) {
					value -= (sym_table[labValue]->offset);
				}
				else {
					value += (sym_table[labValue]->offset);
				}
				addRelocation(locationCounter, R_386_32, REL_W, sym_table[labValue]->section);
				//	printf("%d\t R_386_32\t%d\t%c\nValue in instr: %d\n", locationCounter, section_table[sym_table[labValue]->section]->symbol_number, oldSign ? '-' : '+', value);
			}
			else if (sym_table[labValue]->binding == GLOBAL) {
				addRelocation(locationCounter, R_386_32, oldSign ? REL_NW : REL_W, labValue);
				//	printf("%d\t R_386_32\t%d\t%c\nValue in instr: %d\n", locationCounter, labValue, oldSign ? '-' : '+', value);
			}
		}
		oldSign = sign;
		if (*strexpr != 0)
			strexpr++;
	}
	return value;
}

void addToCode(unsigned char * bytes) {
	int i;
	char * code = section_code[current_section]->code;
	code += locationCounter;

	for (i = 0; i < 4; i++) {
		memcpy(code + i - 4, bytes + i, sizeof(char));
	}
}

void processDirective(char * readLine) {
	int directive = getDirectiveFromName(readLine);
	while (*readLine && *readLine != ' ')
		readLine++;
	if (*readLine) readLine++;
	char * commaPtr = strchr(readLine, ',');
	while (commaPtr != 0) {
		*commaPtr = ' ';
		commaPtr = strchr(commaPtr + 1, ',');
	}
	switch (directive) {

	case DIR_CHAR: case DIR_WORD: {
		int counter = 0;
		unsigned char bytes[4] = { 0 };

		char * token = strtok(readLine, " ");
		while (token != 0) {
			int value = getValueFromDirective(token);

			if (directive == DIR_CHAR) {
				bytes[counter] = value & 0xFF;
				counter++;
				locationCounter += 1;
			}
			else if (directive == DIR_WORD) {
				bytes[counter++] = (value & 0xFF00) >> 8;
				bytes[counter++] = value & 0x00FF;
				locationCounter += 2;
			}
			if (counter % 4 == 0) {
				addToCode(bytes);
				counter = 0;
			}
			token = strtok(0, " ");
		}

		if (counter != 0) {
			int i;
			for (i = counter; i < 4; i++) {
				bytes[i] = 0;
			}
			locationCounter += 4 - counter;
			addToCode(bytes);
		}

		align();
		break;
	}

	case DIR_LONG: {
		int counter = 0;
		unsigned char bytes[4] = { 0 };
		char * token = strtok(readLine, " ");
		while (token != 0) {
			int value;
			if (directive == DIR_LONG && isExpression(token)) {
				value = processExpression(token);
			}
			else {
				value = getValueFromDirective(token);
			}
			bytes[0] = (value & 0xFF000000) >> 24;
			bytes[1] = (value & 0x00FF0000) >> 16;
			bytes[2] = (value & 0x0000FF00) >> 8;
			bytes[3] = value & 0x000000FF;
			locationCounter += 4;
			addToCode(bytes);

			token = strtok(0, " ");
		}

		break;
	}

	case DIR_ALIGN: {
		align();
		break;
	}
	case DIR_SKIP: {
		locationCounter += atoi(readLine);
		break;
	}
	case DIR_EXTERN: {

		return;
	}

	case DIR_PUBLIC: {
		char * token = strtok(readLine, " ");
		while (token) {
			makeSymbolGlobal(token);
			token = strtok(0, " ");
		}
		break;
	}
	case DIR_SECTION: {
		if (current_section != -1) {
			//	printf("New location counter: %d\nOld length: %d\n", locationCounter, section_table[current_section]->length);

		}
		locationCounter = 0;
		current_section = getSectionByName(readLine);
		section_code[current_section] = (code*)calloc(1, sizeof(code));
		section_code[current_section]->section = current_section;
		section_code[current_section]->code = (char*)calloc(section_table[current_section]->length, sizeof(char));

	}

	}

}

int getRegisterOperand(char * operand) {
	if (*operand == 'r') {
		operand++;
		int reg = 0;
		while (*operand) {
			if (*operand < '0' || *operand > '9')
				return -1;
			reg = reg * 10 + (*operand - '0');
			operand++;
		}
		if (reg >= 0 && reg <= 15)
			return reg;
	}
	else if (strcmp(operand, "pc") == 0)
		return REG_PC;
	else if (strcmp(operand, "lr") == 0)
		return REG_LR;
	else if (strcmp(operand, "sp") == 0)
		return REG_SP;
	else if (strcmp(operand, "psw") == 0)
		return REG_PSW;

	return -1;
}

int getImmediateOperand(char * operand) {
	int negative = 0;
	int retVal = 0;

	if (*operand == '-') {
		negative = 1;
		operand++;
	}

	while (*operand != 0) {
		retVal = retVal * 10 + (*operand - '0');
		operand++;
	}
	if (negative)
		retVal = -retVal;

	return retVal;

}

int getSymbolFromTable(char * symbol) {
	int i = 0;
	for (; i < symbolCounter; i++)
		if (strcmp(sym_table[i]->name, symbol) == 0) {
			return i;
		}
	return -1;
}

void addRelocation(int offset, int type, int bytes, int symbol) {
	if (rel_table == 0) {
		rel_table = (rel_entry**)calloc(100, sizeof(rel_entry*));
	}
	rel_entry* temp_rel_entry = (rel_entry*)calloc(1, sizeof(rel_entry));
	temp_rel_entry->offset = offset;
	temp_rel_entry->type = type;
	temp_rel_entry->bytes = bytes;
	temp_rel_entry->value = symbol;
	temp_rel_entry->section = current_section;
	rel_table[relocationCounter++] = temp_rel_entry;

}



void processInstruction(char * instruction) {
	unsigned char bytes[4] = { 0 };

	char * token = strtok(instruction, " ");
	int instr = getInstructionByMnemonic(token);
	if (instr == -1) {
		printf("Error: Nepostojeca instrukcija '%s'", token);
		abortAssembly();
	}
	char condAndS = getCondAndS(instruction, instr);
	bytes[0] |= condAndS << 4;
	bytes[0] |= allInstr[instr].opcode;

	switch (instr) {

	case INS_INT: {
		token = strtok(0, " ");
		if (token == 0 || *token != '#') {
			printf("Error: instrukcija int mora imati oblik 'int #imm'\n");
			abortAssembly();
		}
		else {
			token++;
			int intEntry = 0;
			while (*token) {
				if (*token < '0' || *token > '9') {
					printf("Error: Greska kod instrukcije '%s' zbog operanda\n", instruction);
					abortAssembly();
				}
				intEntry = intEntry * 10 + (*token - '0');
				token++;
			}
			if (intEntry > 15 || intEntry < 0) {
				printf("Error: Ulaz u IVT mora biti iz opsega [0, 15]\n");
				abortAssembly();
			}
			bytes[1] |= (intEntry << 4);
			addToCode(bytes);
		}
		break;
	}

	case INS_ADD: case INS_SUB: case INS_MUL: case INS_DIV: case INS_CMP: {
		int operand1, operand2;
		int immediate = 0;
		token = strtok(0, " ");
		if (token == 0) {
			printf("Error: Aritmeticke instrukcije moraju imati 2 operanda.\n");
			abortAssembly();
		}
		else {
			if (*token == '#') {
				printf("Error: Prvi operand aritmetickih instrukcija mora biti registar.\n");
				abortAssembly();
			}
			operand1 = getRegisterOperand(token);
			if (operand1 == -1) {
				printf("Error: Greska kod operanda '%s' u instrukciji '%s'\n", token, instruction);
				abortAssembly();
			}
		}

		token = strtok(0, " ");
		if (token == 0) {
			printf("Error: Aritmeticke instrukcije moraju imati 2 operanda.\n");
			abortAssembly();
		}
		else {
			if (*token == '#') {
				immediate = 1;
				token++;
				operand2 = getImmediateOperand(token);
			}
			else {
				immediate = 0;
				operand2 = getRegisterOperand(token);
				if (operand2 == -1) {
					printf("Error: Greska kod operanda '%s' u instrukciji '%s'\n", token, instruction);
					abortAssembly();
				}
			}
		}
		if ((!immediate && (operand2 == REG_PSW)) || (operand1 == REG_PSW)) {
			printf("Error: Registar PSW se ne moze koristiti uz aritmeticke operacije\n");
			abortAssembly();
		}
		if (((!immediate && (operand2 == REG_PC || operand2 == REG_LR || operand2 == REG_SP)) ||
			(operand1 == REG_PC || operand1 == REG_LR || operand1 == REG_SP)) &&
			(instr == INS_MUL || instr == INS_DIV || instr == INS_CMP)) {
			printf("Error: Registri PC, LR i SP se koriste samo uz add i sub\n");
			abortAssembly();
		}
		bytes[1] |= (operand1 << 3);
		bytes[1] |= (immediate << 2);
		if (immediate == 0) {
			bytes[1] |= ((operand2 >> 3) & 0x03);
			bytes[2] |= ((operand2 & 0b00000111) << 5);
		}
		else {
			bytes[1] |= ((operand2 >> 16) & 0x03);
			bytes[2] |= ((operand2 >> 8) & 0xFF);
			bytes[3] |= (operand2 & 0xFF);
		}

		addToCode(bytes);

		break;
	}

	case INS_AND:  case INS_OR:  case INS_NOT:  case INS_TEST: {
		int operand1, operand2;

		token = strtok(0, " ");
		if (token == 0) {
			printf("Error: Bitske instrukcije moraju imati 2 operanda, oba registra.\n");
			abortAssembly();
		}
		else {
			operand1 = getRegisterOperand(token);
			if (operand1 == -1) {
				printf("Error: Greska kod operanda '%s' u instrukciji '%s'\n", token, instruction);
				abortAssembly();
			}
		}
		token = strtok(0, " ");
		if (token == 0) {
			printf("Error: Bitske instrukcije moraju imati 2 operanda, oba registra.\n");
			abortAssembly();
		}
		else {
			operand2 = getRegisterOperand(token);
			if (operand2 == -1) {
				printf("Error: Greska kod operanda '%s' u instrukciji '%s'\n", token, instruction);
				abortAssembly();
			}
		}

		if (operand1 == REG_PC || operand1 == REG_LR || operand1 == REG_PSW ||
			operand2 == REG_PC || operand2 == REG_LR || operand2 == REG_PSW) {
			printf("Error: Registri PC, LR i PSW se ne mogu koristiti uz bitske instrukcije\n");
			abortAssembly();
		}

		bytes[1] |= (operand1 << 3);
		bytes[1] |= ((operand2 >> 2) & 0x07);
		bytes[2] |= ((operand2 & 0x03) << 6);

		addToCode(bytes);
		///// testiranje ispisa
		/*int i = 0;
		for (; i < 4; i++) {
			printf("%x%x\t", (bytes[i] >> 4) & 0x0F, bytes[i] & 0x0F);
		}
		printf("\n");*/


		break;
	}

	case INS_LDR: case INS_STR: {
		token = strtok(0, " ");
		int imm = 0;
		int addressing = 0;
		unsigned char f = 0;
		if (token == 0) {
			printf("Error: Nedostaju parametri instrukcije LDR/STR\n");
			abortAssembly();
		}
		int regDest = getRegisterOperand(token);
		int regSource;
		if (regDest == -1) {
			printf("Error: Greska kod destinacionog registra u LDR/STR\n");
			abortAssembly();
		}

		token = strtok(0, " ");
		if (token == 0) {
			printf("Error: Nedostaju parametri instrukcije LDR/STR\n");
			abortAssembly();
		}
		if (*token == '=') {
			addressing = 1;
			token++;
			int label = getSymbolFromTable(token);
			if (label == -1) {
				printf("Error: Nepostojeca labela '%s' u instrukciji LDR/STR\n", token);
				abortAssembly();
			}
			else if (label != -1 && sym_table[label]->section != current_section) {
				printf("Error: Labela '%s' ne pripada tekucoj sekciji\n", token);
				abortAssembly();
			}
			imm = sym_table[label]->offset - locationCounter;
			regSource = REG_PC;
		}
		if (addressing == 0) {
			regSource = getRegisterOperand(token);
			if (regSource == -1) {
				printf("Error: Greska kod izvornog registra u LDR/STR\n");
				abortAssembly();
			}
			if (regSource == REG_PSW) {
				printf("Error: PSW ne moze biti adresni registar u LDR/STR\n");
				abortAssembly();
			}

			token = strtok(0, " ");
			if (token != 0) {
				if (*token == '#') {
					token++;
					imm = getImmediateOperand(token);
					token = strtok(0, " ");
				}
			}
			if (token != 0) {
				if (strcmp(token, "ia") == 0)
					f = 2;
				else if (strcmp(token, "ib") == 0)
					f = 4;
				else if (strcmp(token, "da") == 0)
					f = 3;
				else if (strcmp(token, "db") == 0)
					f = 5;
				if (regSource == REG_PC && f != 0) {
					printf("Error: Ne moze se inkrementirati/dekrementirati PC u LDR/STR\n");
					abortAssembly();
				}
			}
		}

		bytes[1] |= ((regSource & 0b00011111) << 3);
		bytes[1] |= ((regDest >> 2) & 0b00000111);
		bytes[2] |= ((regDest & 0b00000011) << 6);
		bytes[2] |= ((f & 0b00000111) << 3);
		if (instr == INS_LDR)
			bytes[2] |= (1 << 2);
		bytes[2] |= ((imm >> 8) & 0b00000011);
		bytes[3] |= (imm & 0b11111111);

		addToCode(bytes);
		///// testiranje ispisa
		/*int i = 0;
		for (; i < 4; i++) {
		printf("%x%x\t", (bytes[i] >> 4) & 0x0F, bytes[i] & 0x0F);
		}
		printf("\n");*/


		break;
	}

	case INS_CALL: {
		token = strtok(0, " ");
		int imm = 0;
		if (token == 0) {
			printf("Error: Nedostaju parametri za 'call' instrukciju\n");
			abortAssembly();
		}
		int regDest = getRegisterOperand(token);
		if (regDest != -1) {
			token = strtok(0, " ");
			if (token && *token == '#') {
				token++;
				imm = getImmediateOperand(token);
			}
		}
		else {
			int label = getSymbolFromTable(token);
			if (label == -1) {
				printf("Error: Nepostojeca labela '%s' u instrukciji 'call'\n", token);
				abortAssembly();
			}
			else if (label != -1 && sym_table[label]->section != current_section) {
				printf("Error: Labela '%s' ne pripada tekucoj sekciji\n", token);
				abortAssembly();
			}
			imm = sym_table[label]->offset - locationCounter;
			regDest = REG_PC;
		}

		bytes[1] |= ((regDest & 0b00011111) << 3);
		bytes[1] |= ((imm >> 16) & 0b00000111);
		bytes[2] |= ((imm >> 8) & 0b11111111);
		bytes[3] |= (imm & 0b11111111);

		addToCode(bytes);
		///// testiranje ispisa
		/*int i = 0;
		for (; i < 4; i++) {
		printf("%x%x\t", (bytes[i] >> 4) & 0x0F, bytes[i] & 0x0F);
		}
		printf("\n");*/

		break;
	}

	case INS_IN: case INS_OUT: {
		token = strtok(0, " ");
		if (token == 0) {
			printf("Error: Nedostaju parametri za 'in/out' instrukciju\n");
			abortAssembly();
		}
		int regDest = getRegisterOperand(token);
		if (regDest < 0 || regDest > 15) {
			printf("Error: Destinacioni registar mora biti jedan od r0-r15\n");
			abortAssembly();
		}
		token = strtok(0, " ");
		if (token == 0) {
			printf("Error: Nedostaju parametri za 'in/out' instrukciju\n");
			abortAssembly();
		}
		int regSrc = getRegisterOperand(token);
		if (regSrc < 0 || regSrc > 15) {
			printf("Error: Izvorni registar mora biti jedan od r0-r15\n");
			abortAssembly();
		}

		bytes[1] |= ((regDest & 0x0F) << 4);
		bytes[1] |= (regSrc & 0x0F);
		if (instr == INS_IN) {
			bytes[2] |= (1 << 7);
		}

		addToCode(bytes);
		///// testiranje ispisa
		/*int i = 0;
		for (; i < 4; i++) {
		printf("%x%x\t", (bytes[i] >> 4) & 0x0F, bytes[i] & 0x0F);
		}
		printf("\n");*/

		break;
	}

	case INS_MOV: case INS_SHR: case INS_SHL: {
		int regDest, regSrc, imm;
		imm = 0;

		token = strtok(0, " ");
		if (token == 0) {
			printf("Error: Nedostaju parametri za 'mov/shr/shl'\n");
			abortAssembly();
		}
		regDest = getRegisterOperand(token);
		if (regDest == -1) {
			printf("Error: Nepostojeci registar '%s' u instrukciji '%s'\n", token, instruction);
			abortAssembly();
		}

		token = strtok(0, " ");
		if (token == 0) {
			printf("Error: Nedostaju parametri za 'mov/shr/shl'\n");
			abortAssembly();
		}
		regSrc = getRegisterOperand(token);
		if (regSrc == -1) {
			printf("Error: Nepostojeci registar '%s' u instrukciji '%s'\n", token, instruction);
			abortAssembly();
		}

		if (instr == INS_SHR || instr == INS_SHL) {
			token = strtok(0, " ");
			if (token == 0 || (token != 0 && *token != '#')) {
				printf("Error: Nedostaje neposredni operand za 'shr/shl'\n");
				abortAssembly();
			}
			token++;
			imm = getImmediateOperand(token);
		}

		bytes[1] |= ((regDest & 0b00011111) << 3);
		bytes[1] |= ((regSrc >> 2) & 0b00000111);
		bytes[2] |= ((regSrc & 0b00000011) << 6);
		bytes[2] |= ((imm & 0b00011111) << 1);
		if (instr == INS_SHL)
			bytes[2] |= 1;

		addToCode(bytes);
		///// testiranje ispisa
		/*int i = 0;
		for (; i < 4; i++) {
			printf("%x%x\t", (bytes[i] >> 4) & 0x0F, bytes[i] & 0x0F);
		}
		printf("\n");*/

		break;
	}

	case INS_LDCH: case INS_LDCL: case INS_LDC: {
		token = strtok(0, " ");
		int regDst = getRegisterOperand(token);
		if (regDst < 0 || regDst > 15) {
			printf("Error: Destinacioni registar u instrukciji 'ldc' mora biti jedan od r0-r15\n");
			abortAssembly();
		}
		bytes[1] |= ((regDst & 0b00001111) << 4);

		token = strtok(0, " ");
		if (token == 0 || (token != 0 && *token != '=')) {
			printf("Error: Drugi operand u instrukciji 'ldc' je oblika '=[symbol]'\n");
			abortAssembly();
		}
		token++;
		int symbol = getSymbolFromTable(token);
		if (symbol == -1) {
			printf("Error: Simbol '%s' se ne nalazi u tabeli simbola\n", token);
			abortAssembly();
		}
		int bytesRel = REL_W;

		if (instr == INS_LDCL) {
			bytesRel = REL_HW1;
			if (sym_table[symbol]->binding == LOCAL) {
				bytes[2] |= ((sym_table[symbol]->offset >> 8) & 0xFF);
				bytes[3] |= (sym_table[symbol]->offset & 0xFF);
				addRelocation(locationCounter - 2, R_386_32, bytesRel, sym_table[symbol]->section);
			}
			else {
				bytes[2] = 0;
				bytes[3] = 0;
				addRelocation(locationCounter - 2, R_386_32, bytesRel, symbol);
			}
			addToCode(bytes);
		}
		else if (instr == INS_LDCH) {
			bytes[1] |= 0b00001000;
			bytesRel = REL_HW0;
			addRelocation(locationCounter - 2, R_386_32, bytesRel, symbol);
			addToCode(bytes);
		}
		else if (instr == INS_LDC) {
			bytesRel = REL_HW1;
			addRelocation(locationCounter - 2, R_386_32, bytesRel, symbol);
			addToCode(bytes);

			locationCounter += 4;
			bytes[1] |= 0b00001000;
			bytesRel = REL_HW0;
			addRelocation(locationCounter - 2, R_386_32, bytesRel, symbol);
			addToCode(bytes);
		}
		break;
	}
	}
}

void secondPass(FILE * f) {
	char readLine[LINE_MAX_LENGTH];
	int lineLen;
	current_section = -1;
	section_code = (code**)calloc(sectionCounter, sizeof(code*));

	do {
		readLine[0] = 0;
		fgets(readLine, LINE_MAX_LENGTH, f);
		lineLen = strlen(readLine);
		if (readLine[lineLen - 1] == '\n')
			readLine[lineLen - 1] = '\0';
		if (strcmp(DIRECTIVE_END, readLine) == 0) {
			///zavrsetak fajla
			break;
		}
		if (lineLen == 1) continue;

		char * directiveLine = strchr(readLine, '.');
		if (directiveLine != 0) {
			processDirective(directiveLine);
		}
		else {
			char * instr;
			instr = strchr(readLine, ':');
			if (instr != 0) {
				instr++;
				while (*instr && *instr == ' ')
					instr++;
			}
			else {
				instr = readLine;
			}

			if (*instr != 0) {
				locationCounter += 4;
				processInstruction(instr);
			}
		}
	} while (strcmp(DIRECTIVE_END, readLine) != 0);


}
