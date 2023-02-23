/*
generated at Thu May 28 10:46:34 2020
by $Id: pburg.c,v 2.5 2017/11/16 09:41:42 prs Exp $
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PBURG_PREFIX "yy"
#define PBURG_VERSION "2.5"
#define MAX_COST 0x7fff
#if defined(__STDC__) || defined(__cplusplus)
#define YYCONST const
#else
#define YYCONST
#endif

#line 1 "minor.brg"

/*
 * selecção de instruções com postfix
 */
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "node.h"
#include "postfix.h"
#include "y.tab.h"
#include "lib/tabid.h"
#include "minor.h"

#define ASM(x, ...) fprintf(yyout, x, ##__VA_ARGS__)

#ifndef U_
#define U_ "_"
#endif

#define TRACE
static void yytrace(NODEPTR_TYPE p, int eruleno, int cost, int bestcost);

extern FILE *yyout;
extern char **yynames;

int lbl;
static char *mklbl(int n) {
	static char buf[20];
	sprintf(buf, "%cL%d", n < 0 ? '.' : '_', n);
	return strcpy(malloc(strlen(buf)+1),buf);
}
static char *mkfunc(char *s) {
	static char buf[80];
	strcpy(buf, "_");
	strcat(buf, s);
	return buf;
}

static void outstr(char *s) {
  while (*s) fprintf(yyout, pfCHAR, (unsigned char)*s++);
  fprintf(yyout, pfCHAR, 0);
}

#define LARGE_INT 32768

char assertStr[50];

char* fName;

int currentArraySize;
int* currentArray;

int nForwardNames;
char** forwardNames;

int numLocalArrays;
int* localArrays;

int numForLabels;
int* forLabels;
int* forIncrLabels;
int* forAfterLabels;

typedef struct {
	int retType;
	char* name;
	char flags;
	int paramsSize;
} Function;

typedef struct {
	int type;
	char flags;
	int offset;
} Variable;

static int power(int base, int exponent) {
	int result = 1;
	while (exponent --) result *= base;
	return result;
}

static void printExpr(Node* exprNode) {
	switch(exprNode->info) {
		case NUMBER:
			ASM(pfEXTRN pfCALL pfTRASH, "_printi", "_printi", 4);
			break;
		case STRING:
			ASM(pfEXTRN pfCALL pfTRASH, "_prints", "_prints", pfWORD);
			break;
	}
}

static int getSize(int type) {
	return type == NUMBER ? 4 : pfWORD;
}

static void parseID(Node* idNode, char isIndexed) {
	char* id = idNode->value.s;
	void* data;
	int result = IDfind(id, &data);

	if (result == FUNCTION) { /* function with no arguments */
		Function* func = (Function*) data;
		ASM(pfCALL, mkfunc(idNode->value.s));
		if (func->retType != NIL) /* does not return void */
			ASM(pfPUSH);
		if (isIndexed) ASM(pfLOAD);
	}
	else {
		Variable* var = (Variable*) data;
		if (isIndexed) {
			if (var->flags & F_GLOBAL)
				ASM(pfADDRV, id);
			else
				ASM(pfLOCV, var->offset);
		}
		else {
			if (var->flags & F_GLOBAL)
				ASM(pfADDR, id);
			else
				ASM(pfLOCAL, var->offset);
		}
		
	}

	
}

static void loadID(Node* idNode) {
	char* id = idNode->value.s;
	void* data;
	int result = IDfind(id, &data);

	if (result == FUNCTION) { /* function with no arguments */
		Function* func = (Function*) data;
		ASM(pfCALL, mkfunc(idNode->value.s));
		if (func->retType != NIL) /* does not return void */
			ASM(pfPUSH);
	}
	else {
		Variable* var = (Variable*) data;
				
		if (var->flags & F_GLOBAL)
			ASM(pfADDRV, id);
		else
			ASM(pfLOCV, var->offset);
	}
}

static void storeID(Node* idNode) {
	char* id = idNode->value.s;
	void* data;
	int result = IDfind(id, &data);

	Variable* var = (Variable*) data;
			
	if (var->flags & F_GLOBAL)
		ASM(pfADDRA, id);
	else
		ASM(pfLOCA, var->offset);
}

static void parseIndex(Node*p) {
	char* name = LEFT_CHILD(p)->value.s;
	void* data;
	int result = IDfind(name, &data);

	if (result == FUNCTION) {
		Function* func = (Function*) data;
		result = func->retType;
	}
	if (result == STRING)
		ASM(pfADD);
	else /* array */
		ASM(pfIMM pfMUL pfADD, 4);
}

void storeIndex(Node* idNode) {
	char* name = idNode->value.s;
	void* data;
	int result = IDfind(name, &data);

	if (result == FUNCTION) {
		Function* func = (Function*) data;
		result = func->retType;
	}

	if (result == STRING)
		ASM(pfSTCHR);
	else /* array */
		ASM(pfSTORE);
}

void loadIndex(Node* p) {
	char* name = LEFT_CHILD(p)->value.s;
	void* data;
	int result = IDfind(name, &data);

	if (result == FUNCTION) {
		Function* func = (Function*) data;
		result = func->retType;
	}

	if (result == STRING)
		ASM(pfADD pfLDCHR);
	else /* array */
		ASM(pfIMM pfMUL pfADD pfLOAD, 4);
}

static void addLocalArray(int offset) {
	if (numLocalArrays++ == 0)
		localArrays = (int*) malloc(sizeof(int));
	else
		localArrays = (int*) realloc(localArrays, numLocalArrays * sizeof(int));
	localArrays[numLocalArrays - 1] = offset;
}

static void resetLocalArrays() {
	if (numLocalArrays > 0) 
		free(localArrays);
	numLocalArrays = 0;
}

static void initLocalArrays() {
	for (int i = 0; i < numLocalArrays; i ++)
		ASM(pfLOCAL pfLOCAL pfSTORE, localArrays[i] + 4, localArrays[i]);
}

static void pushForLabels(int forLabel, int forIncrLabel, int forAfterLabel) {
	numForLabels ++;
	if (forLabels == 0) {
		forLabels = (int*) malloc(sizeof(int));
		forIncrLabels = (int*) malloc(sizeof(int));
		forAfterLabels = (int*) malloc(sizeof(int));
	}
	else {
		forLabels = (int*) realloc(forLabels, numForLabels * sizeof(int));
		forIncrLabels = (int*) realloc(forIncrLabels, numForLabels * sizeof(int));
		forAfterLabels = (int*) realloc(forAfterLabels, numForLabels * sizeof(int));
	}
	forLabels[numForLabels - 1] = forLabel;
	forIncrLabels[numForLabels - 1] = forIncrLabel;	
	forAfterLabels[numForLabels - 1] = forAfterLabel;	
}

static void popForLabel() {
	numForLabels --;
}

static char* forLabel() {
	return mklbl(forLabels[numForLabels - 1]);
}

static char* forIncrLabel() {
	return mklbl(forIncrLabels[numForLabels - 1]);
}

static char* forAfterLabel() {
	return mklbl(forAfterLabels[numForLabels - 1]);
}

static void addForward(char* name) {
	nForwardNames ++;
	if (forwardNames == 0)
		forwardNames = (char**) malloc(sizeof(char*));
	else
		forwardNames = (char**) realloc(forwardNames, nForwardNames * sizeof(char*));
	forwardNames[nForwardNames - 1] = name;
}

static void newVar(char* name, int type, char flags, int offset) {
	Variable* var = (Variable*) malloc(sizeof(Variable));
	var->type = type;
	var->flags = flags;
	var->offset = offset;
	if (IDfind(name, (void**) IDtest) == -1) {
		IDnew(type, strdup(name), var);
	}
	else
		IDreplace(type, strdup(name), var);

	if (flags & F_FORWARD)
		addForward(strdup(name));
}

static void globalVar(Node* n) {
	int type = LEFT_CHILD(LEFT_CHILD(n))->info;
	int initType = RIGHT_CHILD(n)->info;
	char flags = n->info;
	char* name = LEFT_CHILD(RIGHT_CHILD(LEFT_CHILD(n)))->value.s;
	int size = RIGHT_CHILD(RIGHT_CHILD(LEFT_CHILD(n)))->place;
	newVar(name, type, F_GLOBAL | flags, 0);
	
	if (!(flags & F_FORWARD)) {
		if (flags & F_PUBLIC)
			ASM(pfGLOBL, name, pfOBJ);
		if (initType == NIL) {
			if (type != ARRAY)
				ASM(pfBSS pfALIGN pfLABEL, name);
			switch(type) {
				case NUMBER: ASM(pfBYTE, 4); break;
				case STRING: ASM(pfBYTE, pfWORD); break;
				case ARRAY:
					if (size == 0)
						ASM(pfBSS pfALIGN pfLABEL pfBYTE, name, pfWORD);
					else {
						if (flags & F_CONST) ASM(pfRODATA);
						else ASM(pfDATA);
						lbl ++;
						ASM(pfALIGN pfLABEL pfID pfLABEL, 
							name, mklbl(lbl), mklbl(lbl));
						for (int i = 0; i < size; i ++)
							ASM(pfINTEGER, 0);
					}
					break;
			}
		}
		else {
			if (flags & F_CONST) ASM(pfRODATA);
			else ASM(pfDATA);
			ASM(pfALIGN pfLABEL, name);
			switch(type) {
				case NUMBER: 
					ASM(pfINTEGER, LEFT_CHILD(RIGHT_CHILD(n))->value.i);
					break;
				case STRING: 
					ASM(pfID, mklbl(++lbl)); /* pointer value */
					if (flags & F_CONST) ASM(pfDATA pfALIGN); /* switch back to data */
					ASM(pfLABEL, mklbl(lbl));
					outstr(LEFT_CHILD(RIGHT_CHILD(n))->value.s); /* string value */
					break;
				case ARRAY:
					ASM(pfID, mklbl(++lbl)); /* pointer value */
					if (flags & F_CONST) ASM(pfDATA pfALIGN); /* switch back to data */
					ASM(pfLABEL, mklbl(lbl));
					int i = 0;
					for (; i < currentArraySize; i ++)
						ASM(pfINTEGER, currentArray[i]);
					for (; i < size; i ++)
						ASM(pfINTEGER, 0);
					break;
			}
		}
	}
}

static void newFunc(char* name, char flags) {
	Function* func = (Function*) malloc(sizeof(Function));
	fName = strdup(name);
	func->name = strdup(name);
	func->flags = flags;

	if (IDfind(name, (void**) IDtest) == -1) {
		IDnew(FUNCTION, strdup(name), func);
	}
	else {
		IDreplace(FUNCTION, strdup(name), func);
	}
	if (flags & F_FORWARD) {
		addForward(strdup(name));
	}
}

static void enterFunction(Node* n) {
	if (fName) {
		void* data;
		IDfind(fName, &data);
		Function* func = (Function*) data;

		ASM(pfTEXT pfALIGN pfLABEL, fName);
	}
	ASM(pfENTER, (int) n->place); 
	initLocalArrays();	
}

static void setParamsSize(char* name, int size) {
	void* data;
	IDfind(name, &data);
	((Function*) data)->paramsSize = size;
}

static void addExterns() {
	for (int i = 0; i < nForwardNames; i ++) {
		void* data;
		if (IDfind(forwardNames[i], &data) == FUNCTION) {
			Function* func = (Function*) data;
			if (func->flags & F_FORWARD)
				ASM(pfEXTRN, forwardNames[i]);
		}
		else {
			Variable* var = (Variable*) data;
			if (var->flags & F_FORWARD)
				ASM(pfEXTRN, forwardNames[i]);
		}
	}
}

static char idIsGlobal(char* id, void** data) {
	int result = IDfind(id, data);
	if (result == FUNCTION)
		return ((Function*) *data)->flags & F_GLOBAL;
	else
		return ((Variable*) *data)->flags & F_GLOBAL;
}

static void incdec(Node* idNode, Node* valueNode, char op) {
	/* get address */
	char* srcName = idNode->value.s;
	Variable* srcVar;
	if (idIsGlobal(srcName, (void**) &srcVar))
		ASM(pfADDR, srcName);
	else
		ASM(pfLOCAL, srcVar->offset);

	/* do operation */
	int value = valueNode->value.i;
	if (op == '+')
		ASM(pfINCR, value);
	else /* - */
		ASM(pfDECR, value);
}

static int isNumberLeft(Node* n) {
	return LEFT_CHILD(n)->info == NUMBER ? 1 : LARGE_INT;
}

static int isStrLeft(Node* n) {
	return LEFT_CHILD(n)->info == STRING ? 1 : LARGE_INT;
}

static int isNilLeft(Node* n) {
	return LEFT_CHILD(n)->info == NIL ? 1 : LARGE_INT;
}

static int isStrArrLeft(Node* n) {
	return (LEFT_CHILD(n)->info == STRING || LEFT_CHILD(n)->info == ARRAY) ? 1 : LARGE_INT;
}

static int isStrRight(Node* n) {
	return RIGHT_CHILD(n)->info == STRING ? 1 : LARGE_INT;
}

static int isArrRight(Node* n) {
	return RIGHT_CHILD(n)->info == ARRAY ? 1 : LARGE_INT;
}

static int isArrAndNum(Node* n) {
	return (LEFT_CHILD(n)->info == ARRAY && RIGHT_CHILD(n)->info == NUMBER) ? 1 : LARGE_INT;
}

static int isNumAndArr(Node* n) {
	return (LEFT_CHILD(n)->info == NUMBER && RIGHT_CHILD(n)->info == ARRAY) ? 1 : LARGE_INT;
}

static int isArrAndArr(Node* n) {
	return (LEFT_CHILD(n)->info == ARRAY && RIGHT_CHILD(n)->info == ARRAY) ? 1 : LARGE_INT;
}

static int isNumAndNum(Node* n) {
	return (LEFT_CHILD(n)->info == NUMBER && RIGHT_CHILD(n)->info == NUMBER) ? 1 : LARGE_INT;
}

static int sameNameLLR(Node* n) {
	return strcmp(LEFT_CHILD(LEFT_CHILD(n))->value.s, RIGHT_CHILD(n)->value.s) == 0 ? 1 : LARGE_INT;
}

static int sameNameRLR(Node* n) {
	return strcmp(RIGHT_CHILD(LEFT_CHILD(n))->value.s, RIGHT_CHILD(n)->value.s) == 0 ? 1 : LARGE_INT;
}

#ifndef PANIC
#define PANIC yypanic
static void yypanic(YYCONST char *rot, YYCONST char *msg, int val) {
	fprintf(stderr, "Internal Error in %s: %s %d\nexiting...\n",
		rot, msg, val);
	exit(2);
}
#endif
static void yykids(NODEPTR_TYPE, int, NODEPTR_TYPE[]);
#define yyfile_NT 1
#define yydecls_prog_NT 2
#define yybody_NT 3
#define yydecls_NT 4
#define yydecl_NT 5
#define yytype_NT 6
#define yyf_id_NT 7
#define yysize_NT 8
#define yyinit_NT 9
#define yyarray_NT 10
#define yyf_id_do_NT 11
#define yyparams_end_NT 12
#define yyf_id_done_NT 13
#define yyparams_NT 14
#define yyvars_end_NT 15
#define yyblock_NT 16
#define yyvars_NT 17
#define yyinstrs_NT 18
#define yyendinstr_NT 19
#define yyinstr_NT 20
#define yyelifs_NT 21
#define yyelifs_last_NT 22
#define yyfor_init_NT 23
#define yyuntil_NT 24
#define yyexpr_NT 25
#define yyalloc_size_str_NT 26
#define yylval_NT 27
#define yyalloc_size_arr_NT 28
#define yycond_NT 29
#define yyuntil_expr_NT 30
#define yystep_NT 31
#define yystep_block_NT 32
#define yyid_idx_NT 33
#define yyargs_NT 34
#define yyexprX4_NT 35
#define yyand_NT 36
#define yyor_NT 37
#define yyassign_value_NT 38
#define yynum_NT 39
#define yyincdecInstr_NT 40
#define yynumConst_NT 41

static YYCONST char *yyntname[] = {
	0,
	"file",
	"decls_prog",
	"body",
	"decls",
	"decl",
	"type",
	"f_id",
	"size",
	"init",
	"array",
	"f_id_do",
	"params_end",
	"f_id_done",
	"params",
	"vars_end",
	"block",
	"vars",
	"instrs",
	"endinstr",
	"instr",
	"elifs",
	"elifs_last",
	"for_init",
	"until",
	"expr",
	"alloc_size_str",
	"lval",
	"alloc_size_arr",
	"cond",
	"until_expr",
	"step",
	"step_block",
	"id_idx",
	"args",
	"exprX4",
	"and",
	"or",
	"assign_value",
	"num",
	"incdecInstr",
	"numConst",
	0
};

static YYCONST char *yytermname[] = {
 "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
 "", "", "", "", "", "", "", "", "",
	/* 34 */ "PRINT",
 "",
	/* 36 */ "ALLOC",
 "",
	/* 38 */ "MOD",
	/* 39 */ "AND",
 "",
	/* 41 */ "CALL",
 "",
	/* 43 */ "MUL",
	/* 44 */ "ADD",
	/* 45 */ "ELEM",
	/* 46 */ "SUB",
 "",
	/* 48 */ "DIV",
 "", "", "", "", "", "", "", "", "", "", "",
	/* 60 */ "EXPR",
	/* 61 */ "LT",
	/* 62 */ "EQ",
	/* 63 */ "GT",
	/* 64 */ "INPUT",
 "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
 "", "", "",
	/* 92 */ "INDEX",
 "", "",
	/* 95 */ "POW",
 "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
 "", "", "", "", "",
	/* 125 */ "OR",
 "",
	/* 127 */ "NOT",
 "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
 "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
 "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
 "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
 "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
 "", "", "", "", "", "", "", "", "", "",
	/* 258 */ "PROGRAM",
	/* 259 */ "MODULE",
	/* 260 */ "START",
	/* 261 */ "END",
	/* 262 */ "VOID",
	/* 263 */ "CONST",
	/* 264 */ "NUMBER",
	/* 265 */ "ARRAY",
	/* 266 */ "STRING",
	/* 267 */ "FUNCTION",
	/* 268 */ "PUBLIC",
	/* 269 */ "FORWARD",
	/* 270 */ "IF",
	/* 271 */ "THEN",
	/* 272 */ "ELSE",
	/* 273 */ "ELIF",
	/* 274 */ "FI",
	/* 275 */ "FOR",
	/* 276 */ "UNTIL",
	/* 277 */ "STEP",
	/* 278 */ "DO",
	/* 279 */ "DONE",
	/* 280 */ "REPEAT",
	/* 281 */ "STOP",
	/* 282 */ "RETURN",
	/* 283 */ "ASSIGN",
	/* 284 */ "LEQ",
	/* 285 */ "GEQ",
	/* 286 */ "NEQ",
	/* 287 */ "BOR",
	/* 288 */ "ASSERT",
	/* 289 */ "NUMLIT",
	/* 290 */ "CHRLIT",
	/* 291 */ "TXTLIT",
	/* 292 */ "ID",
	/* 293 */ "UMINUS",
	/* 294 */ "ADDR",
	/* 295 */ "NIL",
	/* 296 */ "ERR",
	/* 297 */ "DECLS",
	/* 298 */ "SIZE",
	/* 299 */ "INIT",
	/* 300 */ "PARAM",
	/* 301 */ "ARG",
	/* 302 */ "INSTR",
	/* 303 */ "BLOCK",
	/* 304 */ "BODY",
	/* 305 */ "VARS",
	/* 306 */ "VAR",
	/* 307 */ "F_RET",
	/* 308 */ "F_ID",
	/* 309 */ "F_PARAMS",
	/* 310 */ "V_TYPE",
	/* 311 */ "V_ID",
	/* 312 */ "ELIFS",
	/* 313 */ "ELIF_COND",
	""
};

struct yystate {
	short cost[42];
	struct {
		unsigned int yyfile:2;
		unsigned int yydecls_prog:1;
		unsigned int yybody:1;
		unsigned int yydecls:2;
		unsigned int yydecl:2;
		unsigned int yytype:3;
		unsigned int yyf_id:2;
		unsigned int yysize:2;
		unsigned int yyinit:3;
		unsigned int yyarray:2;
		unsigned int yyf_id_do:1;
		unsigned int yyparams_end:1;
		unsigned int yyf_id_done:1;
		unsigned int yyparams:2;
		unsigned int yyvars_end:1;
		unsigned int yyblock:1;
		unsigned int yyvars:2;
		unsigned int yyinstrs:2;
		unsigned int yyendinstr:3;
		unsigned int yyinstr:4;
		unsigned int yyelifs:2;
		unsigned int yyelifs_last:2;
		unsigned int yyfor_init:1;
		unsigned int yyuntil:1;
		unsigned int yyexpr:6;
		unsigned int yyalloc_size_str:1;
		unsigned int yylval:2;
		unsigned int yyalloc_size_arr:1;
		unsigned int yycond:3;
		unsigned int yyuntil_expr:1;
		unsigned int yystep:1;
		unsigned int yystep_block:1;
		unsigned int yyid_idx:1;
		unsigned int yyargs:2;
		unsigned int yyexprX4:1;
		unsigned int yyand:1;
		unsigned int yyor:1;
		unsigned int yyassign_value:1;
		unsigned int yynum:2;
		unsigned int yyincdecInstr:3;
		unsigned int yynumConst:5;
	} rule;
};

static short yynts_0[] = { yydecls_prog_NT, yybody_NT, 0 };
static short yynts_1[] = { yydecls_NT, 0 };
static short yynts_2[] = { yydecls_NT, yydecl_NT, 0 };
static short yynts_3[] = { 0 };
static short yynts_4[] = { yytype_NT, yyf_id_NT, 0 };
static short yynts_5[] = { yytype_NT, yysize_NT, yyinit_NT, 0 };
static short yynts_6[] = { yyarray_NT, 0 };
static short yynts_7[] = { yyf_id_do_NT, yyparams_end_NT, yybody_NT, 0 };
static short yynts_8[] = { yyf_id_done_NT, yyparams_end_NT, 0 };
static short yynts_9[] = { yyparams_NT, 0 };
static short yynts_10[] = { yyparams_NT, yytype_NT, yysize_NT, 0 };
static short yynts_11[] = { yyvars_end_NT, yyblock_NT, 0 };
static short yynts_12[] = { yyvars_NT, 0 };
static short yynts_13[] = { yyvars_NT, yytype_NT, yysize_NT, 0 };
static short yynts_14[] = { yyinstrs_NT, yyendinstr_NT, 0 };
static short yynts_15[] = { yyinstrs_NT, yyinstr_NT, 0 };
static short yynts_16[] = { yyelifs_NT, yyblock_NT, 0 };
static short yynts_17[] = { yyelifs_last_NT, 0 };
static short yynts_18[] = { yyfor_init_NT, yyuntil_NT, 0 };
static short yynts_19[] = { yyexpr_NT, 0 };
static short yynts_20[] = { yyalloc_size_str_NT, yylval_NT, 0 };
static short yynts_21[] = { yyalloc_size_arr_NT, yylval_NT, 0 };
static short yynts_22[] = { yycond_NT, yyblock_NT, 0 };
static short yynts_23[] = { yyelifs_NT, yycond_NT, yyblock_NT, 0 };
static short yynts_24[] = { yyuntil_expr_NT, yystep_NT, 0 };
static short yynts_25[] = { yystep_block_NT, yyexpr_NT, 0 };
static short yynts_26[] = { yyblock_NT, 0 };
static short yynts_27[] = { yyid_idx_NT, yyexpr_NT, 0 };
static short yynts_28[] = { yyargs_NT, 0 };
static short yynts_29[] = { yylval_NT, 0 };
static short yynts_30[] = { yyexpr_NT, yyexpr_NT, 0 };
static short yynts_31[] = { yyexprX4_NT, yyexpr_NT, 0 };
static short yynts_32[] = { yyexpr_NT, yyexprX4_NT, 0 };
static short yynts_33[] = { yyand_NT, yyexpr_NT, 0 };
static short yynts_34[] = { yyor_NT, yyexpr_NT, 0 };
static short yynts_35[] = { yyassign_value_NT, yyid_idx_NT, yyexpr_NT, 0 };
static short yynts_36[] = { yyassign_value_NT, yylval_NT, 0 };
static short yynts_37[] = { yyexpr_NT, yyargs_NT, 0 };
static short yynts_38[] = { yyincdecInstr_NT, 0 };
static short yynts_39[] = { yynum_NT, 0 };
static short yynts_40[] = { yynumConst_NT, 0 };
static short yynts_41[] = { yynumConst_NT, yynumConst_NT, 0 };

static short *yynts[] = {
	0,	/* 0 */
	yynts_0,	/* 1 */
	yynts_1,	/* 2 */
	yynts_1,	/* 3 */
	yynts_2,	/* 4 */
	yynts_3,	/* 5 */
	yynts_4,	/* 6 */
	yynts_5,	/* 7 */
	yynts_3,	/* 8 */
	yynts_3,	/* 9 */
	yynts_3,	/* 10 */
	yynts_3,	/* 11 */
	yynts_3,	/* 12 */
	yynts_3,	/* 13 */
	yynts_3,	/* 14 */
	yynts_3,	/* 15 */
	yynts_3,	/* 16 */
	yynts_6,	/* 17 */
	yynts_3,	/* 18 */
	yynts_3,	/* 19 */
	yynts_3,	/* 20 */
	yynts_6,	/* 21 */
	yynts_7,	/* 22 */
	yynts_8,	/* 23 */
	yynts_3,	/* 24 */
	yynts_3,	/* 25 */
	yynts_9,	/* 26 */
	yynts_10,	/* 27 */
	yynts_3,	/* 28 */
	yynts_11,	/* 29 */
	yynts_12,	/* 30 */
	yynts_13,	/* 31 */
	yynts_3,	/* 32 */
	yynts_14,	/* 33 */
	yynts_15,	/* 34 */
	yynts_3,	/* 35 */
	yynts_16,	/* 36 */
	yynts_17,	/* 37 */
	yynts_18,	/* 38 */
	yynts_19,	/* 39 */
	yynts_19,	/* 40 */
	yynts_19,	/* 41 */
	yynts_19,	/* 42 */
	yynts_20,	/* 43 */
	yynts_21,	/* 44 */
	yynts_19,	/* 45 */
	yynts_19,	/* 46 */
	yynts_19,	/* 47 */
	yynts_22,	/* 48 */
	yynts_23,	/* 49 */
	yynts_22,	/* 50 */
	yynts_23,	/* 51 */
	yynts_19,	/* 52 */
	yynts_19,	/* 53 */
	yynts_24,	/* 54 */
	yynts_19,	/* 55 */
	yynts_25,	/* 56 */
	yynts_26,	/* 57 */
	yynts_3,	/* 58 */
	yynts_3,	/* 59 */
	yynts_3,	/* 60 */
	yynts_19,	/* 61 */
	yynts_3,	/* 62 */
	yynts_3,	/* 63 */
	yynts_27,	/* 64 */
	yynts_3,	/* 65 */
	yynts_27,	/* 66 */
	yynts_3,	/* 67 */
	yynts_3,	/* 68 */
	yynts_3,	/* 69 */
	yynts_3,	/* 70 */
	yynts_3,	/* 71 */
	yynts_28,	/* 72 */
	yynts_29,	/* 73 */
	yynts_19,	/* 74 */
	yynts_30,	/* 75 */
	yynts_30,	/* 76 */
	yynts_30,	/* 77 */
	yynts_30,	/* 78 */
	yynts_30,	/* 79 */
	yynts_31,	/* 80 */
	yynts_32,	/* 81 */
	yynts_30,	/* 82 */
	yynts_31,	/* 83 */
	yynts_32,	/* 84 */
	yynts_30,	/* 85 */
	yynts_19,	/* 86 */
	yynts_30,	/* 87 */
	yynts_30,	/* 88 */
	yynts_30,	/* 89 */
	yynts_30,	/* 90 */
	yynts_30,	/* 91 */
	yynts_30,	/* 92 */
	yynts_30,	/* 93 */
	yynts_30,	/* 94 */
	yynts_30,	/* 95 */
	yynts_30,	/* 96 */
	yynts_30,	/* 97 */
	yynts_30,	/* 98 */
	yynts_30,	/* 99 */
	yynts_19,	/* 100 */
	yynts_33,	/* 101 */
	yynts_34,	/* 102 */
	yynts_35,	/* 103 */
	yynts_36,	/* 104 */
	yynts_19,	/* 105 */
	yynts_19,	/* 106 */
	yynts_19,	/* 107 */
	yynts_37,	/* 108 */
	yynts_3,	/* 109 */
	yynts_3,	/* 110 */
	yynts_3,	/* 111 */
	yynts_3,	/* 112 */
	yynts_19,	/* 113 */
	yynts_38,	/* 114 */
	yynts_39,	/* 115 */
	yynts_39,	/* 116 */
	yynts_39,	/* 117 */
	yynts_39,	/* 118 */
	yynts_30,	/* 119 */
	yynts_30,	/* 120 */
	yynts_30,	/* 121 */
	yynts_30,	/* 122 */
	yynts_30,	/* 123 */
	yynts_30,	/* 124 */
	yynts_40,	/* 125 */
	yynts_39,	/* 126 */
	yynts_40,	/* 127 */
	yynts_41,	/* 128 */
	yynts_41,	/* 129 */
	yynts_41,	/* 130 */
	yynts_41,	/* 131 */
	yynts_41,	/* 132 */
	yynts_41,	/* 133 */
	yynts_41,	/* 134 */
	yynts_41,	/* 135 */
	yynts_41,	/* 136 */
	yynts_41,	/* 137 */
	yynts_40,	/* 138 */
	yynts_41,	/* 139 */
	yynts_41,	/* 140 */
	yynts_3,	/* 141 */
	yynts_3,	/* 142 */
	yynts_3,	/* 143 */
	yynts_3,	/* 144 */
	yynts_3,	/* 145 */
	yynts_3,	/* 146 */
};


static YYCONST char *yystring[] = {
/* 0 */	0,
/* 1 */	"file: PROGRAM(decls_prog,body)",
/* 2 */	"file: MODULE(decls)",
/* 3 */	"decls_prog: decls",
/* 4 */	"decls: DECLS(decls,decl)",
/* 5 */	"decls: NIL",
/* 6 */	"decl: F_RET(type,f_id)",
/* 7 */	"decl: VAR(V_TYPE(type,V_ID(ID,size)),init)",
/* 8 */	"size: NUMLIT",
/* 9 */	"size: NIL",
/* 10 */	"type: NUMBER",
/* 11 */	"type: STRING",
/* 12 */	"type: ARRAY",
/* 13 */	"type: VOID",
/* 14 */	"init: INIT(NUMLIT)",
/* 15 */	"init: INIT(CHRLIT)",
/* 16 */	"init: INIT(TXTLIT)",
/* 17 */	"init: INIT(array)",
/* 18 */	"init: NIL",
/* 19 */	"array: ELEM(NIL,NUMLIT)",
/* 20 */	"array: ELEM(NUMLIT,NUMLIT)",
/* 21 */	"array: ELEM(array,NUMLIT)",
/* 22 */	"f_id: F_ID(f_id_do,F_PARAMS(params_end,DO(body)))",
/* 23 */	"f_id: F_ID(f_id_done,F_PARAMS(params_end,DONE))",
/* 24 */	"f_id_do: ID",
/* 25 */	"f_id_done: ID",
/* 26 */	"params_end: params",
/* 27 */	"params: PARAM(params,V_TYPE(type,V_ID(ID,size)))",
/* 28 */	"params: NIL",
/* 29 */	"body: BODY(vars_end,block)",
/* 30 */	"vars_end: vars",
/* 31 */	"vars: VARS(vars,V_TYPE(type,V_ID(ID,size)))",
/* 32 */	"vars: NIL",
/* 33 */	"block: BLOCK(instrs,endinstr)",
/* 34 */	"instrs: INSTR(instrs,instr)",
/* 35 */	"instrs: NIL",
/* 36 */	"instr: FI(elifs,ELSE(block))",
/* 37 */	"instr: FI(elifs_last,NIL)",
/* 38 */	"instr: FOR(for_init,until)",
/* 39 */	"instr: EXPR(expr)",
/* 40 */	"instr: EXPR(expr)",
/* 41 */	"instr: EXPR(expr)",
/* 42 */	"instr: PRINT(expr)",
/* 43 */	"instr: ALLOC(alloc_size_str,lval)",
/* 44 */	"instr: ALLOC(alloc_size_arr,lval)",
/* 45 */	"instr: ASSERT(expr)",
/* 46 */	"alloc_size_str: expr",
/* 47 */	"alloc_size_arr: expr",
/* 48 */	"elifs: IF(cond,block)",
/* 49 */	"elifs: ELIFS(elifs,ELIF(cond,block))",
/* 50 */	"elifs_last: IF(cond,block)",
/* 51 */	"elifs_last: ELIFS(elifs,ELIF(cond,block))",
/* 52 */	"cond: expr",
/* 53 */	"for_init: expr",
/* 54 */	"until: UNTIL(until_expr,step)",
/* 55 */	"until_expr: expr",
/* 56 */	"step: STEP(step_block,expr)",
/* 57 */	"step_block: block",
/* 58 */	"endinstr: REPEAT",
/* 59 */	"endinstr: STOP",
/* 60 */	"endinstr: RETURN(NIL)",
/* 61 */	"endinstr: RETURN(expr)",
/* 62 */	"endinstr: NIL",
/* 63 */	"lval: ID",
/* 64 */	"lval: INDEX(id_idx,expr)",
/* 65 */	"id_idx: ID",
/* 66 */	"expr: INDEX(id_idx,expr)",
/* 67 */	"expr: ID",
/* 68 */	"expr: NUMLIT",
/* 69 */	"expr: CHRLIT",
/* 70 */	"expr: TXTLIT",
/* 71 */	"expr: INPUT",
/* 72 */	"expr: CALL(ID,args)",
/* 73 */	"expr: ADDR(lval)",
/* 74 */	"expr: UMINUS(expr)",
/* 75 */	"expr: POW(expr,expr)",
/* 76 */	"expr: MUL(expr,expr)",
/* 77 */	"expr: DIV(expr,expr)",
/* 78 */	"expr: MOD(expr,expr)",
/* 79 */	"expr: ADD(expr,expr)",
/* 80 */	"expr: ADD(exprX4,expr)",
/* 81 */	"expr: ADD(expr,exprX4)",
/* 82 */	"expr: SUB(expr,expr)",
/* 83 */	"expr: SUB(exprX4,expr)",
/* 84 */	"expr: SUB(expr,exprX4)",
/* 85 */	"expr: SUB(expr,expr)",
/* 86 */	"exprX4: expr",
/* 87 */	"expr: LT(expr,expr)",
/* 88 */	"expr: GT(expr,expr)",
/* 89 */	"expr: LEQ(expr,expr)",
/* 90 */	"expr: GEQ(expr,expr)",
/* 91 */	"expr: EQ(expr,expr)",
/* 92 */	"expr: NEQ(expr,expr)",
/* 93 */	"expr: BOR(expr,expr)",
/* 94 */	"expr: LT(expr,expr)",
/* 95 */	"expr: GT(expr,expr)",
/* 96 */	"expr: LEQ(expr,expr)",
/* 97 */	"expr: GEQ(expr,expr)",
/* 98 */	"expr: EQ(expr,expr)",
/* 99 */	"expr: NEQ(expr,expr)",
/* 100 */	"expr: NOT(expr)",
/* 101 */	"expr: AND(and,expr)",
/* 102 */	"expr: OR(or,expr)",
/* 103 */	"expr: ASSIGN(assign_value,INDEX(id_idx,expr))",
/* 104 */	"expr: ASSIGN(assign_value,lval)",
/* 105 */	"assign_value: expr",
/* 106 */	"and: expr",
/* 107 */	"or: expr",
/* 108 */	"args: ARG(expr,args)",
/* 109 */	"args: NIL",
/* 110 */	"num: NUMLIT",
/* 111 */	"num: CHRLIT",
/* 112 */	"expr: ASSIGN(ID,ID)",
/* 113 */	"expr: ASSIGN(expr,ID)",
/* 114 */	"instr: incdecInstr",
/* 115 */	"incdecInstr: ASSIGN(ADD(ID,num),ID)",
/* 116 */	"incdecInstr: ASSIGN(ADD(num,ID),ID)",
/* 117 */	"incdecInstr: ASSIGN(SUB(ID,num),ID)",
/* 118 */	"incdecInstr: ASSIGN(SUB(num,ID),ID)",
/* 119 */	"cond: LT(expr,expr)",
/* 120 */	"cond: GT(expr,expr)",
/* 121 */	"cond: LEQ(expr,expr)",
/* 122 */	"cond: GEQ(expr,expr)",
/* 123 */	"cond: EQ(expr,expr)",
/* 124 */	"cond: NEQ(expr,expr)",
/* 125 */	"expr: numConst",
/* 126 */	"numConst: num",
/* 127 */	"numConst: UMINUS(numConst)",
/* 128 */	"numConst: POW(numConst,numConst)",
/* 129 */	"numConst: MUL(numConst,numConst)",
/* 130 */	"numConst: DIV(numConst,numConst)",
/* 131 */	"numConst: MOD(numConst,numConst)",
/* 132 */	"numConst: ADD(numConst,numConst)",
/* 133 */	"numConst: LT(numConst,numConst)",
/* 134 */	"numConst: GT(numConst,numConst)",
/* 135 */	"numConst: LEQ(numConst,numConst)",
/* 136 */	"numConst: GEQ(numConst,numConst)",
/* 137 */	"numConst: EQ(numConst,numConst)",
/* 138 */	"numConst: NOT(numConst)",
/* 139 */	"numConst: AND(numConst,numConst)",
/* 140 */	"numConst: OR(numConst,numConst)",
/* 141 */	"numConst: LT(CHRLIT,CHRLIT)",
/* 142 */	"numConst: GT(CHRLIT,CHRLIT)",
/* 143 */	"numConst: LEQ(CHRLIT,CHRLIT)",
/* 144 */	"numConst: GEQ(CHRLIT,CHRLIT)",
/* 145 */	"numConst: EQ(CHRLIT,CHRLIT)",
/* 146 */	"numConst: NEQ(CHRLIT,CHRLIT)",
};

#ifndef TRACE
static void yytrace(NODEPTR_TYPE p, int eruleno, int cost, int bestcost)
{
	int op = OP_LABEL(p);
	YYCONST char *tname = yytermname[op] ? yytermname[op] : "?";
	fprintf(stderr, "0x%lx:%s matched %s with cost %d vs. %d\n", (long)p, tname, yystring[eruleno], cost, bestcost);
}
#endif

static short yydecode_file[] = {
	0,
	1,
	2,
};

static short yydecode_decls_prog[] = {
	0,
	3,
};

static short yydecode_body[] = {
	0,
	29,
};

static short yydecode_decls[] = {
	0,
	4,
	5,
};

static short yydecode_decl[] = {
	0,
	6,
	7,
};

static short yydecode_type[] = {
	0,
	10,
	11,
	12,
	13,
};

static short yydecode_f_id[] = {
	0,
	22,
	23,
};

static short yydecode_size[] = {
	0,
	8,
	9,
};

static short yydecode_init[] = {
	0,
	14,
	15,
	16,
	17,
	18,
};

static short yydecode_array[] = {
	0,
	19,
	20,
	21,
};

static short yydecode_f_id_do[] = {
	0,
	24,
};

static short yydecode_params_end[] = {
	0,
	26,
};

static short yydecode_f_id_done[] = {
	0,
	25,
};

static short yydecode_params[] = {
	0,
	27,
	28,
};

static short yydecode_vars_end[] = {
	0,
	30,
};

static short yydecode_block[] = {
	0,
	33,
};

static short yydecode_vars[] = {
	0,
	31,
	32,
};

static short yydecode_instrs[] = {
	0,
	34,
	35,
};

static short yydecode_endinstr[] = {
	0,
	58,
	59,
	60,
	61,
	62,
};

static short yydecode_instr[] = {
	0,
	36,
	37,
	38,
	39,
	40,
	41,
	42,
	43,
	44,
	45,
	114,
};

static short yydecode_elifs[] = {
	0,
	48,
	49,
};

static short yydecode_elifs_last[] = {
	0,
	50,
	51,
};

static short yydecode_for_init[] = {
	0,
	53,
};

static short yydecode_until[] = {
	0,
	54,
};

static short yydecode_expr[] = {
	0,
	66,
	67,
	68,
	69,
	70,
	71,
	72,
	73,
	74,
	75,
	76,
	77,
	78,
	79,
	80,
	81,
	82,
	83,
	84,
	85,
	87,
	88,
	89,
	90,
	91,
	92,
	93,
	94,
	95,
	96,
	97,
	98,
	99,
	100,
	101,
	102,
	103,
	104,
	112,
	113,
	125,
};

static short yydecode_alloc_size_str[] = {
	0,
	46,
};

static short yydecode_lval[] = {
	0,
	63,
	64,
};

static short yydecode_alloc_size_arr[] = {
	0,
	47,
};

static short yydecode_cond[] = {
	0,
	52,
	119,
	120,
	121,
	122,
	123,
	124,
};

static short yydecode_until_expr[] = {
	0,
	55,
};

static short yydecode_step[] = {
	0,
	56,
};

static short yydecode_step_block[] = {
	0,
	57,
};

static short yydecode_id_idx[] = {
	0,
	65,
};

static short yydecode_args[] = {
	0,
	108,
	109,
};

static short yydecode_exprX4[] = {
	0,
	86,
};

static short yydecode_and[] = {
	0,
	106,
};

static short yydecode_or[] = {
	0,
	107,
};

static short yydecode_assign_value[] = {
	0,
	105,
};

static short yydecode_num[] = {
	0,
	110,
	111,
};

static short yydecode_incdecInstr[] = {
	0,
	115,
	116,
	117,
	118,
};

static short yydecode_numConst[] = {
	0,
	126,
	127,
	128,
	129,
	130,
	131,
	132,
	133,
	134,
	135,
	136,
	137,
	138,
	139,
	140,
	141,
	142,
	143,
	144,
	145,
	146,
};

static int yyrule(void *state, int goalnt) {
	if (goalnt < 1 || goalnt > 41)
		PANIC("yyrule", "Bad goal nonterminal", goalnt);
	if (!state)
		return 0;
	switch (goalnt) {
	case yyfile_NT:	return yydecode_file[((struct yystate *)state)->rule.yyfile];
	case yydecls_prog_NT:	return yydecode_decls_prog[((struct yystate *)state)->rule.yydecls_prog];
	case yybody_NT:	return yydecode_body[((struct yystate *)state)->rule.yybody];
	case yydecls_NT:	return yydecode_decls[((struct yystate *)state)->rule.yydecls];
	case yydecl_NT:	return yydecode_decl[((struct yystate *)state)->rule.yydecl];
	case yytype_NT:	return yydecode_type[((struct yystate *)state)->rule.yytype];
	case yyf_id_NT:	return yydecode_f_id[((struct yystate *)state)->rule.yyf_id];
	case yysize_NT:	return yydecode_size[((struct yystate *)state)->rule.yysize];
	case yyinit_NT:	return yydecode_init[((struct yystate *)state)->rule.yyinit];
	case yyarray_NT:	return yydecode_array[((struct yystate *)state)->rule.yyarray];
	case yyf_id_do_NT:	return yydecode_f_id_do[((struct yystate *)state)->rule.yyf_id_do];
	case yyparams_end_NT:	return yydecode_params_end[((struct yystate *)state)->rule.yyparams_end];
	case yyf_id_done_NT:	return yydecode_f_id_done[((struct yystate *)state)->rule.yyf_id_done];
	case yyparams_NT:	return yydecode_params[((struct yystate *)state)->rule.yyparams];
	case yyvars_end_NT:	return yydecode_vars_end[((struct yystate *)state)->rule.yyvars_end];
	case yyblock_NT:	return yydecode_block[((struct yystate *)state)->rule.yyblock];
	case yyvars_NT:	return yydecode_vars[((struct yystate *)state)->rule.yyvars];
	case yyinstrs_NT:	return yydecode_instrs[((struct yystate *)state)->rule.yyinstrs];
	case yyendinstr_NT:	return yydecode_endinstr[((struct yystate *)state)->rule.yyendinstr];
	case yyinstr_NT:	return yydecode_instr[((struct yystate *)state)->rule.yyinstr];
	case yyelifs_NT:	return yydecode_elifs[((struct yystate *)state)->rule.yyelifs];
	case yyelifs_last_NT:	return yydecode_elifs_last[((struct yystate *)state)->rule.yyelifs_last];
	case yyfor_init_NT:	return yydecode_for_init[((struct yystate *)state)->rule.yyfor_init];
	case yyuntil_NT:	return yydecode_until[((struct yystate *)state)->rule.yyuntil];
	case yyexpr_NT:	return yydecode_expr[((struct yystate *)state)->rule.yyexpr];
	case yyalloc_size_str_NT:	return yydecode_alloc_size_str[((struct yystate *)state)->rule.yyalloc_size_str];
	case yylval_NT:	return yydecode_lval[((struct yystate *)state)->rule.yylval];
	case yyalloc_size_arr_NT:	return yydecode_alloc_size_arr[((struct yystate *)state)->rule.yyalloc_size_arr];
	case yycond_NT:	return yydecode_cond[((struct yystate *)state)->rule.yycond];
	case yyuntil_expr_NT:	return yydecode_until_expr[((struct yystate *)state)->rule.yyuntil_expr];
	case yystep_NT:	return yydecode_step[((struct yystate *)state)->rule.yystep];
	case yystep_block_NT:	return yydecode_step_block[((struct yystate *)state)->rule.yystep_block];
	case yyid_idx_NT:	return yydecode_id_idx[((struct yystate *)state)->rule.yyid_idx];
	case yyargs_NT:	return yydecode_args[((struct yystate *)state)->rule.yyargs];
	case yyexprX4_NT:	return yydecode_exprX4[((struct yystate *)state)->rule.yyexprX4];
	case yyand_NT:	return yydecode_and[((struct yystate *)state)->rule.yyand];
	case yyor_NT:	return yydecode_or[((struct yystate *)state)->rule.yyor];
	case yyassign_value_NT:	return yydecode_assign_value[((struct yystate *)state)->rule.yyassign_value];
	case yynum_NT:	return yydecode_num[((struct yystate *)state)->rule.yynum];
	case yyincdecInstr_NT:	return yydecode_incdecInstr[((struct yystate *)state)->rule.yyincdecInstr];
	case yynumConst_NT:	return yydecode_numConst[((struct yystate *)state)->rule.yynumConst];
	default:
		PANIC("yyrule", "Bad goal nonterminal", goalnt);
		return 0;
	}
}

static void yyclosure_decls(NODEPTR_TYPE, int);
static void yyclosure_params(NODEPTR_TYPE, int);
static void yyclosure_block(NODEPTR_TYPE, int);
static void yyclosure_vars(NODEPTR_TYPE, int);
static void yyclosure_expr(NODEPTR_TYPE, int);
static void yyclosure_num(NODEPTR_TYPE, int);
static void yyclosure_incdecInstr(NODEPTR_TYPE, int);
static void yyclosure_numConst(NODEPTR_TYPE, int);

static void yyclosure_decls(NODEPTR_TYPE a, int c) {
	struct yystate *p = (struct yystate *)STATE_LABEL(a);
	yytrace(a, 3, c + 1, p->cost[yydecls_prog_NT]);
	if (c + 1 < p->cost[yydecls_prog_NT]) {
		p->cost[yydecls_prog_NT] = c + 1;
		p->rule.yydecls_prog = 1;
	}
}

static void yyclosure_params(NODEPTR_TYPE a, int c) {
	struct yystate *p = (struct yystate *)STATE_LABEL(a);
	yytrace(a, 26, c + 1, p->cost[yyparams_end_NT]);
	if (c + 1 < p->cost[yyparams_end_NT]) {
		p->cost[yyparams_end_NT] = c + 1;
		p->rule.yyparams_end = 1;
	}
}

static void yyclosure_block(NODEPTR_TYPE a, int c) {
	struct yystate *p = (struct yystate *)STATE_LABEL(a);
	yytrace(a, 57, c + 1, p->cost[yystep_block_NT]);
	if (c + 1 < p->cost[yystep_block_NT]) {
		p->cost[yystep_block_NT] = c + 1;
		p->rule.yystep_block = 1;
	}
}

static void yyclosure_vars(NODEPTR_TYPE a, int c) {
	struct yystate *p = (struct yystate *)STATE_LABEL(a);
	yytrace(a, 30, c + 1, p->cost[yyvars_end_NT]);
	if (c + 1 < p->cost[yyvars_end_NT]) {
		p->cost[yyvars_end_NT] = c + 1;
		p->rule.yyvars_end = 1;
	}
}

static void yyclosure_expr(NODEPTR_TYPE a, int c) {
	struct yystate *p = (struct yystate *)STATE_LABEL(a);
	yytrace(a, 107, c + 1, p->cost[yyor_NT]);
	if (c + 1 < p->cost[yyor_NT]) {
		p->cost[yyor_NT] = c + 1;
		p->rule.yyor = 1;
	}
	yytrace(a, 106, c + 1, p->cost[yyand_NT]);
	if (c + 1 < p->cost[yyand_NT]) {
		p->cost[yyand_NT] = c + 1;
		p->rule.yyand = 1;
	}
	yytrace(a, 105, c + 1, p->cost[yyassign_value_NT]);
	if (c + 1 < p->cost[yyassign_value_NT]) {
		p->cost[yyassign_value_NT] = c + 1;
		p->rule.yyassign_value = 1;
	}
	yytrace(a, 86, c + 0, p->cost[yyexprX4_NT]);
	if (c + 0 < p->cost[yyexprX4_NT]) {
		p->cost[yyexprX4_NT] = c + 0;
		p->rule.yyexprX4 = 1;
	}
	yytrace(a, 55, c + 1, p->cost[yyuntil_expr_NT]);
	if (c + 1 < p->cost[yyuntil_expr_NT]) {
		p->cost[yyuntil_expr_NT] = c + 1;
		p->rule.yyuntil_expr = 1;
	}
	yytrace(a, 53, c + 1, p->cost[yyfor_init_NT]);
	if (c + 1 < p->cost[yyfor_init_NT]) {
		p->cost[yyfor_init_NT] = c + 1;
		p->rule.yyfor_init = 1;
	}
	yytrace(a, 52, c + 2, p->cost[yycond_NT]);
	if (c + 2 < p->cost[yycond_NT]) {
		p->cost[yycond_NT] = c + 2;
		p->rule.yycond = 1;
	}
	yytrace(a, 47, c + 1, p->cost[yyalloc_size_arr_NT]);
	if (c + 1 < p->cost[yyalloc_size_arr_NT]) {
		p->cost[yyalloc_size_arr_NT] = c + 1;
		p->rule.yyalloc_size_arr = 1;
	}
	yytrace(a, 46, c + 1, p->cost[yyalloc_size_str_NT]);
	if (c + 1 < p->cost[yyalloc_size_str_NT]) {
		p->cost[yyalloc_size_str_NT] = c + 1;
		p->rule.yyalloc_size_str = 1;
	}
}

static void yyclosure_num(NODEPTR_TYPE a, int c) {
	struct yystate *p = (struct yystate *)STATE_LABEL(a);
	yytrace(a, 126, c + 0, p->cost[yynumConst_NT]);
	if (c + 0 < p->cost[yynumConst_NT]) {
		p->cost[yynumConst_NT] = c + 0;
		p->rule.yynumConst = 1;
		yyclosure_numConst(a, c + 0);
	}
}

static void yyclosure_incdecInstr(NODEPTR_TYPE a, int c) {
	struct yystate *p = (struct yystate *)STATE_LABEL(a);
	yytrace(a, 114, c + 1, p->cost[yyinstr_NT]);
	if (c + 1 < p->cost[yyinstr_NT]) {
		p->cost[yyinstr_NT] = c + 1;
		p->rule.yyinstr = 11;
	}
}

static void yyclosure_numConst(NODEPTR_TYPE a, int c) {
	struct yystate *p = (struct yystate *)STATE_LABEL(a);
	yytrace(a, 125, c + 1, p->cost[yyexpr_NT]);
	if (c + 1 < p->cost[yyexpr_NT]) {
		p->cost[yyexpr_NT] = c + 1;
		p->rule.yyexpr = 41;
		yyclosure_expr(a, c + 1);
	}
}

static void yylabel(NODEPTR_TYPE a, NODEPTR_TYPE u) {
	int c;
	struct yystate *p;

	if (!a)
		PANIC("yylabel", "Null tree in", OP_LABEL(u));
	STATE_LABEL(a) = p = (struct yystate *)malloc(sizeof *p);
	memset(p, 0, sizeof *p);
	p->cost[1] =
	p->cost[2] =
	p->cost[3] =
	p->cost[4] =
	p->cost[5] =
	p->cost[6] =
	p->cost[7] =
	p->cost[8] =
	p->cost[9] =
	p->cost[10] =
	p->cost[11] =
	p->cost[12] =
	p->cost[13] =
	p->cost[14] =
	p->cost[15] =
	p->cost[16] =
	p->cost[17] =
	p->cost[18] =
	p->cost[19] =
	p->cost[20] =
	p->cost[21] =
	p->cost[22] =
	p->cost[23] =
	p->cost[24] =
	p->cost[25] =
	p->cost[26] =
	p->cost[27] =
	p->cost[28] =
	p->cost[29] =
	p->cost[30] =
	p->cost[31] =
	p->cost[32] =
	p->cost[33] =
	p->cost[34] =
	p->cost[35] =
	p->cost[36] =
	p->cost[37] =
	p->cost[38] =
	p->cost[39] =
	p->cost[40] =
	p->cost[41] =
		0x7fff;
	switch (OP_LABEL(a)) {
	case 33: /* PRINT */
		yylabel(LEFT_CHILD(a),a);
		/* instr: PRINT(expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + 1;
		yytrace(a, 42, c + 0, p->cost[yyinstr_NT]);
		if (c + 0 < p->cost[yyinstr_NT]) {
			p->cost[yyinstr_NT] = c + 0;
			p->rule.yyinstr = 7;
		}
		break;
	case 35: /* ALLOC */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* instr: ALLOC(alloc_size_str,lval) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyalloc_size_str_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yylval_NT] + (isStrRight(a));
		yytrace(a, 43, c + 0, p->cost[yyinstr_NT]);
		if (c + 0 < p->cost[yyinstr_NT]) {
			p->cost[yyinstr_NT] = c + 0;
			p->rule.yyinstr = 8;
		}
		/* instr: ALLOC(alloc_size_arr,lval) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyalloc_size_arr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yylval_NT] + (isArrRight(a));
		yytrace(a, 44, c + 0, p->cost[yyinstr_NT]);
		if (c + 0 < p->cost[yyinstr_NT]) {
			p->cost[yyinstr_NT] = c + 0;
			p->rule.yyinstr = 9;
		}
		break;
	case 37: /* MOD */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* expr: MOD(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + 1;
		yytrace(a, 78, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 13;
			yyclosure_expr(a, c + 0);
		}
		/* numConst: MOD(numConst,numConst) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yynumConst_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yynumConst_NT] + 0;
		yytrace(a, 131, c + 0, p->cost[yynumConst_NT]);
		if (c + 0 < p->cost[yynumConst_NT]) {
			p->cost[yynumConst_NT] = c + 0;
			p->rule.yynumConst = 6;
			yyclosure_numConst(a, c + 0);
		}
		break;
	case 38: /* AND */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* expr: AND(and,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyand_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + 1;
		yytrace(a, 101, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 35;
			yyclosure_expr(a, c + 0);
		}
		/* numConst: AND(numConst,numConst) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yynumConst_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yynumConst_NT] + 0;
		yytrace(a, 139, c + 0, p->cost[yynumConst_NT]);
		if (c + 0 < p->cost[yynumConst_NT]) {
			p->cost[yynumConst_NT] = c + 0;
			p->rule.yynumConst = 14;
			yyclosure_numConst(a, c + 0);
		}
		break;
	case 40: /* CALL */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		if (	/* expr: CALL(ID,args) */
			OP_LABEL(LEFT_CHILD(a)) == 291 /* ID */
		) {
			c = ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyargs_NT] + 1;
			yytrace(a, 72, c + 0, p->cost[yyexpr_NT]);
			if (c + 0 < p->cost[yyexpr_NT]) {
				p->cost[yyexpr_NT] = c + 0;
				p->rule.yyexpr = 7;
				yyclosure_expr(a, c + 0);
			}
		}
		break;
	case 42: /* MUL */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* expr: MUL(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + 1;
		yytrace(a, 76, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 11;
			yyclosure_expr(a, c + 0);
		}
		/* numConst: MUL(numConst,numConst) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yynumConst_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yynumConst_NT] + 0;
		yytrace(a, 129, c + 0, p->cost[yynumConst_NT]);
		if (c + 0 < p->cost[yynumConst_NT]) {
			p->cost[yynumConst_NT] = c + 0;
			p->rule.yynumConst = 4;
			yyclosure_numConst(a, c + 0);
		}
		break;
	case 43: /* ADD */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* expr: ADD(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isNumAndNum(a));
		yytrace(a, 79, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 14;
			yyclosure_expr(a, c + 0);
		}
		/* expr: ADD(exprX4,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexprX4_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isNumAndArr(a));
		yytrace(a, 80, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 15;
			yyclosure_expr(a, c + 0);
		}
		/* expr: ADD(expr,exprX4) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexprX4_NT] + (isArrAndNum(a));
		yytrace(a, 81, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 16;
			yyclosure_expr(a, c + 0);
		}
		/* numConst: ADD(numConst,numConst) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yynumConst_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yynumConst_NT] + 0;
		yytrace(a, 132, c + 0, p->cost[yynumConst_NT]);
		if (c + 0 < p->cost[yynumConst_NT]) {
			p->cost[yynumConst_NT] = c + 0;
			p->rule.yynumConst = 7;
			yyclosure_numConst(a, c + 0);
		}
		break;
	case 44: /* ELEM */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		if (	/* array: ELEM(NIL,NUMLIT) */
			OP_LABEL(LEFT_CHILD(a)) == 294 && /* NIL */
			OP_LABEL(RIGHT_CHILD(a)) == 288 /* NUMLIT */
		) {
			c = 1;
			yytrace(a, 19, c + 0, p->cost[yyarray_NT]);
			if (c + 0 < p->cost[yyarray_NT]) {
				p->cost[yyarray_NT] = c + 0;
				p->rule.yyarray = 1;
			}
		}
		if (	/* array: ELEM(NUMLIT,NUMLIT) */
			OP_LABEL(LEFT_CHILD(a)) == 288 && /* NUMLIT */
			OP_LABEL(RIGHT_CHILD(a)) == 288 /* NUMLIT */
		) {
			c = 1;
			yytrace(a, 20, c + 0, p->cost[yyarray_NT]);
			if (c + 0 < p->cost[yyarray_NT]) {
				p->cost[yyarray_NT] = c + 0;
				p->rule.yyarray = 2;
			}
		}
		if (	/* array: ELEM(array,NUMLIT) */
			OP_LABEL(RIGHT_CHILD(a)) == 288 /* NUMLIT */
		) {
			c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyarray_NT] + 1;
			yytrace(a, 21, c + 0, p->cost[yyarray_NT]);
			if (c + 0 < p->cost[yyarray_NT]) {
				p->cost[yyarray_NT] = c + 0;
				p->rule.yyarray = 3;
			}
		}
		break;
	case 45: /* SUB */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* expr: SUB(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isNumAndNum(a));
		yytrace(a, 82, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 17;
			yyclosure_expr(a, c + 0);
		}
		/* expr: SUB(exprX4,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexprX4_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isNumAndArr(a));
		yytrace(a, 83, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 18;
			yyclosure_expr(a, c + 0);
		}
		/* expr: SUB(expr,exprX4) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexprX4_NT] + (isArrAndNum(a));
		yytrace(a, 84, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 19;
			yyclosure_expr(a, c + 0);
		}
		/* expr: SUB(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isArrAndArr(a));
		yytrace(a, 85, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 20;
			yyclosure_expr(a, c + 0);
		}
		break;
	case 47: /* DIV */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* expr: DIV(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + 1;
		yytrace(a, 77, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 12;
			yyclosure_expr(a, c + 0);
		}
		/* numConst: DIV(numConst,numConst) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yynumConst_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yynumConst_NT] + 0;
		yytrace(a, 130, c + 0, p->cost[yynumConst_NT]);
		if (c + 0 < p->cost[yynumConst_NT]) {
			p->cost[yynumConst_NT] = c + 0;
			p->rule.yynumConst = 5;
			yyclosure_numConst(a, c + 0);
		}
		break;
	case 59: /* EXPR */
		yylabel(LEFT_CHILD(a),a);
		/* instr: EXPR(expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + (isNumberLeft(a));
		yytrace(a, 39, c + 0, p->cost[yyinstr_NT]);
		if (c + 0 < p->cost[yyinstr_NT]) {
			p->cost[yyinstr_NT] = c + 0;
			p->rule.yyinstr = 4;
		}
		/* instr: EXPR(expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + (isStrArrLeft(a));
		yytrace(a, 40, c + 0, p->cost[yyinstr_NT]);
		if (c + 0 < p->cost[yyinstr_NT]) {
			p->cost[yyinstr_NT] = c + 0;
			p->rule.yyinstr = 5;
		}
		/* instr: EXPR(expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + (isNilLeft(a));
		yytrace(a, 41, c + 0, p->cost[yyinstr_NT]);
		if (c + 0 < p->cost[yyinstr_NT]) {
			p->cost[yyinstr_NT] = c + 0;
			p->rule.yyinstr = 6;
		}
		break;
	case 60: /* LT */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* expr: LT(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isNumberLeft(a));
		yytrace(a, 87, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 21;
			yyclosure_expr(a, c + 0);
		}
		/* expr: LT(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isStrLeft(a));
		yytrace(a, 94, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 28;
			yyclosure_expr(a, c + 0);
		}
		/* cond: LT(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isNumberLeft(a));
		yytrace(a, 119, c + 0, p->cost[yycond_NT]);
		if (c + 0 < p->cost[yycond_NT]) {
			p->cost[yycond_NT] = c + 0;
			p->rule.yycond = 2;
		}
		/* numConst: LT(numConst,numConst) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yynumConst_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yynumConst_NT] + 0;
		yytrace(a, 133, c + 0, p->cost[yynumConst_NT]);
		if (c + 0 < p->cost[yynumConst_NT]) {
			p->cost[yynumConst_NT] = c + 0;
			p->rule.yynumConst = 8;
			yyclosure_numConst(a, c + 0);
		}
		if (	/* numConst: LT(CHRLIT,CHRLIT) */
			OP_LABEL(LEFT_CHILD(a)) == 289 && /* CHRLIT */
			OP_LABEL(RIGHT_CHILD(a)) == 289 /* CHRLIT */
		) {
			c = 0;
			yytrace(a, 141, c + 0, p->cost[yynumConst_NT]);
			if (c + 0 < p->cost[yynumConst_NT]) {
				p->cost[yynumConst_NT] = c + 0;
				p->rule.yynumConst = 16;
				yyclosure_numConst(a, c + 0);
			}
		}
		break;
	case 61: /* EQ */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* expr: EQ(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isNumberLeft(a));
		yytrace(a, 91, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 25;
			yyclosure_expr(a, c + 0);
		}
		/* expr: EQ(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isStrLeft(a));
		yytrace(a, 98, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 32;
			yyclosure_expr(a, c + 0);
		}
		/* cond: EQ(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isNumberLeft(a));
		yytrace(a, 123, c + 0, p->cost[yycond_NT]);
		if (c + 0 < p->cost[yycond_NT]) {
			p->cost[yycond_NT] = c + 0;
			p->rule.yycond = 6;
		}
		/* numConst: EQ(numConst,numConst) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yynumConst_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yynumConst_NT] + 0;
		yytrace(a, 137, c + 0, p->cost[yynumConst_NT]);
		if (c + 0 < p->cost[yynumConst_NT]) {
			p->cost[yynumConst_NT] = c + 0;
			p->rule.yynumConst = 12;
			yyclosure_numConst(a, c + 0);
		}
		if (	/* numConst: EQ(CHRLIT,CHRLIT) */
			OP_LABEL(LEFT_CHILD(a)) == 289 && /* CHRLIT */
			OP_LABEL(RIGHT_CHILD(a)) == 289 /* CHRLIT */
		) {
			c = 0;
			yytrace(a, 145, c + 0, p->cost[yynumConst_NT]);
			if (c + 0 < p->cost[yynumConst_NT]) {
				p->cost[yynumConst_NT] = c + 0;
				p->rule.yynumConst = 20;
				yyclosure_numConst(a, c + 0);
			}
		}
		break;
	case 62: /* GT */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* expr: GT(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isNumberLeft(a));
		yytrace(a, 88, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 22;
			yyclosure_expr(a, c + 0);
		}
		/* expr: GT(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isStrLeft(a));
		yytrace(a, 95, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 29;
			yyclosure_expr(a, c + 0);
		}
		/* cond: GT(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isNumberLeft(a));
		yytrace(a, 120, c + 0, p->cost[yycond_NT]);
		if (c + 0 < p->cost[yycond_NT]) {
			p->cost[yycond_NT] = c + 0;
			p->rule.yycond = 3;
		}
		/* numConst: GT(numConst,numConst) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yynumConst_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yynumConst_NT] + 0;
		yytrace(a, 134, c + 0, p->cost[yynumConst_NT]);
		if (c + 0 < p->cost[yynumConst_NT]) {
			p->cost[yynumConst_NT] = c + 0;
			p->rule.yynumConst = 9;
			yyclosure_numConst(a, c + 0);
		}
		if (	/* numConst: GT(CHRLIT,CHRLIT) */
			OP_LABEL(LEFT_CHILD(a)) == 289 && /* CHRLIT */
			OP_LABEL(RIGHT_CHILD(a)) == 289 /* CHRLIT */
		) {
			c = 0;
			yytrace(a, 142, c + 0, p->cost[yynumConst_NT]);
			if (c + 0 < p->cost[yynumConst_NT]) {
				p->cost[yynumConst_NT] = c + 0;
				p->rule.yynumConst = 17;
				yyclosure_numConst(a, c + 0);
			}
		}
		break;
	case 63: /* INPUT */
		/* expr: INPUT */
		yytrace(a, 71, 1 + 0, p->cost[yyexpr_NT]);
		if (1 + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = 1 + 0;
			p->rule.yyexpr = 6;
			yyclosure_expr(a, 1 + 0);
		}
		break;
	case 91: /* INDEX */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* lval: INDEX(id_idx,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyid_idx_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + 1;
		yytrace(a, 64, c + 0, p->cost[yylval_NT]);
		if (c + 0 < p->cost[yylval_NT]) {
			p->cost[yylval_NT] = c + 0;
			p->rule.yylval = 2;
		}
		/* expr: INDEX(id_idx,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyid_idx_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + 1;
		yytrace(a, 66, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 1;
			yyclosure_expr(a, c + 0);
		}
		break;
	case 94: /* POW */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* expr: POW(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + 1;
		yytrace(a, 75, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 10;
			yyclosure_expr(a, c + 0);
		}
		/* numConst: POW(numConst,numConst) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yynumConst_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yynumConst_NT] + 0;
		yytrace(a, 128, c + 0, p->cost[yynumConst_NT]);
		if (c + 0 < p->cost[yynumConst_NT]) {
			p->cost[yynumConst_NT] = c + 0;
			p->rule.yynumConst = 3;
			yyclosure_numConst(a, c + 0);
		}
		break;
	case 124: /* OR */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* expr: OR(or,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyor_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + 1;
		yytrace(a, 102, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 36;
			yyclosure_expr(a, c + 0);
		}
		/* numConst: OR(numConst,numConst) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yynumConst_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yynumConst_NT] + 0;
		yytrace(a, 140, c + 0, p->cost[yynumConst_NT]);
		if (c + 0 < p->cost[yynumConst_NT]) {
			p->cost[yynumConst_NT] = c + 0;
			p->rule.yynumConst = 15;
			yyclosure_numConst(a, c + 0);
		}
		break;
	case 126: /* NOT */
		yylabel(LEFT_CHILD(a),a);
		/* expr: NOT(expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + 1;
		yytrace(a, 100, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 34;
			yyclosure_expr(a, c + 0);
		}
		/* numConst: NOT(numConst) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yynumConst_NT] + 0;
		yytrace(a, 138, c + 0, p->cost[yynumConst_NT]);
		if (c + 0 < p->cost[yynumConst_NT]) {
			p->cost[yynumConst_NT] = c + 0;
			p->rule.yynumConst = 13;
			yyclosure_numConst(a, c + 0);
		}
		break;
	case 257: /* PROGRAM */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* file: PROGRAM(decls_prog,body) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yydecls_prog_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yybody_NT] + 1;
		yytrace(a, 1, c + 0, p->cost[yyfile_NT]);
		if (c + 0 < p->cost[yyfile_NT]) {
			p->cost[yyfile_NT] = c + 0;
			p->rule.yyfile = 1;
		}
		break;
	case 258: /* MODULE */
		yylabel(LEFT_CHILD(a),a);
		/* file: MODULE(decls) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yydecls_NT] + 1;
		yytrace(a, 2, c + 0, p->cost[yyfile_NT]);
		if (c + 0 < p->cost[yyfile_NT]) {
			p->cost[yyfile_NT] = c + 0;
			p->rule.yyfile = 2;
		}
		break;
	case 259: /* START */
		return;
	case 260: /* END */
		return;
	case 261: /* VOID */
		/* type: VOID */
		yytrace(a, 13, 1 + 0, p->cost[yytype_NT]);
		if (1 + 0 < p->cost[yytype_NT]) {
			p->cost[yytype_NT] = 1 + 0;
			p->rule.yytype = 4;
		}
		break;
	case 262: /* CONST */
		return;
	case 263: /* NUMBER */
		/* type: NUMBER */
		yytrace(a, 10, 1 + 0, p->cost[yytype_NT]);
		if (1 + 0 < p->cost[yytype_NT]) {
			p->cost[yytype_NT] = 1 + 0;
			p->rule.yytype = 1;
		}
		break;
	case 264: /* ARRAY */
		/* type: ARRAY */
		yytrace(a, 12, 1 + 0, p->cost[yytype_NT]);
		if (1 + 0 < p->cost[yytype_NT]) {
			p->cost[yytype_NT] = 1 + 0;
			p->rule.yytype = 3;
		}
		break;
	case 265: /* STRING */
		/* type: STRING */
		yytrace(a, 11, 1 + 0, p->cost[yytype_NT]);
		if (1 + 0 < p->cost[yytype_NT]) {
			p->cost[yytype_NT] = 1 + 0;
			p->rule.yytype = 2;
		}
		break;
	case 266: /* FUNCTION */
		return;
	case 267: /* PUBLIC */
		return;
	case 268: /* FORWARD */
		return;
	case 269: /* IF */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* elifs: IF(cond,block) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yycond_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyblock_NT] + 1;
		yytrace(a, 48, c + 0, p->cost[yyelifs_NT]);
		if (c + 0 < p->cost[yyelifs_NT]) {
			p->cost[yyelifs_NT] = c + 0;
			p->rule.yyelifs = 1;
		}
		/* elifs_last: IF(cond,block) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yycond_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyblock_NT] + 1;
		yytrace(a, 50, c + 0, p->cost[yyelifs_last_NT]);
		if (c + 0 < p->cost[yyelifs_last_NT]) {
			p->cost[yyelifs_last_NT] = c + 0;
			p->rule.yyelifs_last = 1;
		}
		break;
	case 270: /* THEN */
		return;
	case 271: /* ELSE */
		yylabel(LEFT_CHILD(a),a);
		return;
	case 272: /* ELIF */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		return;
	case 273: /* FI */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		if (	/* instr: FI(elifs,ELSE(block)) */
			OP_LABEL(RIGHT_CHILD(a)) == 271 /* ELSE */
		) {
			c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyelifs_NT] + ((struct yystate *)STATE_LABEL(LEFT_CHILD(RIGHT_CHILD(a))))->cost[yyblock_NT] + 1;
			yytrace(a, 36, c + 0, p->cost[yyinstr_NT]);
			if (c + 0 < p->cost[yyinstr_NT]) {
				p->cost[yyinstr_NT] = c + 0;
				p->rule.yyinstr = 1;
			}
		}
		if (	/* instr: FI(elifs_last,NIL) */
			OP_LABEL(RIGHT_CHILD(a)) == 294 /* NIL */
		) {
			c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyelifs_last_NT] + 1;
			yytrace(a, 37, c + 0, p->cost[yyinstr_NT]);
			if (c + 0 < p->cost[yyinstr_NT]) {
				p->cost[yyinstr_NT] = c + 0;
				p->rule.yyinstr = 2;
			}
		}
		break;
	case 274: /* FOR */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* instr: FOR(for_init,until) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyfor_init_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyuntil_NT] + 1;
		yytrace(a, 38, c + 0, p->cost[yyinstr_NT]);
		if (c + 0 < p->cost[yyinstr_NT]) {
			p->cost[yyinstr_NT] = c + 0;
			p->rule.yyinstr = 3;
		}
		break;
	case 275: /* UNTIL */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* until: UNTIL(until_expr,step) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyuntil_expr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yystep_NT] + 1;
		yytrace(a, 54, c + 0, p->cost[yyuntil_NT]);
		if (c + 0 < p->cost[yyuntil_NT]) {
			p->cost[yyuntil_NT] = c + 0;
			p->rule.yyuntil = 1;
		}
		break;
	case 276: /* STEP */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* step: STEP(step_block,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yystep_block_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + 1;
		yytrace(a, 56, c + 0, p->cost[yystep_NT]);
		if (c + 0 < p->cost[yystep_NT]) {
			p->cost[yystep_NT] = c + 0;
			p->rule.yystep = 1;
		}
		break;
	case 277: /* DO */
		yylabel(LEFT_CHILD(a),a);
		return;
	case 278: /* DONE */
		return;
	case 279: /* REPEAT */
		/* endinstr: REPEAT */
		yytrace(a, 58, 1 + 0, p->cost[yyendinstr_NT]);
		if (1 + 0 < p->cost[yyendinstr_NT]) {
			p->cost[yyendinstr_NT] = 1 + 0;
			p->rule.yyendinstr = 1;
		}
		break;
	case 280: /* STOP */
		/* endinstr: STOP */
		yytrace(a, 59, 1 + 0, p->cost[yyendinstr_NT]);
		if (1 + 0 < p->cost[yyendinstr_NT]) {
			p->cost[yyendinstr_NT] = 1 + 0;
			p->rule.yyendinstr = 2;
		}
		break;
	case 281: /* RETURN */
		yylabel(LEFT_CHILD(a),a);
		if (	/* endinstr: RETURN(NIL) */
			OP_LABEL(LEFT_CHILD(a)) == 294 /* NIL */
		) {
			c = 1;
			yytrace(a, 60, c + 0, p->cost[yyendinstr_NT]);
			if (c + 0 < p->cost[yyendinstr_NT]) {
				p->cost[yyendinstr_NT] = c + 0;
				p->rule.yyendinstr = 3;
			}
		}
		/* endinstr: RETURN(expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + 1;
		yytrace(a, 61, c + 0, p->cost[yyendinstr_NT]);
		if (c + 0 < p->cost[yyendinstr_NT]) {
			p->cost[yyendinstr_NT] = c + 0;
			p->rule.yyendinstr = 4;
		}
		break;
	case 282: /* ASSIGN */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		if (	/* expr: ASSIGN(assign_value,INDEX(id_idx,expr)) */
			OP_LABEL(RIGHT_CHILD(a)) == 91 /* INDEX */
		) {
			c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyassign_value_NT] + ((struct yystate *)STATE_LABEL(LEFT_CHILD(RIGHT_CHILD(a))))->cost[yyid_idx_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(RIGHT_CHILD(a))))->cost[yyexpr_NT] + 1;
			yytrace(a, 103, c + 0, p->cost[yyexpr_NT]);
			if (c + 0 < p->cost[yyexpr_NT]) {
				p->cost[yyexpr_NT] = c + 0;
				p->rule.yyexpr = 37;
				yyclosure_expr(a, c + 0);
			}
		}
		/* expr: ASSIGN(assign_value,lval) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyassign_value_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yylval_NT] + 3;
		yytrace(a, 104, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 38;
			yyclosure_expr(a, c + 0);
		}
		if (	/* expr: ASSIGN(ID,ID) */
			OP_LABEL(LEFT_CHILD(a)) == 291 && /* ID */
			OP_LABEL(RIGHT_CHILD(a)) == 291 /* ID */
		) {
			c = 1;
			yytrace(a, 112, c + 0, p->cost[yyexpr_NT]);
			if (c + 0 < p->cost[yyexpr_NT]) {
				p->cost[yyexpr_NT] = c + 0;
				p->rule.yyexpr = 39;
				yyclosure_expr(a, c + 0);
			}
		}
		if (	/* expr: ASSIGN(expr,ID) */
			OP_LABEL(RIGHT_CHILD(a)) == 291 /* ID */
		) {
			c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + 2;
			yytrace(a, 113, c + 0, p->cost[yyexpr_NT]);
			if (c + 0 < p->cost[yyexpr_NT]) {
				p->cost[yyexpr_NT] = c + 0;
				p->rule.yyexpr = 40;
				yyclosure_expr(a, c + 0);
			}
		}
		if (	/* incdecInstr: ASSIGN(ADD(ID,num),ID) */
			OP_LABEL(LEFT_CHILD(a)) == 43 && /* ADD */
			OP_LABEL(LEFT_CHILD(LEFT_CHILD(a))) == 291 && /* ID */
			OP_LABEL(RIGHT_CHILD(a)) == 291 /* ID */
		) {
			c = ((struct yystate *)STATE_LABEL(RIGHT_CHILD(LEFT_CHILD(a))))->cost[yynum_NT] + (sameNameLLR(a));
			yytrace(a, 115, c + 0, p->cost[yyincdecInstr_NT]);
			if (c + 0 < p->cost[yyincdecInstr_NT]) {
				p->cost[yyincdecInstr_NT] = c + 0;
				p->rule.yyincdecInstr = 1;
				yyclosure_incdecInstr(a, c + 0);
			}
		}
		if (	/* incdecInstr: ASSIGN(ADD(num,ID),ID) */
			OP_LABEL(LEFT_CHILD(a)) == 43 && /* ADD */
			OP_LABEL(RIGHT_CHILD(LEFT_CHILD(a))) == 291 && /* ID */
			OP_LABEL(RIGHT_CHILD(a)) == 291 /* ID */
		) {
			c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(LEFT_CHILD(a))))->cost[yynum_NT] + (sameNameRLR(a));
			yytrace(a, 116, c + 0, p->cost[yyincdecInstr_NT]);
			if (c + 0 < p->cost[yyincdecInstr_NT]) {
				p->cost[yyincdecInstr_NT] = c + 0;
				p->rule.yyincdecInstr = 2;
				yyclosure_incdecInstr(a, c + 0);
			}
		}
		if (	/* incdecInstr: ASSIGN(SUB(ID,num),ID) */
			OP_LABEL(LEFT_CHILD(a)) == 45 && /* SUB */
			OP_LABEL(LEFT_CHILD(LEFT_CHILD(a))) == 291 && /* ID */
			OP_LABEL(RIGHT_CHILD(a)) == 291 /* ID */
		) {
			c = ((struct yystate *)STATE_LABEL(RIGHT_CHILD(LEFT_CHILD(a))))->cost[yynum_NT] + (sameNameLLR(a));
			yytrace(a, 117, c + 0, p->cost[yyincdecInstr_NT]);
			if (c + 0 < p->cost[yyincdecInstr_NT]) {
				p->cost[yyincdecInstr_NT] = c + 0;
				p->rule.yyincdecInstr = 3;
				yyclosure_incdecInstr(a, c + 0);
			}
		}
		if (	/* incdecInstr: ASSIGN(SUB(num,ID),ID) */
			OP_LABEL(LEFT_CHILD(a)) == 45 && /* SUB */
			OP_LABEL(RIGHT_CHILD(LEFT_CHILD(a))) == 291 && /* ID */
			OP_LABEL(RIGHT_CHILD(a)) == 291 /* ID */
		) {
			c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(LEFT_CHILD(a))))->cost[yynum_NT] + (sameNameRLR(a));
			yytrace(a, 118, c + 0, p->cost[yyincdecInstr_NT]);
			if (c + 0 < p->cost[yyincdecInstr_NT]) {
				p->cost[yyincdecInstr_NT] = c + 0;
				p->rule.yyincdecInstr = 4;
				yyclosure_incdecInstr(a, c + 0);
			}
		}
		break;
	case 283: /* LEQ */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* expr: LEQ(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isNumberLeft(a));
		yytrace(a, 89, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 23;
			yyclosure_expr(a, c + 0);
		}
		/* expr: LEQ(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isStrLeft(a));
		yytrace(a, 96, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 30;
			yyclosure_expr(a, c + 0);
		}
		/* cond: LEQ(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isNumberLeft(a));
		yytrace(a, 121, c + 0, p->cost[yycond_NT]);
		if (c + 0 < p->cost[yycond_NT]) {
			p->cost[yycond_NT] = c + 0;
			p->rule.yycond = 4;
		}
		/* numConst: LEQ(numConst,numConst) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yynumConst_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yynumConst_NT] + 0;
		yytrace(a, 135, c + 0, p->cost[yynumConst_NT]);
		if (c + 0 < p->cost[yynumConst_NT]) {
			p->cost[yynumConst_NT] = c + 0;
			p->rule.yynumConst = 10;
			yyclosure_numConst(a, c + 0);
		}
		if (	/* numConst: LEQ(CHRLIT,CHRLIT) */
			OP_LABEL(LEFT_CHILD(a)) == 289 && /* CHRLIT */
			OP_LABEL(RIGHT_CHILD(a)) == 289 /* CHRLIT */
		) {
			c = 0;
			yytrace(a, 143, c + 0, p->cost[yynumConst_NT]);
			if (c + 0 < p->cost[yynumConst_NT]) {
				p->cost[yynumConst_NT] = c + 0;
				p->rule.yynumConst = 18;
				yyclosure_numConst(a, c + 0);
			}
		}
		break;
	case 284: /* GEQ */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* expr: GEQ(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isNumberLeft(a));
		yytrace(a, 90, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 24;
			yyclosure_expr(a, c + 0);
		}
		/* expr: GEQ(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isStrLeft(a));
		yytrace(a, 97, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 31;
			yyclosure_expr(a, c + 0);
		}
		/* cond: GEQ(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isNumberLeft(a));
		yytrace(a, 122, c + 0, p->cost[yycond_NT]);
		if (c + 0 < p->cost[yycond_NT]) {
			p->cost[yycond_NT] = c + 0;
			p->rule.yycond = 5;
		}
		/* numConst: GEQ(numConst,numConst) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yynumConst_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yynumConst_NT] + 0;
		yytrace(a, 136, c + 0, p->cost[yynumConst_NT]);
		if (c + 0 < p->cost[yynumConst_NT]) {
			p->cost[yynumConst_NT] = c + 0;
			p->rule.yynumConst = 11;
			yyclosure_numConst(a, c + 0);
		}
		if (	/* numConst: GEQ(CHRLIT,CHRLIT) */
			OP_LABEL(LEFT_CHILD(a)) == 289 && /* CHRLIT */
			OP_LABEL(RIGHT_CHILD(a)) == 289 /* CHRLIT */
		) {
			c = 0;
			yytrace(a, 144, c + 0, p->cost[yynumConst_NT]);
			if (c + 0 < p->cost[yynumConst_NT]) {
				p->cost[yynumConst_NT] = c + 0;
				p->rule.yynumConst = 19;
				yyclosure_numConst(a, c + 0);
			}
		}
		break;
	case 285: /* NEQ */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* expr: NEQ(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isNumberLeft(a));
		yytrace(a, 92, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 26;
			yyclosure_expr(a, c + 0);
		}
		/* expr: NEQ(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isStrLeft(a));
		yytrace(a, 99, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 33;
			yyclosure_expr(a, c + 0);
		}
		/* cond: NEQ(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isNumberLeft(a));
		yytrace(a, 124, c + 0, p->cost[yycond_NT]);
		if (c + 0 < p->cost[yycond_NT]) {
			p->cost[yycond_NT] = c + 0;
			p->rule.yycond = 7;
		}
		if (	/* numConst: NEQ(CHRLIT,CHRLIT) */
			OP_LABEL(LEFT_CHILD(a)) == 289 && /* CHRLIT */
			OP_LABEL(RIGHT_CHILD(a)) == 289 /* CHRLIT */
		) {
			c = 0;
			yytrace(a, 146, c + 0, p->cost[yynumConst_NT]);
			if (c + 0 < p->cost[yynumConst_NT]) {
				p->cost[yynumConst_NT] = c + 0;
				p->rule.yynumConst = 21;
				yyclosure_numConst(a, c + 0);
			}
		}
		break;
	case 286: /* BOR */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* expr: BOR(expr,expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyexpr_NT] + (isNumberLeft(a));
		yytrace(a, 93, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 27;
			yyclosure_expr(a, c + 0);
		}
		break;
	case 287: /* ASSERT */
		yylabel(LEFT_CHILD(a),a);
		/* instr: ASSERT(expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + 1;
		yytrace(a, 45, c + 0, p->cost[yyinstr_NT]);
		if (c + 0 < p->cost[yyinstr_NT]) {
			p->cost[yyinstr_NT] = c + 0;
			p->rule.yyinstr = 10;
		}
		break;
	case 288: /* NUMLIT */
		/* size: NUMLIT */
		yytrace(a, 8, 1 + 0, p->cost[yysize_NT]);
		if (1 + 0 < p->cost[yysize_NT]) {
			p->cost[yysize_NT] = 1 + 0;
			p->rule.yysize = 1;
		}
		/* expr: NUMLIT */
		yytrace(a, 68, 1 + 0, p->cost[yyexpr_NT]);
		if (1 + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = 1 + 0;
			p->rule.yyexpr = 3;
			yyclosure_expr(a, 1 + 0);
		}
		/* num: NUMLIT */
		yytrace(a, 110, 0 + 0, p->cost[yynum_NT]);
		if (0 + 0 < p->cost[yynum_NT]) {
			p->cost[yynum_NT] = 0 + 0;
			p->rule.yynum = 1;
			yyclosure_num(a, 0 + 0);
		}
		break;
	case 289: /* CHRLIT */
		/* expr: CHRLIT */
		yytrace(a, 69, 1 + 0, p->cost[yyexpr_NT]);
		if (1 + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = 1 + 0;
			p->rule.yyexpr = 4;
			yyclosure_expr(a, 1 + 0);
		}
		/* num: CHRLIT */
		yytrace(a, 111, 0 + 0, p->cost[yynum_NT]);
		if (0 + 0 < p->cost[yynum_NT]) {
			p->cost[yynum_NT] = 0 + 0;
			p->rule.yynum = 2;
			yyclosure_num(a, 0 + 0);
		}
		break;
	case 290: /* TXTLIT */
		/* expr: TXTLIT */
		yytrace(a, 70, 1 + 0, p->cost[yyexpr_NT]);
		if (1 + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = 1 + 0;
			p->rule.yyexpr = 5;
			yyclosure_expr(a, 1 + 0);
		}
		break;
	case 291: /* ID */
		/* f_id_do: ID */
		yytrace(a, 24, 1 + 0, p->cost[yyf_id_do_NT]);
		if (1 + 0 < p->cost[yyf_id_do_NT]) {
			p->cost[yyf_id_do_NT] = 1 + 0;
			p->rule.yyf_id_do = 1;
		}
		/* f_id_done: ID */
		yytrace(a, 25, 1 + 0, p->cost[yyf_id_done_NT]);
		if (1 + 0 < p->cost[yyf_id_done_NT]) {
			p->cost[yyf_id_done_NT] = 1 + 0;
			p->rule.yyf_id_done = 1;
		}
		/* lval: ID */
		yytrace(a, 63, 1 + 0, p->cost[yylval_NT]);
		if (1 + 0 < p->cost[yylval_NT]) {
			p->cost[yylval_NT] = 1 + 0;
			p->rule.yylval = 1;
		}
		/* id_idx: ID */
		yytrace(a, 65, 1 + 0, p->cost[yyid_idx_NT]);
		if (1 + 0 < p->cost[yyid_idx_NT]) {
			p->cost[yyid_idx_NT] = 1 + 0;
			p->rule.yyid_idx = 1;
		}
		/* expr: ID */
		yytrace(a, 67, 1 + 0, p->cost[yyexpr_NT]);
		if (1 + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = 1 + 0;
			p->rule.yyexpr = 2;
			yyclosure_expr(a, 1 + 0);
		}
		break;
	case 292: /* UMINUS */
		yylabel(LEFT_CHILD(a),a);
		/* expr: UMINUS(expr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + 1;
		yytrace(a, 74, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 9;
			yyclosure_expr(a, c + 0);
		}
		/* numConst: UMINUS(numConst) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yynumConst_NT] + 0;
		yytrace(a, 127, c + 0, p->cost[yynumConst_NT]);
		if (c + 0 < p->cost[yynumConst_NT]) {
			p->cost[yynumConst_NT] = c + 0;
			p->rule.yynumConst = 2;
			yyclosure_numConst(a, c + 0);
		}
		break;
	case 293: /* ADDR */
		yylabel(LEFT_CHILD(a),a);
		/* expr: ADDR(lval) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yylval_NT] + 1;
		yytrace(a, 73, c + 0, p->cost[yyexpr_NT]);
		if (c + 0 < p->cost[yyexpr_NT]) {
			p->cost[yyexpr_NT] = c + 0;
			p->rule.yyexpr = 8;
			yyclosure_expr(a, c + 0);
		}
		break;
	case 294: /* NIL */
		/* decls: NIL */
		yytrace(a, 5, 1 + 0, p->cost[yydecls_NT]);
		if (1 + 0 < p->cost[yydecls_NT]) {
			p->cost[yydecls_NT] = 1 + 0;
			p->rule.yydecls = 2;
			yyclosure_decls(a, 1 + 0);
		}
		/* size: NIL */
		yytrace(a, 9, 1 + 0, p->cost[yysize_NT]);
		if (1 + 0 < p->cost[yysize_NT]) {
			p->cost[yysize_NT] = 1 + 0;
			p->rule.yysize = 2;
		}
		/* init: NIL */
		yytrace(a, 18, 1 + 0, p->cost[yyinit_NT]);
		if (1 + 0 < p->cost[yyinit_NT]) {
			p->cost[yyinit_NT] = 1 + 0;
			p->rule.yyinit = 5;
		}
		/* params: NIL */
		yytrace(a, 28, 1 + 0, p->cost[yyparams_NT]);
		if (1 + 0 < p->cost[yyparams_NT]) {
			p->cost[yyparams_NT] = 1 + 0;
			p->rule.yyparams = 2;
			yyclosure_params(a, 1 + 0);
		}
		/* vars: NIL */
		yytrace(a, 32, 1 + 0, p->cost[yyvars_NT]);
		if (1 + 0 < p->cost[yyvars_NT]) {
			p->cost[yyvars_NT] = 1 + 0;
			p->rule.yyvars = 2;
			yyclosure_vars(a, 1 + 0);
		}
		/* instrs: NIL */
		yytrace(a, 35, 1 + 0, p->cost[yyinstrs_NT]);
		if (1 + 0 < p->cost[yyinstrs_NT]) {
			p->cost[yyinstrs_NT] = 1 + 0;
			p->rule.yyinstrs = 2;
		}
		/* endinstr: NIL */
		yytrace(a, 62, 1 + 0, p->cost[yyendinstr_NT]);
		if (1 + 0 < p->cost[yyendinstr_NT]) {
			p->cost[yyendinstr_NT] = 1 + 0;
			p->rule.yyendinstr = 5;
		}
		/* args: NIL */
		yytrace(a, 109, 1 + 0, p->cost[yyargs_NT]);
		if (1 + 0 < p->cost[yyargs_NT]) {
			p->cost[yyargs_NT] = 1 + 0;
			p->rule.yyargs = 2;
		}
		break;
	case 295: /* ERR */
		return;
	case 296: /* DECLS */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* decls: DECLS(decls,decl) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yydecls_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yydecl_NT] + 1;
		yytrace(a, 4, c + 0, p->cost[yydecls_NT]);
		if (c + 0 < p->cost[yydecls_NT]) {
			p->cost[yydecls_NT] = c + 0;
			p->rule.yydecls = 1;
			yyclosure_decls(a, c + 0);
		}
		break;
	case 297: /* SIZE */
		return;
	case 298: /* INIT */
		yylabel(LEFT_CHILD(a),a);
		if (	/* init: INIT(NUMLIT) */
			OP_LABEL(LEFT_CHILD(a)) == 288 /* NUMLIT */
		) {
			c = 1;
			yytrace(a, 14, c + 0, p->cost[yyinit_NT]);
			if (c + 0 < p->cost[yyinit_NT]) {
				p->cost[yyinit_NT] = c + 0;
				p->rule.yyinit = 1;
			}
		}
		if (	/* init: INIT(CHRLIT) */
			OP_LABEL(LEFT_CHILD(a)) == 289 /* CHRLIT */
		) {
			c = 1;
			yytrace(a, 15, c + 0, p->cost[yyinit_NT]);
			if (c + 0 < p->cost[yyinit_NT]) {
				p->cost[yyinit_NT] = c + 0;
				p->rule.yyinit = 2;
			}
		}
		if (	/* init: INIT(TXTLIT) */
			OP_LABEL(LEFT_CHILD(a)) == 290 /* TXTLIT */
		) {
			c = 1;
			yytrace(a, 16, c + 0, p->cost[yyinit_NT]);
			if (c + 0 < p->cost[yyinit_NT]) {
				p->cost[yyinit_NT] = c + 0;
				p->rule.yyinit = 3;
			}
		}
		/* init: INIT(array) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyarray_NT] + 1;
		yytrace(a, 17, c + 0, p->cost[yyinit_NT]);
		if (c + 0 < p->cost[yyinit_NT]) {
			p->cost[yyinit_NT] = c + 0;
			p->rule.yyinit = 4;
		}
		break;
	case 299: /* PARAM */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		if (	/* params: PARAM(params,V_TYPE(type,V_ID(ID,size))) */
			OP_LABEL(RIGHT_CHILD(a)) == 309 && /* V_TYPE */
			OP_LABEL(RIGHT_CHILD(RIGHT_CHILD(a))) == 310 && /* V_ID */
			OP_LABEL(LEFT_CHILD(RIGHT_CHILD(RIGHT_CHILD(a)))) == 291 /* ID */
		) {
			c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyparams_NT] + ((struct yystate *)STATE_LABEL(LEFT_CHILD(RIGHT_CHILD(a))))->cost[yytype_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(RIGHT_CHILD(RIGHT_CHILD(a)))))->cost[yysize_NT] + 1;
			yytrace(a, 27, c + 0, p->cost[yyparams_NT]);
			if (c + 0 < p->cost[yyparams_NT]) {
				p->cost[yyparams_NT] = c + 0;
				p->rule.yyparams = 1;
				yyclosure_params(a, c + 0);
			}
		}
		break;
	case 300: /* ARG */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* args: ARG(expr,args) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyexpr_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyargs_NT] + 1;
		yytrace(a, 108, c + 0, p->cost[yyargs_NT]);
		if (c + 0 < p->cost[yyargs_NT]) {
			p->cost[yyargs_NT] = c + 0;
			p->rule.yyargs = 1;
		}
		break;
	case 301: /* INSTR */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* instrs: INSTR(instrs,instr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyinstrs_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyinstr_NT] + 1;
		yytrace(a, 34, c + 0, p->cost[yyinstrs_NT]);
		if (c + 0 < p->cost[yyinstrs_NT]) {
			p->cost[yyinstrs_NT] = c + 0;
			p->rule.yyinstrs = 1;
		}
		break;
	case 302: /* BLOCK */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* block: BLOCK(instrs,endinstr) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyinstrs_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyendinstr_NT] + 1;
		yytrace(a, 33, c + 0, p->cost[yyblock_NT]);
		if (c + 0 < p->cost[yyblock_NT]) {
			p->cost[yyblock_NT] = c + 0;
			p->rule.yyblock = 1;
			yyclosure_block(a, c + 0);
		}
		break;
	case 303: /* BODY */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* body: BODY(vars_end,block) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyvars_end_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyblock_NT] + 1;
		yytrace(a, 29, c + 0, p->cost[yybody_NT]);
		if (c + 0 < p->cost[yybody_NT]) {
			p->cost[yybody_NT] = c + 0;
			p->rule.yybody = 1;
		}
		break;
	case 304: /* VARS */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		if (	/* vars: VARS(vars,V_TYPE(type,V_ID(ID,size))) */
			OP_LABEL(RIGHT_CHILD(a)) == 309 && /* V_TYPE */
			OP_LABEL(RIGHT_CHILD(RIGHT_CHILD(a))) == 310 && /* V_ID */
			OP_LABEL(LEFT_CHILD(RIGHT_CHILD(RIGHT_CHILD(a)))) == 291 /* ID */
		) {
			c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyvars_NT] + ((struct yystate *)STATE_LABEL(LEFT_CHILD(RIGHT_CHILD(a))))->cost[yytype_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(RIGHT_CHILD(RIGHT_CHILD(a)))))->cost[yysize_NT] + 1;
			yytrace(a, 31, c + 0, p->cost[yyvars_NT]);
			if (c + 0 < p->cost[yyvars_NT]) {
				p->cost[yyvars_NT] = c + 0;
				p->rule.yyvars = 1;
				yyclosure_vars(a, c + 0);
			}
		}
		break;
	case 305: /* VAR */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		if (	/* decl: VAR(V_TYPE(type,V_ID(ID,size)),init) */
			OP_LABEL(LEFT_CHILD(a)) == 309 && /* V_TYPE */
			OP_LABEL(RIGHT_CHILD(LEFT_CHILD(a))) == 310 && /* V_ID */
			OP_LABEL(LEFT_CHILD(RIGHT_CHILD(LEFT_CHILD(a)))) == 291 /* ID */
		) {
			c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(LEFT_CHILD(a))))->cost[yytype_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(RIGHT_CHILD(LEFT_CHILD(a)))))->cost[yysize_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyinit_NT] + 1;
			yytrace(a, 7, c + 0, p->cost[yydecl_NT]);
			if (c + 0 < p->cost[yydecl_NT]) {
				p->cost[yydecl_NT] = c + 0;
				p->rule.yydecl = 2;
			}
		}
		break;
	case 306: /* F_RET */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		/* decl: F_RET(type,f_id) */
		c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yytype_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(a)))->cost[yyf_id_NT] + 1;
		yytrace(a, 6, c + 0, p->cost[yydecl_NT]);
		if (c + 0 < p->cost[yydecl_NT]) {
			p->cost[yydecl_NT] = c + 0;
			p->rule.yydecl = 1;
		}
		break;
	case 307: /* F_ID */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		if (	/* f_id: F_ID(f_id_do,F_PARAMS(params_end,DO(body))) */
			OP_LABEL(RIGHT_CHILD(a)) == 308 && /* F_PARAMS */
			OP_LABEL(RIGHT_CHILD(RIGHT_CHILD(a))) == 277 /* DO */
		) {
			c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyf_id_do_NT] + ((struct yystate *)STATE_LABEL(LEFT_CHILD(RIGHT_CHILD(a))))->cost[yyparams_end_NT] + ((struct yystate *)STATE_LABEL(LEFT_CHILD(RIGHT_CHILD(RIGHT_CHILD(a)))))->cost[yybody_NT] + 1;
			yytrace(a, 22, c + 0, p->cost[yyf_id_NT]);
			if (c + 0 < p->cost[yyf_id_NT]) {
				p->cost[yyf_id_NT] = c + 0;
				p->rule.yyf_id = 1;
			}
		}
		if (	/* f_id: F_ID(f_id_done,F_PARAMS(params_end,DONE)) */
			OP_LABEL(RIGHT_CHILD(a)) == 308 && /* F_PARAMS */
			OP_LABEL(RIGHT_CHILD(RIGHT_CHILD(a))) == 278 /* DONE */
		) {
			c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyf_id_done_NT] + ((struct yystate *)STATE_LABEL(LEFT_CHILD(RIGHT_CHILD(a))))->cost[yyparams_end_NT] + 1;
			yytrace(a, 23, c + 0, p->cost[yyf_id_NT]);
			if (c + 0 < p->cost[yyf_id_NT]) {
				p->cost[yyf_id_NT] = c + 0;
				p->rule.yyf_id = 2;
			}
		}
		break;
	case 308: /* F_PARAMS */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		return;
	case 309: /* V_TYPE */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		return;
	case 310: /* V_ID */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		return;
	case 311: /* ELIFS */
		yylabel(LEFT_CHILD(a),a);
		yylabel(RIGHT_CHILD(a),a);
		if (	/* elifs: ELIFS(elifs,ELIF(cond,block)) */
			OP_LABEL(RIGHT_CHILD(a)) == 272 /* ELIF */
		) {
			c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyelifs_NT] + ((struct yystate *)STATE_LABEL(LEFT_CHILD(RIGHT_CHILD(a))))->cost[yycond_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(RIGHT_CHILD(a))))->cost[yyblock_NT] + 1;
			yytrace(a, 49, c + 0, p->cost[yyelifs_NT]);
			if (c + 0 < p->cost[yyelifs_NT]) {
				p->cost[yyelifs_NT] = c + 0;
				p->rule.yyelifs = 2;
			}
		}
		if (	/* elifs_last: ELIFS(elifs,ELIF(cond,block)) */
			OP_LABEL(RIGHT_CHILD(a)) == 272 /* ELIF */
		) {
			c = ((struct yystate *)STATE_LABEL(LEFT_CHILD(a)))->cost[yyelifs_NT] + ((struct yystate *)STATE_LABEL(LEFT_CHILD(RIGHT_CHILD(a))))->cost[yycond_NT] + ((struct yystate *)STATE_LABEL(RIGHT_CHILD(RIGHT_CHILD(a))))->cost[yyblock_NT] + 1;
			yytrace(a, 51, c + 0, p->cost[yyelifs_last_NT]);
			if (c + 0 < p->cost[yyelifs_last_NT]) {
				p->cost[yyelifs_last_NT] = c + 0;
				p->rule.yyelifs_last = 2;
			}
		}
		break;
	case 312: /* ELIF_COND */
		return;
	default:
		PANIC("yylabel", "Bad terminal", OP_LABEL(a));
	}
}

static void yykids(NODEPTR_TYPE p, int eruleno, NODEPTR_TYPE kids[]) {
	if (!p)
		PANIC("yykids", "Null tree in rule", eruleno);
	if (!kids)
		PANIC("yykids", "Null kids in", OP_LABEL(p));
	switch (eruleno) {
	case 140: /* numConst: OR(numConst,numConst) */
	case 139: /* numConst: AND(numConst,numConst) */
	case 137: /* numConst: EQ(numConst,numConst) */
	case 136: /* numConst: GEQ(numConst,numConst) */
	case 135: /* numConst: LEQ(numConst,numConst) */
	case 134: /* numConst: GT(numConst,numConst) */
	case 133: /* numConst: LT(numConst,numConst) */
	case 132: /* numConst: ADD(numConst,numConst) */
	case 131: /* numConst: MOD(numConst,numConst) */
	case 130: /* numConst: DIV(numConst,numConst) */
	case 129: /* numConst: MUL(numConst,numConst) */
	case 128: /* numConst: POW(numConst,numConst) */
	case 124: /* cond: NEQ(expr,expr) */
	case 123: /* cond: EQ(expr,expr) */
	case 122: /* cond: GEQ(expr,expr) */
	case 121: /* cond: LEQ(expr,expr) */
	case 120: /* cond: GT(expr,expr) */
	case 119: /* cond: LT(expr,expr) */
	case 108: /* args: ARG(expr,args) */
	case 104: /* expr: ASSIGN(assign_value,lval) */
	case 102: /* expr: OR(or,expr) */
	case 101: /* expr: AND(and,expr) */
	case 99: /* expr: NEQ(expr,expr) */
	case 98: /* expr: EQ(expr,expr) */
	case 97: /* expr: GEQ(expr,expr) */
	case 96: /* expr: LEQ(expr,expr) */
	case 95: /* expr: GT(expr,expr) */
	case 94: /* expr: LT(expr,expr) */
	case 93: /* expr: BOR(expr,expr) */
	case 92: /* expr: NEQ(expr,expr) */
	case 91: /* expr: EQ(expr,expr) */
	case 90: /* expr: GEQ(expr,expr) */
	case 89: /* expr: LEQ(expr,expr) */
	case 88: /* expr: GT(expr,expr) */
	case 87: /* expr: LT(expr,expr) */
	case 85: /* expr: SUB(expr,expr) */
	case 84: /* expr: SUB(expr,exprX4) */
	case 83: /* expr: SUB(exprX4,expr) */
	case 82: /* expr: SUB(expr,expr) */
	case 81: /* expr: ADD(expr,exprX4) */
	case 80: /* expr: ADD(exprX4,expr) */
	case 79: /* expr: ADD(expr,expr) */
	case 78: /* expr: MOD(expr,expr) */
	case 77: /* expr: DIV(expr,expr) */
	case 76: /* expr: MUL(expr,expr) */
	case 75: /* expr: POW(expr,expr) */
	case 66: /* expr: INDEX(id_idx,expr) */
	case 64: /* lval: INDEX(id_idx,expr) */
	case 56: /* step: STEP(step_block,expr) */
	case 54: /* until: UNTIL(until_expr,step) */
	case 50: /* elifs_last: IF(cond,block) */
	case 48: /* elifs: IF(cond,block) */
	case 44: /* instr: ALLOC(alloc_size_arr,lval) */
	case 43: /* instr: ALLOC(alloc_size_str,lval) */
	case 38: /* instr: FOR(for_init,until) */
	case 34: /* instrs: INSTR(instrs,instr) */
	case 33: /* block: BLOCK(instrs,endinstr) */
	case 29: /* body: BODY(vars_end,block) */
	case 6: /* decl: F_RET(type,f_id) */
	case 4: /* decls: DECLS(decls,decl) */
	case 1: /* file: PROGRAM(decls_prog,body) */
		kids[0] = LEFT_CHILD(p);
		kids[1] = RIGHT_CHILD(p);
		break;
	case 138: /* numConst: NOT(numConst) */
	case 127: /* numConst: UMINUS(numConst) */
	case 113: /* expr: ASSIGN(expr,ID) */
	case 100: /* expr: NOT(expr) */
	case 74: /* expr: UMINUS(expr) */
	case 73: /* expr: ADDR(lval) */
	case 61: /* endinstr: RETURN(expr) */
	case 45: /* instr: ASSERT(expr) */
	case 42: /* instr: PRINT(expr) */
	case 41: /* instr: EXPR(expr) */
	case 40: /* instr: EXPR(expr) */
	case 39: /* instr: EXPR(expr) */
	case 37: /* instr: FI(elifs_last,NIL) */
	case 21: /* array: ELEM(array,NUMLIT) */
	case 17: /* init: INIT(array) */
	case 2: /* file: MODULE(decls) */
		kids[0] = LEFT_CHILD(p);
		break;
	case 126: /* numConst: num */
	case 125: /* expr: numConst */
	case 114: /* instr: incdecInstr */
	case 107: /* or: expr */
	case 106: /* and: expr */
	case 105: /* assign_value: expr */
	case 86: /* exprX4: expr */
	case 57: /* step_block: block */
	case 55: /* until_expr: expr */
	case 53: /* for_init: expr */
	case 52: /* cond: expr */
	case 47: /* alloc_size_arr: expr */
	case 46: /* alloc_size_str: expr */
	case 30: /* vars_end: vars */
	case 26: /* params_end: params */
	case 3: /* decls_prog: decls */
		kids[0] = p;
		break;
	case 146: /* numConst: NEQ(CHRLIT,CHRLIT) */
	case 145: /* numConst: EQ(CHRLIT,CHRLIT) */
	case 144: /* numConst: GEQ(CHRLIT,CHRLIT) */
	case 143: /* numConst: LEQ(CHRLIT,CHRLIT) */
	case 142: /* numConst: GT(CHRLIT,CHRLIT) */
	case 141: /* numConst: LT(CHRLIT,CHRLIT) */
	case 112: /* expr: ASSIGN(ID,ID) */
	case 111: /* num: CHRLIT */
	case 110: /* num: NUMLIT */
	case 109: /* args: NIL */
	case 71: /* expr: INPUT */
	case 70: /* expr: TXTLIT */
	case 69: /* expr: CHRLIT */
	case 68: /* expr: NUMLIT */
	case 67: /* expr: ID */
	case 65: /* id_idx: ID */
	case 63: /* lval: ID */
	case 62: /* endinstr: NIL */
	case 60: /* endinstr: RETURN(NIL) */
	case 59: /* endinstr: STOP */
	case 58: /* endinstr: REPEAT */
	case 35: /* instrs: NIL */
	case 32: /* vars: NIL */
	case 28: /* params: NIL */
	case 25: /* f_id_done: ID */
	case 24: /* f_id_do: ID */
	case 20: /* array: ELEM(NUMLIT,NUMLIT) */
	case 19: /* array: ELEM(NIL,NUMLIT) */
	case 18: /* init: NIL */
	case 16: /* init: INIT(TXTLIT) */
	case 15: /* init: INIT(CHRLIT) */
	case 14: /* init: INIT(NUMLIT) */
	case 13: /* type: VOID */
	case 12: /* type: ARRAY */
	case 11: /* type: STRING */
	case 10: /* type: NUMBER */
	case 9: /* size: NIL */
	case 8: /* size: NUMLIT */
	case 5: /* decls: NIL */
		break;
	case 7: /* decl: VAR(V_TYPE(type,V_ID(ID,size)),init) */
		kids[0] = LEFT_CHILD(LEFT_CHILD(p));
		kids[1] = RIGHT_CHILD(RIGHT_CHILD(LEFT_CHILD(p)));
		kids[2] = RIGHT_CHILD(p);
		break;
	case 22: /* f_id: F_ID(f_id_do,F_PARAMS(params_end,DO(body))) */
		kids[0] = LEFT_CHILD(p);
		kids[1] = LEFT_CHILD(RIGHT_CHILD(p));
		kids[2] = LEFT_CHILD(RIGHT_CHILD(RIGHT_CHILD(p)));
		break;
	case 36: /* instr: FI(elifs,ELSE(block)) */
	case 23: /* f_id: F_ID(f_id_done,F_PARAMS(params_end,DONE)) */
		kids[0] = LEFT_CHILD(p);
		kids[1] = LEFT_CHILD(RIGHT_CHILD(p));
		break;
	case 31: /* vars: VARS(vars,V_TYPE(type,V_ID(ID,size))) */
	case 27: /* params: PARAM(params,V_TYPE(type,V_ID(ID,size))) */
		kids[0] = LEFT_CHILD(p);
		kids[1] = LEFT_CHILD(RIGHT_CHILD(p));
		kids[2] = RIGHT_CHILD(RIGHT_CHILD(RIGHT_CHILD(p)));
		break;
	case 103: /* expr: ASSIGN(assign_value,INDEX(id_idx,expr)) */
	case 51: /* elifs_last: ELIFS(elifs,ELIF(cond,block)) */
	case 49: /* elifs: ELIFS(elifs,ELIF(cond,block)) */
		kids[0] = LEFT_CHILD(p);
		kids[1] = LEFT_CHILD(RIGHT_CHILD(p));
		kids[2] = RIGHT_CHILD(RIGHT_CHILD(p));
		break;
	case 72: /* expr: CALL(ID,args) */
		kids[0] = RIGHT_CHILD(p);
		break;
	case 117: /* incdecInstr: ASSIGN(SUB(ID,num),ID) */
	case 115: /* incdecInstr: ASSIGN(ADD(ID,num),ID) */
		kids[0] = RIGHT_CHILD(LEFT_CHILD(p));
		break;
	case 118: /* incdecInstr: ASSIGN(SUB(num,ID),ID) */
	case 116: /* incdecInstr: ASSIGN(ADD(num,ID),ID) */
		kids[0] = LEFT_CHILD(LEFT_CHILD(p));
		break;
	default:
		PANIC("yykids", "Bad rule number", eruleno);
	}
}

static void yyreduce(NODEPTR_TYPE p, int goalnt)
{
  int eruleno = yyrule(STATE_LABEL(p), goalnt);
  short *nts = yynts[eruleno];
  NODEPTR_TYPE kids[3];
  int i;

  for (yykids(p, eruleno, kids), i = 0; nts[i]; i++)
    yyreduce(kids[i], nts[i]);

  switch(eruleno) {
	case 1: /* file: PROGRAM(decls_prog,body) */
		fprintf(stderr, "0x%lx: line 481: file: PROGRAM(decls_prog,body)\n",(long)p);
#line 481 "minor.brg"
{ IDpop(); ASM(pfIMM pfPOP pfLEAVE pfRET, 0); addExterns(); }
		break;
	case 2: /* file: MODULE(decls) */
		fprintf(stderr, "0x%lx: line 482: file: MODULE(decls)\n",(long)p);
#line 482 "minor.brg"
{ addExterns(); }
		break;
	case 3: /* decls_prog: decls */
		fprintf(stderr, "0x%lx: line 484: decls_prog: decls\n",(long)p);
#line 484 "minor.brg"
{ ASM(pfTEXT pfGLOBL pfALIGN pfLABEL, "_main", pfFUNC, "_main"); IDpush(); }
		break;
	case 4: /* decls: DECLS(decls,decl) */
		fprintf(stderr, "0x%lx: line 486: decls: DECLS(decls,decl)\n",(long)p);
#line 486 "minor.brg"
{}
		break;
	case 5: /* decls: NIL */
		fprintf(stderr, "0x%lx: line 487: decls: NIL\n",(long)p);
#line 487 "minor.brg"
{}
		break;
	case 6: /* decl: F_RET(type,f_id) */
		fprintf(stderr, "0x%lx: line 489: decl: F_RET(type,f_id)\n",(long)p);
#line 489 "minor.brg"
{ if (p->info == F_PUBLIC) ASM(pfGLOBL, fName, pfFUNC); fName = 0; }
		break;
	case 7: /* decl: VAR(V_TYPE(type,V_ID(ID,size)),init) */
		fprintf(stderr, "0x%lx: line 490: decl: VAR(V_TYPE(type,V_ID(ID,size)),init)\n",(long)p);
#line 490 "minor.brg"
{ globalVar(p); }
		break;
	case 8: /* size: NUMLIT */
		fprintf(stderr, "0x%lx: line 492: size: NUMLIT\n",(long)p);
#line 492 "minor.brg"
{ p->place = p->value.i; }
		break;
	case 9: /* size: NIL */
		fprintf(stderr, "0x%lx: line 493: size: NIL\n",(long)p);
#line 493 "minor.brg"
{ p->place = 0; }
		break;
	case 10: /* type: NUMBER */
		fprintf(stderr, "0x%lx: line 495: type: NUMBER\n",(long)p);
#line 495 "minor.brg"
{}
		break;
	case 11: /* type: STRING */
		fprintf(stderr, "0x%lx: line 496: type: STRING\n",(long)p);
#line 496 "minor.brg"
{}
		break;
	case 12: /* type: ARRAY */
		fprintf(stderr, "0x%lx: line 497: type: ARRAY\n",(long)p);
#line 497 "minor.brg"
{}
		break;
	case 13: /* type: VOID */
		fprintf(stderr, "0x%lx: line 499: type: VOID\n",(long)p);
#line 499 "minor.brg"
{}
		break;
	case 14: /* init: INIT(NUMLIT) */
		fprintf(stderr, "0x%lx: line 501: init: INIT(NUMLIT)\n",(long)p);
#line 501 "minor.brg"
{}
		break;
	case 15: /* init: INIT(CHRLIT) */
		fprintf(stderr, "0x%lx: line 502: init: INIT(CHRLIT)\n",(long)p);
#line 502 "minor.brg"
{}
		break;
	case 16: /* init: INIT(TXTLIT) */
		fprintf(stderr, "0x%lx: line 503: init: INIT(TXTLIT)\n",(long)p);
#line 503 "minor.brg"
{}
		break;
	case 17: /* init: INIT(array) */
		fprintf(stderr, "0x%lx: line 504: init: INIT(array)\n",(long)p);
#line 504 "minor.brg"
{}
		break;
	case 18: /* init: NIL */
		fprintf(stderr, "0x%lx: line 505: init: NIL\n",(long)p);
#line 505 "minor.brg"
{}
		break;
	case 19: /* array: ELEM(NIL,NUMLIT) */
		fprintf(stderr, "0x%lx: line 507: array: ELEM(NIL,NUMLIT)\n",(long)p);
#line 507 "minor.brg"
{ if (currentArray) free(currentArray); currentArray = (int*) malloc(sizeof(int));
										currentArraySize = 1; currentArray[0] = RIGHT_CHILD(p)->value.i; }
		break;
	case 20: /* array: ELEM(NUMLIT,NUMLIT) */
		fprintf(stderr, "0x%lx: line 509: array: ELEM(NUMLIT,NUMLIT)\n",(long)p);
#line 509 "minor.brg"
{ if (currentArray) free(currentArray); currentArray = (int*) malloc(2 * sizeof(int)); 
										currentArraySize = 2; currentArray[0] = LEFT_CHILD(p)->value.i; currentArray[1] = RIGHT_CHILD(p)->value.i; }
		break;
	case 21: /* array: ELEM(array,NUMLIT) */
		fprintf(stderr, "0x%lx: line 511: array: ELEM(array,NUMLIT)\n",(long)p);
#line 511 "minor.brg"
{ currentArray = (int*) realloc(currentArray, (++currentArraySize) * sizeof(int));
										currentArray[currentArraySize - 1] = RIGHT_CHILD(p)->value.i; }
		break;
	case 22: /* f_id: F_ID(f_id_do,F_PARAMS(params_end,DO(body))) */
		fprintf(stderr, "0x%lx: line 514: f_id: F_ID(f_id_do,F_PARAMS(params_end,DO(body)))\n",(long)p);
#line 514 "minor.brg"
{ IDpop(); ASM(pfPOP pfLEAVE pfRET); }
		break;
	case 23: /* f_id: F_ID(f_id_done,F_PARAMS(params_end,DONE)) */
		fprintf(stderr, "0x%lx: line 515: f_id: F_ID(f_id_done,F_PARAMS(params_end,DONE))\n",(long)p);
#line 515 "minor.brg"
{ IDpop(); }
		break;
	case 24: /* f_id_do: ID */
		fprintf(stderr, "0x%lx: line 517: f_id_do: ID\n",(long)p);
#line 517 "minor.brg"
{ newFunc(mkfunc(p->value.s), 0); IDpush(); }
		break;
	case 25: /* f_id_done: ID */
		fprintf(stderr, "0x%lx: line 519: f_id_done: ID\n",(long)p);
#line 519 "minor.brg"
{ newFunc(mkfunc(p->value.s), F_FORWARD); IDpush(); }
		break;
	case 26: /* params_end: params */
		fprintf(stderr, "0x%lx: line 521: params_end: params\n",(long)p);
#line 521 "minor.brg"
{ setParamsSize(fName, p->place - 8); }
		break;
	case 27: /* params: PARAM(params,V_TYPE(type,V_ID(ID,size))) */
		fprintf(stderr, "0x%lx: line 523: params: PARAM(params,V_TYPE(type,V_ID(ID,size)))\n",(long)p);
#line 523 "minor.brg"
{ int type = LEFT_CHILD(RIGHT_CHILD(p))->info;
															int place = LEFT_CHILD(p)->place;
															newVar(LEFT_CHILD(RIGHT_CHILD(RIGHT_CHILD(p)))->value.s,
																type, 0, place);
															p->place = place + getSize(type); }
		break;
	case 28: /* params: NIL */
		fprintf(stderr, "0x%lx: line 528: params: NIL\n",(long)p);
#line 528 "minor.brg"
{ p->place = 8; }
		break;
	case 29: /* body: BODY(vars_end,block) */
		fprintf(stderr, "0x%lx: line 530: body: BODY(vars_end,block)\n",(long)p);
#line 530 "minor.brg"
{}
		break;
	case 30: /* vars_end: vars */
		fprintf(stderr, "0x%lx: line 532: vars_end: vars\n",(long)p);
#line 532 "minor.brg"
{ enterFunction(p); }
		break;
	case 31: /* vars: VARS(vars,V_TYPE(type,V_ID(ID,size))) */
		fprintf(stderr, "0x%lx: line 534: vars: VARS(vars,V_TYPE(type,V_ID(ID,size)))\n",(long)p);
#line 534 "minor.brg"
{ int type = LEFT_CHILD(RIGHT_CHILD(p))->info;
															p->place = LEFT_CHILD(p)->place + getSize(type);
															int arraySize = RIGHT_CHILD(RIGHT_CHILD(RIGHT_CHILD(p)))->place;
															if (arraySize > 0) {
																p->place += arraySize * 4;
																addLocalArray(-p->place);
															}
															newVar(LEFT_CHILD(RIGHT_CHILD(RIGHT_CHILD(p)))->value.s,
																type, 0, -p->place); }
		break;
	case 32: /* vars: NIL */
		fprintf(stderr, "0x%lx: line 543: vars: NIL\n",(long)p);
#line 543 "minor.brg"
{ resetLocalArrays(); p->place = 0; }
		break;
	case 33: /* block: BLOCK(instrs,endinstr) */
		fprintf(stderr, "0x%lx: line 545: block: BLOCK(instrs,endinstr)\n",(long)p);
#line 545 "minor.brg"
{}
		break;
	case 34: /* instrs: INSTR(instrs,instr) */
		fprintf(stderr, "0x%lx: line 547: instrs: INSTR(instrs,instr)\n",(long)p);
#line 547 "minor.brg"
{}
		break;
	case 35: /* instrs: NIL */
		fprintf(stderr, "0x%lx: line 548: instrs: NIL\n",(long)p);
#line 548 "minor.brg"
{}
		break;
	case 36: /* instr: FI(elifs,ELSE(block)) */
		fprintf(stderr, "0x%lx: line 550: instr: FI(elifs,ELSE(block))\n",(long)p);
#line 550 "minor.brg"
{ ASM(pfLABEL, mklbl(LEFT_CHILD(p)->place)); }
		break;
	case 37: /* instr: FI(elifs_last,NIL) */
		fprintf(stderr, "0x%lx: line 551: instr: FI(elifs_last,NIL)\n",(long)p);
#line 551 "minor.brg"
{ ASM(pfLABEL, mklbl(LEFT_CHILD(p)->place)); }
		break;
	case 38: /* instr: FOR(for_init,until) */
		fprintf(stderr, "0x%lx: line 552: instr: FOR(for_init,until)\n",(long)p);
#line 552 "minor.brg"
{ ASM(pfLABEL, forAfterLabel()); popForLabel(); }
		break;
	case 39: /* instr: EXPR(expr) */
		fprintf(stderr, "0x%lx: line 553: instr: EXPR(expr)\n",(long)p);
#line 553 "minor.brg"
{ ASM(pfTRASH, 4); }
		break;
	case 40: /* instr: EXPR(expr) */
		fprintf(stderr, "0x%lx: line 554: instr: EXPR(expr)\n",(long)p);
#line 554 "minor.brg"
{ ASM(pfTRASH, pfWORD); }
		break;
	case 41: /* instr: EXPR(expr) */
		fprintf(stderr, "0x%lx: line 555: instr: EXPR(expr)\n",(long)p);
#line 555 "minor.brg"
{}
		break;
	case 42: /* instr: PRINT(expr) */
		fprintf(stderr, "0x%lx: line 556: instr: PRINT(expr)\n",(long)p);
#line 556 "minor.brg"
{ printExpr(LEFT_CHILD(p)); }
		break;
	case 43: /* instr: ALLOC(alloc_size_str,lval) */
		fprintf(stderr, "0x%lx: line 557: instr: ALLOC(alloc_size_str,lval)\n",(long)p);
#line 557 "minor.brg"
{ ASM(pfSTORE); }
		break;
	case 44: /* instr: ALLOC(alloc_size_arr,lval) */
		fprintf(stderr, "0x%lx: line 558: instr: ALLOC(alloc_size_arr,lval)\n",(long)p);
#line 558 "minor.brg"
{ ASM(pfSTORE); }
		break;
	case 45: /* instr: ASSERT(expr) */
		fprintf(stderr, "0x%lx: line 559: instr: ASSERT(expr)\n",(long)p);
#line 559 "minor.brg"
{ ++lbl; ASM(pfJNZ, mklbl(lbl));
										sprintf(assertStr, "assert failed: line %i\n", p->info);
										++lbl; ASM(pfRODATA pfALIGN pfLABEL, mklbl(lbl)); outstr(assertStr); ASM(pfTEXT pfADDR, mklbl(lbl));
										ASM(pfEXTRN pfCALL pfTRASH, "_prints", "_prints", pfWORD);
										ASM(pfLABEL, mklbl(lbl - 1)); }
		break;
	case 46: /* alloc_size_str: expr */
		fprintf(stderr, "0x%lx: line 565: alloc_size_str: expr\n",(long)p);
#line 565 "minor.brg"
{ ASM(pfALLOC pfSP); }
		break;
	case 47: /* alloc_size_arr: expr */
		fprintf(stderr, "0x%lx: line 566: alloc_size_arr: expr\n",(long)p);
#line 566 "minor.brg"
{ ASM(pfIMM pfMUL pfALLOC pfSP, 4); }
		break;
	case 48: /* elifs: IF(cond,block) */
		fprintf(stderr, "0x%lx: line 569: elifs: IF(cond,block)\n",(long)p);
#line 569 "minor.brg"
{ p->place = ++lbl; ASM(pfJMP pfLABEL, 
											mklbl(p->place), mklbl(LEFT_CHILD(p)->place)); }
		break;
	case 49: /* elifs: ELIFS(elifs,ELIF(cond,block)) */
		fprintf(stderr, "0x%lx: line 571: elifs: ELIFS(elifs,ELIF(cond,block))\n",(long)p);
#line 571 "minor.brg"
{ p->place = LEFT_CHILD(p)->place; ASM(pfJMP pfLABEL,
											mklbl(p->place), mklbl(LEFT_CHILD(RIGHT_CHILD(p))->place)); }
		break;
	case 50: /* elifs_last: IF(cond,block) */
		fprintf(stderr, "0x%lx: line 575: elifs_last: IF(cond,block)\n",(long)p);
#line 575 "minor.brg"
{ p->place = LEFT_CHILD(p)->place; }
		break;
	case 51: /* elifs_last: ELIFS(elifs,ELIF(cond,block)) */
		fprintf(stderr, "0x%lx: line 576: elifs_last: ELIFS(elifs,ELIF(cond,block))\n",(long)p);
#line 576 "minor.brg"
{ p->place = LEFT_CHILD(p)->place; ASM(pfLABEL,
													mklbl(LEFT_CHILD(RIGHT_CHILD(p))->place)); }
		break;
	case 52: /* cond: expr */
		fprintf(stderr, "0x%lx: line 579: cond: expr\n",(long)p);
#line 579 "minor.brg"
{ p->place = ++lbl; ASM(pfJZ, mklbl(lbl)); }
		break;
	case 53: /* for_init: expr */
		fprintf(stderr, "0x%lx: line 581: for_init: expr\n",(long)p);
#line 581 "minor.brg"
{ ASM(pfTRASH pfLABEL, getSize(p->info), mklbl(++lbl)); pushForLabels(lbl, lbl + 1, lbl + 2); lbl += 2; }
		break;
	case 54: /* until: UNTIL(until_expr,step) */
		fprintf(stderr, "0x%lx: line 582: until: UNTIL(until_expr,step)\n",(long)p);
#line 582 "minor.brg"
{}
		break;
	case 55: /* until_expr: expr */
		fprintf(stderr, "0x%lx: line 583: until_expr: expr\n",(long)p);
#line 583 "minor.brg"
{ ASM(pfJNZ, forAfterLabel());}
		break;
	case 56: /* step: STEP(step_block,expr) */
		fprintf(stderr, "0x%lx: line 584: step: STEP(step_block,expr)\n",(long)p);
#line 584 "minor.brg"
{ ASM(pfTRASH pfJMP, getSize(RIGHT_CHILD(p)->info), forLabel()); }
		break;
	case 57: /* step_block: block */
		fprintf(stderr, "0x%lx: line 585: step_block: block\n",(long)p);
#line 585 "minor.brg"
{ ASM(pfLABEL, forIncrLabel()); }
		break;
	case 58: /* endinstr: REPEAT */
		fprintf(stderr, "0x%lx: line 587: endinstr: REPEAT\n",(long)p);
#line 587 "minor.brg"
{ ASM(pfJMP, forIncrLabel()); }
		break;
	case 59: /* endinstr: STOP */
		fprintf(stderr, "0x%lx: line 588: endinstr: STOP\n",(long)p);
#line 588 "minor.brg"
{ ASM(pfJMP, forAfterLabel()); }
		break;
	case 60: /* endinstr: RETURN(NIL) */
		fprintf(stderr, "0x%lx: line 589: endinstr: RETURN(NIL)\n",(long)p);
#line 589 "minor.brg"
{ ASM(pfPOP pfLEAVE pfRET); }
		break;
	case 61: /* endinstr: RETURN(expr) */
		fprintf(stderr, "0x%lx: line 590: endinstr: RETURN(expr)\n",(long)p);
#line 590 "minor.brg"
{ ASM(pfPOP pfLEAVE pfRET); }
		break;
	case 62: /* endinstr: NIL */
		fprintf(stderr, "0x%lx: line 591: endinstr: NIL\n",(long)p);
#line 591 "minor.brg"
{}
		break;
	case 63: /* lval: ID */
		fprintf(stderr, "0x%lx: line 593: lval: ID\n",(long)p);
#line 593 "minor.brg"
{ parseID(p, 0); }
		break;
	case 64: /* lval: INDEX(id_idx,expr) */
		fprintf(stderr, "0x%lx: line 594: lval: INDEX(id_idx,expr)\n",(long)p);
#line 594 "minor.brg"
{ parseIndex(p); }
		break;
	case 65: /* id_idx: ID */
		fprintf(stderr, "0x%lx: line 596: id_idx: ID\n",(long)p);
#line 596 "minor.brg"
{ parseID(p, 1); }
		break;
	case 66: /* expr: INDEX(id_idx,expr) */
		fprintf(stderr, "0x%lx: line 598: expr: INDEX(id_idx,expr)\n",(long)p);
#line 598 "minor.brg"
{ loadIndex(p); }
		break;
	case 67: /* expr: ID */
		fprintf(stderr, "0x%lx: line 599: expr: ID\n",(long)p);
#line 599 "minor.brg"
{ loadID(p); }
		break;
	case 68: /* expr: NUMLIT */
		fprintf(stderr, "0x%lx: line 600: expr: NUMLIT\n",(long)p);
#line 600 "minor.brg"
{ ASM(pfIMM, p->value.i); }
		break;
	case 69: /* expr: CHRLIT */
		fprintf(stderr, "0x%lx: line 601: expr: CHRLIT\n",(long)p);
#line 601 "minor.brg"
{ ASM(pfIMM, p->value.i); }
		break;
	case 70: /* expr: TXTLIT */
		fprintf(stderr, "0x%lx: line 602: expr: TXTLIT\n",(long)p);
#line 602 "minor.brg"
{ lbl++; ASM(pfRODATA pfALIGN pfLABEL, mklbl(lbl)); outstr(p->value.s); ASM(pfTEXT pfADDR, mklbl(lbl));}
		break;
	case 71: /* expr: INPUT */
		fprintf(stderr, "0x%lx: line 603: expr: INPUT\n",(long)p);
#line 603 "minor.brg"
{ ASM(pfEXTRN pfCALL pfPUSH, "_readi", "_readi"); }
		break;
	case 72: /* expr: CALL(ID,args) */
		fprintf(stderr, "0x%lx: line 604: expr: CALL(ID,args)\n",(long)p);
#line 604 "minor.brg"
{ char* funcName = mkfunc(LEFT_CHILD(p)->value.s); ASM(pfCALL, funcName);
										void* data; IDfind(funcName, &data); Function* f = (Function*) data;
										if (f->paramsSize > 0) ASM(pfTRASH, f->paramsSize);
										if (f->retType != NIL) ASM(pfPUSH); }
		break;
	case 73: /* expr: ADDR(lval) */
		fprintf(stderr, "0x%lx: line 608: expr: ADDR(lval)\n",(long)p);
#line 608 "minor.brg"
{}
		break;
	case 74: /* expr: UMINUS(expr) */
		fprintf(stderr, "0x%lx: line 609: expr: UMINUS(expr)\n",(long)p);
#line 609 "minor.brg"
{ ASM(pfNEG); }
		break;
	case 75: /* expr: POW(expr,expr) */
		fprintf(stderr, "0x%lx: line 610: expr: POW(expr,expr)\n",(long)p);
#line 610 "minor.brg"
{ ASM(pfEXTRN pfCALL pfTRASH pfPUSH, "_pow", "_pow", 8); }
		break;
	case 76: /* expr: MUL(expr,expr) */
		fprintf(stderr, "0x%lx: line 611: expr: MUL(expr,expr)\n",(long)p);
#line 611 "minor.brg"
{ ASM(pfMUL); }
		break;
	case 77: /* expr: DIV(expr,expr) */
		fprintf(stderr, "0x%lx: line 612: expr: DIV(expr,expr)\n",(long)p);
#line 612 "minor.brg"
{ ASM(pfDIV); }
		break;
	case 78: /* expr: MOD(expr,expr) */
		fprintf(stderr, "0x%lx: line 613: expr: MOD(expr,expr)\n",(long)p);
#line 613 "minor.brg"
{ ASM(pfMOD); }
		break;
	case 79: /* expr: ADD(expr,expr) */
		fprintf(stderr, "0x%lx: line 615: expr: ADD(expr,expr)\n",(long)p);
#line 615 "minor.brg"
{ ASM(pfADD); }
		break;
	case 80: /* expr: ADD(exprX4,expr) */
		fprintf(stderr, "0x%lx: line 616: expr: ADD(exprX4,expr)\n",(long)p);
#line 616 "minor.brg"
{ ASM(pfADD); }
		break;
	case 81: /* expr: ADD(expr,exprX4) */
		fprintf(stderr, "0x%lx: line 617: expr: ADD(expr,exprX4)\n",(long)p);
#line 617 "minor.brg"
{ ASM(pfADD); }
		break;
	case 82: /* expr: SUB(expr,expr) */
		fprintf(stderr, "0x%lx: line 619: expr: SUB(expr,expr)\n",(long)p);
#line 619 "minor.brg"
{ ASM(pfSUB); }
		break;
	case 83: /* expr: SUB(exprX4,expr) */
		fprintf(stderr, "0x%lx: line 620: expr: SUB(exprX4,expr)\n",(long)p);
#line 620 "minor.brg"
{ ASM(pfSUB); }
		break;
	case 84: /* expr: SUB(expr,exprX4) */
		fprintf(stderr, "0x%lx: line 621: expr: SUB(expr,exprX4)\n",(long)p);
#line 621 "minor.brg"
{ ASM(pfSUB); }
		break;
	case 85: /* expr: SUB(expr,expr) */
		fprintf(stderr, "0x%lx: line 622: expr: SUB(expr,expr)\n",(long)p);
#line 622 "minor.brg"
{ ASM(pfSUB pfIMM pfDIV, 4); }
		break;
	case 86: /* exprX4: expr */
		fprintf(stderr, "0x%lx: line 624: exprX4: expr\n",(long)p);
#line 624 "minor.brg"
{ ASM(pfIMM pfMUL, 4); }
		break;
	case 87: /* expr: LT(expr,expr) */
		fprintf(stderr, "0x%lx: line 626: expr: LT(expr,expr)\n",(long)p);
#line 626 "minor.brg"
{ASM(pfLT);}
		break;
	case 88: /* expr: GT(expr,expr) */
		fprintf(stderr, "0x%lx: line 627: expr: GT(expr,expr)\n",(long)p);
#line 627 "minor.brg"
{ASM(pfGT);}
		break;
	case 89: /* expr: LEQ(expr,expr) */
		fprintf(stderr, "0x%lx: line 628: expr: LEQ(expr,expr)\n",(long)p);
#line 628 "minor.brg"
{ASM(pfLE);}
		break;
	case 90: /* expr: GEQ(expr,expr) */
		fprintf(stderr, "0x%lx: line 629: expr: GEQ(expr,expr)\n",(long)p);
#line 629 "minor.brg"
{ASM(pfGE);}
		break;
	case 91: /* expr: EQ(expr,expr) */
		fprintf(stderr, "0x%lx: line 630: expr: EQ(expr,expr)\n",(long)p);
#line 630 "minor.brg"
{ASM(pfEQ);}
		break;
	case 92: /* expr: NEQ(expr,expr) */
		fprintf(stderr, "0x%lx: line 631: expr: NEQ(expr,expr)\n",(long)p);
#line 631 "minor.brg"
{ASM(pfNE);}
		break;
	case 93: /* expr: BOR(expr,expr) */
		fprintf(stderr, "0x%lx: line 633: expr: BOR(expr,expr)\n",(long)p);
#line 633 "minor.brg"
{ASM(pfOR);}
		break;
	case 94: /* expr: LT(expr,expr) */
		fprintf(stderr, "0x%lx: line 635: expr: LT(expr,expr)\n",(long)p);
#line 635 "minor.brg"
{ASM(pfEXTRN pfCALL pfTRASH pfPUSH pfIMM pfLT, "_strcmp", "_strcmp", 2 * pfWORD, 0);}
		break;
	case 95: /* expr: GT(expr,expr) */
		fprintf(stderr, "0x%lx: line 636: expr: GT(expr,expr)\n",(long)p);
#line 636 "minor.brg"
{ASM(pfEXTRN pfCALL pfTRASH pfPUSH pfIMM pfGT, "_strcmp", "_strcmp", 2 * pfWORD, 0);}
		break;
	case 96: /* expr: LEQ(expr,expr) */
		fprintf(stderr, "0x%lx: line 637: expr: LEQ(expr,expr)\n",(long)p);
#line 637 "minor.brg"
{ASM(pfEXTRN pfCALL pfTRASH pfPUSH pfIMM pfLE, "_strcmp", "_strcmp", 2 * pfWORD, 0);}
		break;
	case 97: /* expr: GEQ(expr,expr) */
		fprintf(stderr, "0x%lx: line 638: expr: GEQ(expr,expr)\n",(long)p);
#line 638 "minor.brg"
{ASM(pfEXTRN pfCALL pfTRASH pfPUSH pfIMM pfGE, "_strcmp", "_strcmp", 2 * pfWORD, 0);}
		break;
	case 98: /* expr: EQ(expr,expr) */
		fprintf(stderr, "0x%lx: line 639: expr: EQ(expr,expr)\n",(long)p);
#line 639 "minor.brg"
{ASM(pfEXTRN pfCALL pfTRASH pfPUSH pfIMM pfEQ, "_strcmp", "_strcmp", 2 * pfWORD, 0);}
		break;
	case 99: /* expr: NEQ(expr,expr) */
		fprintf(stderr, "0x%lx: line 640: expr: NEQ(expr,expr)\n",(long)p);
#line 640 "minor.brg"
{ASM(pfEXTRN pfCALL pfTRASH pfPUSH pfIMM pfNE, "_strcmp", "_strcmp", 2 * pfWORD, 0);}
		break;
	case 100: /* expr: NOT(expr) */
		fprintf(stderr, "0x%lx: line 642: expr: NOT(expr)\n",(long)p);
#line 642 "minor.brg"
{ ASM(pfIMM pfEQ, 0); }
		break;
	case 101: /* expr: AND(and,expr) */
		fprintf(stderr, "0x%lx: line 643: expr: AND(and,expr)\n",(long)p);
#line 643 "minor.brg"
{ ASM(pfLABEL, mklbl(LEFT_CHILD(p)->place)); }
		break;
	case 102: /* expr: OR(or,expr) */
		fprintf(stderr, "0x%lx: line 644: expr: OR(or,expr)\n",(long)p);
#line 644 "minor.brg"
{ ASM(pfLABEL, mklbl(LEFT_CHILD(p)->place)); }
		break;
	case 103: /* expr: ASSIGN(assign_value,INDEX(id_idx,expr)) */
		fprintf(stderr, "0x%lx: line 646: expr: ASSIGN(assign_value,INDEX(id_idx,expr))\n",(long)p);
#line 646 "minor.brg"
{ parseIndex(RIGHT_CHILD(p)); 
															storeIndex(LEFT_CHILD(RIGHT_CHILD(p))); }
		break;
	case 104: /* expr: ASSIGN(assign_value,lval) */
		fprintf(stderr, "0x%lx: line 648: expr: ASSIGN(assign_value,lval)\n",(long)p);
#line 648 "minor.brg"
{ ASM(pfSTORE); }
		break;
	case 105: /* assign_value: expr */
		fprintf(stderr, "0x%lx: line 651: assign_value: expr\n",(long)p);
#line 651 "minor.brg"
{ ASM(pfDUP); }
		break;
	case 106: /* and: expr */
		fprintf(stderr, "0x%lx: line 653: and: expr\n",(long)p);
#line 653 "minor.brg"
{ p->place = ++lbl; ASM(pfDUP pfJZ pfTRASH, mklbl(lbl), pfWORD); }
		break;
	case 107: /* or: expr */
		fprintf(stderr, "0x%lx: line 654: or: expr\n",(long)p);
#line 654 "minor.brg"
{ p->place = ++lbl; ASM(pfDUP pfJNZ pfTRASH, mklbl(lbl), pfWORD); }
		break;
	case 108: /* args: ARG(expr,args) */
		fprintf(stderr, "0x%lx: line 656: args: ARG(expr,args)\n",(long)p);
#line 656 "minor.brg"
{}
		break;
	case 109: /* args: NIL */
		fprintf(stderr, "0x%lx: line 657: args: NIL\n",(long)p);
#line 657 "minor.brg"
{}
		break;
	case 110: /* num: NUMLIT */
		fprintf(stderr, "0x%lx: line 660: num: NUMLIT\n",(long)p);
#line 660 "minor.brg"
{}
		break;
	case 111: /* num: CHRLIT */
		fprintf(stderr, "0x%lx: line 661: num: CHRLIT\n",(long)p);
#line 661 "minor.brg"
{}
		break;
	case 112: /* expr: ASSIGN(ID,ID) */
		fprintf(stderr, "0x%lx: line 663: expr: ASSIGN(ID,ID)\n",(long)p);
#line 663 "minor.brg"
{ loadID(LEFT_CHILD(p)); ASM(pfDUP); storeID(RIGHT_CHILD(p)); /* x := y */ }
		break;
	case 113: /* expr: ASSIGN(expr,ID) */
		fprintf(stderr, "0x%lx: line 664: expr: ASSIGN(expr,ID)\n",(long)p);
#line 664 "minor.brg"
{ ASM(pfDUP); storeID(RIGHT_CHILD(p)); /* x := 19 */ }
		break;
	case 114: /* instr: incdecInstr */
		fprintf(stderr, "0x%lx: line 667: instr: incdecInstr\n",(long)p);
#line 667 "minor.brg"
{}
		break;
	case 115: /* incdecInstr: ASSIGN(ADD(ID,num),ID) */
		fprintf(stderr, "0x%lx: line 668: incdecInstr: ASSIGN(ADD(ID,num),ID)\n",(long)p);
#line 668 "minor.brg"
{ incdec(RIGHT_CHILD(p), LEFT_CHILD(LEFT_CHILD(p)), '+'); }
		break;
	case 116: /* incdecInstr: ASSIGN(ADD(num,ID),ID) */
		fprintf(stderr, "0x%lx: line 669: incdecInstr: ASSIGN(ADD(num,ID),ID)\n",(long)p);
#line 669 "minor.brg"
{ incdec(RIGHT_CHILD(p), RIGHT_CHILD(LEFT_CHILD(p)), '+'); }
		break;
	case 117: /* incdecInstr: ASSIGN(SUB(ID,num),ID) */
		fprintf(stderr, "0x%lx: line 670: incdecInstr: ASSIGN(SUB(ID,num),ID)\n",(long)p);
#line 670 "minor.brg"
{ incdec(RIGHT_CHILD(p), LEFT_CHILD(LEFT_CHILD(p)), '-'); }
		break;
	case 118: /* incdecInstr: ASSIGN(SUB(num,ID),ID) */
		fprintf(stderr, "0x%lx: line 671: incdecInstr: ASSIGN(SUB(num,ID),ID)\n",(long)p);
#line 671 "minor.brg"
{ incdec(RIGHT_CHILD(p), RIGHT_CHILD(LEFT_CHILD(p)), '-'); }
		break;
	case 119: /* cond: LT(expr,expr) */
		fprintf(stderr, "0x%lx: line 674: cond: LT(expr,expr)\n",(long)p);
#line 674 "minor.brg"
{ p->place = ++lbl; ASM(pfJGE, mklbl(lbl)); }
		break;
	case 120: /* cond: GT(expr,expr) */
		fprintf(stderr, "0x%lx: line 675: cond: GT(expr,expr)\n",(long)p);
#line 675 "minor.brg"
{ p->place = ++lbl; ASM(pfJLE, mklbl(lbl)); }
		break;
	case 121: /* cond: LEQ(expr,expr) */
		fprintf(stderr, "0x%lx: line 676: cond: LEQ(expr,expr)\n",(long)p);
#line 676 "minor.brg"
{ p->place = ++lbl; ASM(pfJGT, mklbl(lbl)); }
		break;
	case 122: /* cond: GEQ(expr,expr) */
		fprintf(stderr, "0x%lx: line 677: cond: GEQ(expr,expr)\n",(long)p);
#line 677 "minor.brg"
{ p->place = ++lbl; ASM(pfJLT, mklbl(lbl)); }
		break;
	case 123: /* cond: EQ(expr,expr) */
		fprintf(stderr, "0x%lx: line 678: cond: EQ(expr,expr)\n",(long)p);
#line 678 "minor.brg"
{ p->place = ++lbl; ASM(pfJNE, mklbl(lbl)); }
		break;
	case 124: /* cond: NEQ(expr,expr) */
		fprintf(stderr, "0x%lx: line 679: cond: NEQ(expr,expr)\n",(long)p);
#line 679 "minor.brg"
{ p->place = ++lbl; ASM(pfJEQ, mklbl(lbl)); }
		break;
	case 125: /* expr: numConst */
		fprintf(stderr, "0x%lx: line 682: expr: numConst\n",(long)p);
#line 682 "minor.brg"
{ p->info = NUMBER; ASM(pfIMM, (int) p->place); }
		break;
	case 126: /* numConst: num */
		fprintf(stderr, "0x%lx: line 683: numConst: num\n",(long)p);
#line 683 "minor.brg"
{ p->place = p->value.i; }
		break;
	case 127: /* numConst: UMINUS(numConst) */
		fprintf(stderr, "0x%lx: line 685: numConst: UMINUS(numConst)\n",(long)p);
#line 685 "minor.brg"
{ p->place = -LEFT_CHILD(p)->place; }
		break;
	case 128: /* numConst: POW(numConst,numConst) */
		fprintf(stderr, "0x%lx: line 686: numConst: POW(numConst,numConst)\n",(long)p);
#line 686 "minor.brg"
{ p->place = power(LEFT_CHILD(p)->place, RIGHT_CHILD(p)->place); }
		break;
	case 129: /* numConst: MUL(numConst,numConst) */
		fprintf(stderr, "0x%lx: line 687: numConst: MUL(numConst,numConst)\n",(long)p);
#line 687 "minor.brg"
{ p->place = LEFT_CHILD(p)->place * RIGHT_CHILD(p)->place; }
		break;
	case 130: /* numConst: DIV(numConst,numConst) */
		fprintf(stderr, "0x%lx: line 688: numConst: DIV(numConst,numConst)\n",(long)p);
#line 688 "minor.brg"
{ p->place = LEFT_CHILD(p)->place / RIGHT_CHILD(p)->place; }
		break;
	case 131: /* numConst: MOD(numConst,numConst) */
		fprintf(stderr, "0x%lx: line 689: numConst: MOD(numConst,numConst)\n",(long)p);
#line 689 "minor.brg"
{ p->place = LEFT_CHILD(p)->place % RIGHT_CHILD(p)->place; }
		break;
	case 132: /* numConst: ADD(numConst,numConst) */
		fprintf(stderr, "0x%lx: line 690: numConst: ADD(numConst,numConst)\n",(long)p);
#line 690 "minor.brg"
{ p->place = LEFT_CHILD(p)->place + RIGHT_CHILD(p)->place; }
		break;
	case 133: /* numConst: LT(numConst,numConst) */
		fprintf(stderr, "0x%lx: line 691: numConst: LT(numConst,numConst)\n",(long)p);
#line 691 "minor.brg"
{ p->place = LEFT_CHILD(p)->place < RIGHT_CHILD(p)->place; }
		break;
	case 134: /* numConst: GT(numConst,numConst) */
		fprintf(stderr, "0x%lx: line 692: numConst: GT(numConst,numConst)\n",(long)p);
#line 692 "minor.brg"
{ p->place = LEFT_CHILD(p)->place > RIGHT_CHILD(p)->place; }
		break;
	case 135: /* numConst: LEQ(numConst,numConst) */
		fprintf(stderr, "0x%lx: line 693: numConst: LEQ(numConst,numConst)\n",(long)p);
#line 693 "minor.brg"
{ p->place = LEFT_CHILD(p)->place <= RIGHT_CHILD(p)->place; }
		break;
	case 136: /* numConst: GEQ(numConst,numConst) */
		fprintf(stderr, "0x%lx: line 694: numConst: GEQ(numConst,numConst)\n",(long)p);
#line 694 "minor.brg"
{ p->place = LEFT_CHILD(p)->place >= RIGHT_CHILD(p)->place; }
		break;
	case 137: /* numConst: EQ(numConst,numConst) */
		fprintf(stderr, "0x%lx: line 695: numConst: EQ(numConst,numConst)\n",(long)p);
#line 695 "minor.brg"
{ p->place = LEFT_CHILD(p)->place == RIGHT_CHILD(p)->place; }
		break;
	case 138: /* numConst: NOT(numConst) */
		fprintf(stderr, "0x%lx: line 696: numConst: NOT(numConst)\n",(long)p);
#line 696 "minor.brg"
{ p->place = !LEFT_CHILD(p)->place; }
		break;
	case 139: /* numConst: AND(numConst,numConst) */
		fprintf(stderr, "0x%lx: line 697: numConst: AND(numConst,numConst)\n",(long)p);
#line 697 "minor.brg"
{ p->place = LEFT_CHILD(p)->place && RIGHT_CHILD(p)->place; }
		break;
	case 140: /* numConst: OR(numConst,numConst) */
		fprintf(stderr, "0x%lx: line 698: numConst: OR(numConst,numConst)\n",(long)p);
#line 698 "minor.brg"
{ p->place = LEFT_CHILD(p)->place || RIGHT_CHILD(p)->place; }
		break;
	case 141: /* numConst: LT(CHRLIT,CHRLIT) */
		fprintf(stderr, "0x%lx: line 700: numConst: LT(CHRLIT,CHRLIT)\n",(long)p);
#line 700 "minor.brg"
{ p->place = strcmp(LEFT_CHILD(p)->value.s, RIGHT_CHILD(p)->value.s) < 0; }
		break;
	case 142: /* numConst: GT(CHRLIT,CHRLIT) */
		fprintf(stderr, "0x%lx: line 701: numConst: GT(CHRLIT,CHRLIT)\n",(long)p);
#line 701 "minor.brg"
{ p->place = strcmp(LEFT_CHILD(p)->value.s, RIGHT_CHILD(p)->value.s) > 0; }
		break;
	case 143: /* numConst: LEQ(CHRLIT,CHRLIT) */
		fprintf(stderr, "0x%lx: line 702: numConst: LEQ(CHRLIT,CHRLIT)\n",(long)p);
#line 702 "minor.brg"
{ p->place = strcmp(LEFT_CHILD(p)->value.s, RIGHT_CHILD(p)->value.s) <= 0; }
		break;
	case 144: /* numConst: GEQ(CHRLIT,CHRLIT) */
		fprintf(stderr, "0x%lx: line 703: numConst: GEQ(CHRLIT,CHRLIT)\n",(long)p);
#line 703 "minor.brg"
{ p->place = strcmp(LEFT_CHILD(p)->value.s, RIGHT_CHILD(p)->value.s) >= 0; }
		break;
	case 145: /* numConst: EQ(CHRLIT,CHRLIT) */
		fprintf(stderr, "0x%lx: line 704: numConst: EQ(CHRLIT,CHRLIT)\n",(long)p);
#line 704 "minor.brg"
{ p->place = strcmp(LEFT_CHILD(p)->value.s, RIGHT_CHILD(p)->value.s) == 0; }
		break;
	case 146: /* numConst: NEQ(CHRLIT,CHRLIT) */
		fprintf(stderr, "0x%lx: line 705: numConst: NEQ(CHRLIT,CHRLIT)\n",(long)p);
#line 705 "minor.brg"
{ p->place = strcmp(LEFT_CHILD(p)->value.s, RIGHT_CHILD(p)->value.s) != 0; }
		break;
	default: break;
  }
}

int yyselect(NODEPTR_TYPE p)
{
	yylabel(p,p);
	if (((struct yystate *)STATE_LABEL(p))->rule.yyfile == 0) {
		fprintf(stderr, "No match for start symbol (%s).\n", yyntname[1]);
		return 1;
	}
	yyreduce(p, 1);
	return 0;
}


#line 706 "minor.brg"


extern int trace, errors, debugNode;
void evaluate(Node *p) {
	if (errors) return;
	if (trace) printNode(p, stdout, yynames);
	if (!yyselect(p) && trace) printf("selection successful\n");
}

#ifndef NOTRACE
static void yytrace(NODEPTR_TYPE p, int eruleno, int cost, int bestcost)
{
	int op = OP_LABEL(p);
	YYCONST char *tname = yytermname[op] ? yytermname[op] : "?";
	if (debugNode) fprintf(stderr, "0x%p:%s matched %s with cost %d vs. %d\n", p, tname, yystring[eruleno], cost, bestcost);
	if (cost >= MAX_COST && bestcost >= MAX_COST) {
		fprintf(stderr, "0x%p:%s NO MATCH %s with cost %d vs. %d\n", p, tname, yystring[eruleno], cost, bestcost);
		printNode(p, stderr, yynames);
	}
}
#endif
