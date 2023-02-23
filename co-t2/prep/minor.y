%{
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "lib/node.h"
#include "lib/tabid.h"
#include "minor.h"

int yylex();
void yyerror(char* s);
int errors;
int evaluate(Node* n);

static long ifDepth = 0;
static long cycleDepth = 0;
static char* fName; /* 0 -> Outside function, "" -> main function */
static char errBuf[256];

typedef struct { 
	int type;
	char* name;
	char flags;
} Variable;

typedef struct {
	int retType;
	int numParams;
	int* paramTypes;
	char flags;
} Function;

typedef struct {
	int type;
	char isZero;
} ArgType;

%}

%union {
	int i;			/* integer value */
	char* s;		/* symbol name or string literal */
	Node* n;		/* node pointer */
}

%token PROGRAM MODULE START END
%token VOID CONST NUMBER ARRAY STRING FUNCTION PUBLIC FORWARD
%token IF THEN ELSE ELIF FI FOR UNTIL STEP DO DONE WHILE REPEAT STOP RETURN
%token ASSIGN LEQ GEQ NEQ

%token<i> NUMLIT CHRLIT
%token<s> TXTLIT ID



%right ASSIGN
%left '|'
%left '&'
%nonassoc '~'
%left '=' NEQ
%left '<' '>' LEQ GEQ
%left '+' '-'
%left '*' '/' '%' XOR
%right '^'
%nonassoc UMINUS ADDR
%nonassoc '(' '['

/* Tokens used on syntax tree only */
%token NIL ERR DECLS SIZE INIT PARAM
%token ARG INSTR BLOCK BODY VARS VAR
%token F_RET F_ID F_PARAMS
%token V_TYPE V_ID
%token ELIFS ELIF_COND START_WHILE

%type<i> qual const
%type<n> program module decls decl var type init lits lit
%type<n> array ret params impl body vars block instrs instr
%type<n> endinstr elifs else lval expr args

%%

file		: program							{ if (!errors) evaluate($1); /*printNode($1, 0, (char**) yynames);*/ }
			| module							{ if (!errors) evaluate($1); /*printNode($1, 0, (char**) yynames);*/ }
			;

program		: PROGRAM decls START 				{ IDpush(); fName = ""; }
			  body END 						    { $$ = binNode(PROGRAM, $2, $5); IDpop(); }
			| PROGRAM START						{ IDpush(); fName = ""; }
			  body END							{ $$ = binNode(PROGRAM, nilNode(NIL), $4); IDpop(); }
			| PROGRAM error START				{ IDpush(); fName = ""; }
			  body END							{ yyerrok; $$ = binNode(PROGRAM, nilNode(ERR), $5); IDpop(); }
			;		

module		: MODULE decls END                  { $$ = uniNode(MODULE, $2); }
			| MODULE END						{ $$ = uniNode(MODULE, nilNode(NIL)); }
			;

decls		: decls ';' decl					{ $$ = binNode(DECLS, $1, $3); }
			| error ';' decl                    { yyerrok; $$ = binNode(DECLS, nilNode(ERR), $3); }
			| decl                              { $$ = binNode(DECLS, nilNode(NIL), $1); } 
			;

decl		: FUNCTION qual ret ID		 		{ IDpush(); fName = $4; }
			  params 							{ createFunction($2, $3, $4, $6); }
			  impl								{ $$ = funcNode($3, strNode(ID, $4), $6, $8);
			  									  $$->info = $2;
												  addFuncImpl($8); IDpop(); }

			| FUNCTION qual ret ID 				{ IDpush(); fName = $4; createFunction($2, $3, $4, 0); }
			  impl								{ $$ = funcNode($3, strNode(ID, $4), nilNode(NIL), $6);
			  									  $$->info = $2;
												  addFuncImpl($6); IDpop(); }

	  		| qual const var init				{ $$ = binNode(VAR, $3, $4);
			  									  $$->info = $1 + $2;
												  createVar($1, $2, $3, $4); }
			;

qual		: PUBLIC							{ $$ = F_PUBLIC; }
	  		| FORWARD							{ $$ = F_FORWARD; }
			| /**/								{ $$ = 0; }
			;

const		: CONST								{ $$ = F_CONST; }
	   		| /**/								{ $$ = 0; }
			;

var			: type ID '[' NUMLIT ']'			{ $$ = varNode($1, strNode(ID, $2),
													intNode(NUMLIT, $4)); 
												  declareVar($$, $1, $2, $4); }
			| type ID							{ $$ = varNode($1, strNode(ID, $2),
													nilNode(NIL)); 
												  declareVar($$, $1, $2, -1); }
	  		;

type		: NUMBER							{ $$ = nilNode(NUMBER); $$->info = NUMBER; }
	  		| STRING							{ $$ = nilNode(STRING); $$->info = STRING; }
			| ARRAY								{ $$ = nilNode(ARRAY); $$->info = ARRAY; }
			;

init		: ASSIGN lit                        { $$ = uniNode(INIT, $2); initValue($$, $2->info, $2); }
			| ASSIGN lits						{ $$ = uniNode(INIT, $2); initValue($$, STRING, $2); }
	  		| ASSIGN array						{ $$ = uniNode(INIT, $2); initValue($$, ARRAY, $2); }
			| /**/								{ $$ = nilNode(NIL); initValue($$, NIL, 0); }
			;

lits		: lits lit							{ $$ = strNode(TXTLIT, 
												  getJoined($1->value.s, toStr($2))); 
												  free($1->value.s); free($1); }
	  		| lit lit							{ $$ = strNode(TXTLIT, 
												  getJoined(toStr($1), toStr($2))); }
			;

lit			: NUMLIT							{ $$ = intNode(NUMLIT, $1); $$->info = NUMBER;
												  $$->user = (void*) flagInt($1, NUMLIT); }
			| CHRLIT							{ $$ = intNode(CHRLIT, $1); $$->info = NUMBER; 
												  $$->user = (void*) flagInt($1, CHRLIT); }
	  		| TXTLIT							{ $$ = strNode(TXTLIT, $1); $$->info = STRING; }
			;

array		: array ',' NUMLIT					{ $$ = binNode(',', $1, intNode(NUMLIT, $3)); 
												  $$->info = $1->info + 1; }
	  		| NUMLIT ',' NUMLIT					{ $$ = binNode(',', intNode(NUMLIT, $1), 
													intNode(NUMLIT, $3)); 
												  $$->info = 2; }
			;

ret			: type
	  		| VOID								{ $$ = nilNode(VOID); $$->info = VOID; }
			;

params		: params ';' var					{ $$ = binNode(PARAM, $1, $3); addParam($$, $1, $3); }					
	  		| var								{ $$ = binNode(PARAM, nilNode(NIL), $1); initParams($$, $1); }
			;

impl		: DONE								{ $$ = nilNode(DONE); $$->info = DONE; }
			| DO body							{ $$ = uniNode(DO, $2); $$->info = DO; }
			;

body		: vars block						{ $$ = binNode(BODY, $1, $2); }
			;

vars		: vars var ';'						{ $$ = binNode(VARS, $1, $2); }
			| vars error ';'					{ yyerrok; $$ = binNode(VARS, $1, nilNode(ERR)); }
			| /**/								{ $$ = nilNode(NIL); }
			;

block		: instrs endinstr					{ $$ = binNode(BLOCK, $1, $2); }
			;

instrs		: instrs instr						{ $$ = binNode(INSTR, $1, $2); }
			| /**/								{ $$ = nilNode(NIL); }
			;

instr		: elifs else FI								{ $$ = binNode(FI, $1, $2); ifDepth --; }	
			| FOR										{ cycleDepth ++; }
			  expr UNTIL expr STEP expr DO block DONE	{ $$ = forNode($3, $5, $7, $9); 
														  cycleDepth --; if ($5->info == NIL)
															  yyerror("Invalid type (NIL) for 'until' test expression"); }
			| WHILE 									{ cycleDepth --; }
			  expr DO block DONE						{ $$ = binNode(START_WHILE, nilNode(NIL), binNode(WHILE, $3, $5)); 
			  												cycleDepth --; if ($5->info == NIL)
															  yyerror("Invalid type (NIL) for 'while' test expression");}
			| expr ';'									{ $$ = uniNode(';', $1); }
			| expr '!'									{ $$ = uniNode('!', $1); if ($1->info == NIL || $1->info == VOID)
															  yyerror("Can only print number, string or array. Got NIL"); }
			| lval '#' expr ';'     					{ $$ = binNode('#', $3, $1); handleAlloc($1, $3); }
			| IF error FI						{ yyerrok; $$ = ifNode(nilNode(ERR),
													nilNode(ERR), nilNode(ERR), nilNode(ERR)); }
			| FOR error DONE					{ yyerrok; $$ = forNode(nilNode(ERR), 
													nilNode(ERR), nilNode(ERR), nilNode(ERR)); }
			| expr error ';'					{ yyerrok; $$ = uniNode(';', nilNode(ERR)); }
			| expr error '!'					{ yyerrok; $$ = uniNode('!', nilNode(ERR)); }
			;

endinstr	: REPEAT							{ $$ = nilNode(REPEAT); 
												  if (cycleDepth <= 0) yyerror("Can't repeat outside loop"); }
			| STOP								{ $$ = nilNode(STOP); 
												  if (cycleDepth <= 0) yyerror("Can't stop outside loop"); }
			| RETURN							{ $$ = uniNode(RETURN, nilNode(NIL)); handleReturn(VOID); }
			| RETURN expr						{ $$ = uniNode(RETURN, $2); handleReturn($2->info); }
			| /**/								{ $$ = nilNode(NIL);}
			;

elifs		: IF 							{ ifDepth ++; }
			  expr THEN block				{ $$ = binNode(IF, $3, $5);
											  if ($3->info == NIL)
												  yyerror("Invalid type (NIL) for 'if' test expression"); }
			| elifs ELIF expr THEN block	{ $$ = binNode(ELIFS, $1, binNode(ELIF, $3, $5)); 
											  if ($3->info == NIL)
												  yyerror("Invalid type (NIL) for 'elif' test expression"); }
			;

else		: ELSE block					{ $$ = uniNode(ELSE, $2); }
			| /**/							{ $$ = nilNode(NIL); }
			;

lval		: ID '[' expr ']'				{ $$ = binNode('[', strNode(ID, $1), $3); 
											  handleIndex($$, $1, $3); } 
			| ID							{ $$ = strNode(ID, $1); 
											  handleID($$, $1); }
			;

args		: args ',' expr					{ $$ = binNode(ARG, $3, $1); addArg($$, $1, $3); }
			| expr							{ $$ = binNode(ARG, $1, nilNode(NIL)); initArgs($$, $1); }
			;

expr		: lval							
			| lit							
			| lits							{ $$ = $1; $$->info = STRING; }
			| '(' expr ')'					{ $$ = $2; $2->user = 0; }
			| ID '(' args ')'               { $$ = binNode('(', strNode(ID, $1), $3);
											  handleCall($$, $1, $3); }
			| '?'							{ $$ = nilNode('?'); $$->info = NUMBER; }
			| '&' lval %prec ADDR			{ $$ = uniNode(ADDR, $2); handleUnOp('&', $$, $2); }
			| '-' expr %prec UMINUS			{ $$ = uniNode(UMINUS, $2); handleUnOp('-', $$, $2); }
			| expr '^' expr					{ $$ = binNode('^', $1, $3); handleBinOp('^', $$, $1, $3); }
			| expr '*' expr					{ $$ = binNode('*', $1, $3); handleBinOp('*', $$, $1, $3); }
			| expr '/' expr					{ $$ = binNode('/', $1, $3); handleBinOp('/', $$, $1, $3); }
			| expr '%' expr                 { $$ = binNode('%', $1, $3); handleBinOp('%', $$, $1, $3); }
			| expr '+' expr                 { $$ = binNode('+', $1, $3); handleBinOp('+', $$, $1, $3); }
			| expr '-' expr                 { $$ = binNode('-', $1, $3); handleBinOp('-', $$, $1, $3); }
			| expr '<' expr                 { $$ = binNode('<', $1, $3); handleBinOp('<', $$, $1, $3); }
			| expr '>' expr                 { $$ = binNode('>', $1, $3); handleBinOp('>', $$, $1, $3); }
			| expr LEQ expr                 { $$ = binNode(LEQ, $1, $3); handleBinOp(LEQ, $$, $1, $3); }
			| expr GEQ expr                 { $$ = binNode(GEQ, $1, $3); handleBinOp(GEQ, $$, $1, $3); }
			| expr '=' expr                 { $$ = binNode('=', $1, $3); handleBinOp('=', $$, $1, $3); }
			| expr NEQ expr                 { $$ = binNode(NEQ, $1, $3); handleBinOp(NEQ, $$, $1, $3); }
			| expr '~' expr %prec XOR		{ $$ = binNode(XOR, $1, $3); handleBinOp(XOR, $$, $1, $3); }
			| '~' expr						{ $$ = uniNode('~', $2); handleUnOp('~', $$, $2); }
			| expr '&' expr                 { $$ = binNode('&', $1, $3); handleBinOp('&', $$, $1, $3); }
			| expr '|' expr                 { $$ = binNode('|', $1, $3); handleBinOp('|', $$, $1, $3); }
			| lval ASSIGN expr				{ $$ = binNode(ASSIGN, $3, $1); handleBinOp(ASSIGN, $$, $1, $3); }
			;
%%

/***** Functions to convert to binNodes *****/

Node* funcNode(Node* retNode, Node* idNode, Node* paramsNode, Node* implNode) {
	return binNode(F_RET, retNode,
			   binNode(F_ID, idNode,
				   binNode(F_PARAMS, paramsNode, implNode)));
}

Node* varNode(Node* typeNode, Node* idNode, Node* sizeNode) {
	return binNode(V_TYPE, typeNode,
			   binNode(V_ID, idNode, sizeNode));

}

Node* ifNode(Node* condNode, Node* thenNode, Node* elifsNode, Node* elseNode) {
	return binNode(FI,
			   binNode(ELIFS,
				   binNode(IF, condNode, thenNode),
				   elifsNode),
			   elseNode);
}

Node* elifsNode(Node* prevElifsNode, Node* condNode, Node* thenNode) {
	return binNode(ELIF, prevElifsNode,
			   binNode(ELIF_COND, condNode, thenNode));
}

Node* forNode(Node* initNode, Node* untilNode, Node* stepNode, Node* doNode) {
	return binNode(FOR, initNode,
			   binNode(UNTIL, untilNode,
				   binNode(STEP, doNode, stepNode)));
}

/***** Other functions *****/

void yyerrorf(const char format[], int n, ...) {
	va_list va;
	va_start(va, n);
	vsprintf(errBuf, format, va);
	yyerror(errBuf);
	va_end(va);
}

char* newChar(char c) {
	char* new = (char*) malloc(sizeof(char));
	*new = c;
	return new;
}

int* newInt(int i) {
	int* new = (int*) malloc(sizeof(int));
	*new = i;
	return new;
}

char getFlags(Node* n) {
	return n->user ? *((char*) n->user) : 0;
}

char* flagInt(int value, int type) {
	char flags = 0;
	if (type == NUMLIT)	flags |= F_INTLIT;
	if (value == 0)		flags |= F_ZERO;
	return newChar(flags);
}

char* getJoined(char* s1, char* s2) {
	char* result = (char*) malloc(strlen(s1) + strlen(s2) + 1);
	*result = 0;
	strcat(result, s1);
	strcat(result, s2);
	return result;
}

char* toStr(Node* n) {
	if (n->info != STRING) {
		char* result = (char*) malloc(2); /* 1 char + \0 */
		sprintf(result, "%c", (char) (n->value.i & 0xFF)); 
		return result;
	}
	else /* STRING */	
		return n->value.s;
}

int stringOrArray(int type) {
	return type == STRING || type == ARRAY;
}

void declareVar(Node* varNode, Node* typeNode, char* id, int size) {
	Variable* var = (Variable*) malloc(sizeof(Variable));
	int type = typeNode->info;
	if (fName) /* Inside function */
		IDnew(type, id, var);
	
	var->type = typeNode->info;
	var->name = id;
	if (size >= 0) {
		if (type != ARRAY)
			yyerrorf("Can only declare size of array. Got: %s", 1, yyname[type]);
		else if (size == 0)
			yyerror("Array size can't be 0");
	}
	varNode->info = size;
	varNode->user = (void*) var;
}												  

void initValue(Node* initNode, int type, Node* valueNode) {
	initNode->info = type;

	switch (type) {
		case NUMBER: case ARRAY:
			initNode->user = (void*) valueNode; break;
	}
}

void createVar(int qualFlag, int constFlag, Node* varNode, Node* initNode) {
	Variable* var = (Variable*) varNode->user;
	
	char repeated = 0;	

	if (IDfind(var->name, (void**) IDtest) == -1)
		IDnew(var->type, var->name, (void*) var);
	else { /* Already exists */
		repeated = 1;
		void* data;
		int result = IDfind(var->name, &data);
		
		if (result == var->type) {
			Variable* oldVar = (Variable*) data;
			if (~oldVar->flags & F_FORWARD)
				yyerrorf("Can't override non-forward variable '%s'", 1, var->name);
			else /* Overriding forward variable: OK */
				oldVar->flags |= qualFlag;
			
			if (!(oldVar->flags & F_CONST) == (constFlag == F_CONST))
				yyerrorf("Declarations for variable '%s' differ on 'const' qualifier",
					1, var->name);
		}
		else
			yyerrorf("'%s': Already declared but with different type", 1, var->name);
	}

	/* Convert single number into array if needed */
	if (initNode->info == NUMBER && var->type == ARRAY) {
		Node* valueNode = (Node*) initNode->user;
		if (~getFlags(valueNode) & F_INTLIT)
			yyerror("Cannot initialize array with character");

		Node* arrayNode = binNode(',', nilNode(NIL), valueNode);
		arrayNode->info = 1;
		
		removeNode(initNode, 0);
		addNode(initNode, arrayNode, 0);
		initNode->user = arrayNode;
		initNode->info = ARRAY;
	}

	if (initNode->info != NIL && var->type != initNode->info)
		yyerrorf("Invalid initializer type: %s. Expected: %s", 
			2, yyname[initNode->info], yyname[var->type]);
	else if (initNode->info == ARRAY) {
		Node* arrayNode = (Node*) initNode->user;
		if (varNode->info != -1 && varNode->info < arrayNode->info) 
			yyerrorf("Array initializer has more elements (%i) than declared size (%i)",
				2, arrayNode->info, varNode->info);
	}

	if (initNode->info == NIL
     && constFlag == F_CONST
	 && qualFlag != F_FORWARD
	 && !(var->type == ARRAY && varNode->info != -1))
        yyerrorf("Constant non-forward variable '%s' is not initialized", 1, var->name);
	else if (initNode->info != NIL && qualFlag == F_FORWARD)
		yyerrorf("Forward variable '%s' can't be initialized", 1, var->name);
	
	if (repeated)
		free(var);	
	else /* Set flags */ {
		var->flags |= qualFlag | constFlag;
	}
}

void initParams(Node* paramsNode, Node* varNode) {
	Variable* var = (Variable*) varNode->user;
	int* paramTypes = (int*) malloc(sizeof(int));
	*paramTypes = var->type;
	
	paramsNode->user = (void*) paramTypes;
	paramsNode->info = 1; /* Number of parameters */
}

void addParam(Node* newParamsNode, Node* oldParamsNode, Node* varNode) {	
	Variable* var = (Variable*) varNode->user;
	int newSize = oldParamsNode->info + 1;
	int* paramTypes = (int*) realloc(oldParamsNode->user, newSize * sizeof(int));
	paramTypes[newSize - 1] = var->type;
	
	newParamsNode->user = (void*) paramTypes;
	newParamsNode->info = newSize;
}

Node* createFunction(int qualFlag, Node* retNode, char* id, Node* paramsNode) {
	Function* f = (Function*) malloc(sizeof(Function));
	f->flags = qualFlag;
	f->retType = retNode->info;

	if (paramsNode) {
		f->numParams = paramsNode->info;
		f->paramTypes = (int*) paramsNode->user;
	}
	else {
		f->numParams = 0;
		f->paramTypes = 0;
	}
	
	char exists = 0;
    if (IDsearch(id, (void**) IDtest, 1, 0) == -1)
		IDinsert(IDlevel() - 1, FUNCTION, id, (void*) f);
    else {
        exists = 1;
		void* data;
        int result = IDsearch(id, &data, 1, 0);

        if (result == FUNCTION) {
            Function* oldF = (Function*) data;
            if (~oldF->flags & F_FORWARD)
                yyerrorf("Can't override non-forward function '%s'", 1, id);
            else /* Non-forward overriding forward: OK */ {
				oldF->flags &= ~F_FORWARD;
                oldF->flags |= qualFlag;
			}

			if (f->retType != oldF->retType)
				yyerrorf("Return type for function '%s' does not match with previous declaration", 1, id);
			if (f->numParams != oldF->numParams)
				yyerrorf("Number of parameters for function '%s' doesn't match with previous declaration", 1, id);
			else for (int i = 0; i < f->numParams; i ++)
				if (f->paramTypes[i] != oldF->paramTypes[i])
					yyerrorf("Type of parameter %i for function '%s' does not match with previous declaration",
						2, i + 1, id);
        }
        else
            yyerrorf("'%s': Already declared but with different type (%s)", 2, id, yyname[result]);
		
    }

	if (exists) {
		if (paramsNode)
			free(paramsNode->user);
		free(f);
	}
}

void addFuncImpl(Node* implNode) {
	Function* f;
	IDfind(fName, (void**) &f);
	if (~f->flags & F_FORWARD && implNode->info == DONE)
		yyerrorf("Non-forward function '%s' must have a body", 1, fName);
	else if (f->flags & F_FORWARD && implNode->info == DO)
		yyerrorf("Forward function '%s' can't have a body", 1, fName);
	fName = 0;
}

void handleCall(Node* callNode, char* id, Node* argsNode) {
	void* data;
	int result = IDsearch(id, &data, 1, 0);

	if (result == FUNCTION) {
		Function* f = (Function*) data;
		
		if (f->numParams != argsNode->info)
			yyerrorf("Wrong number of arguments: %i. Expected: %i", 
				2, argsNode->info, f->numParams);
		
		else {
			ArgType* argTypes = (ArgType*) argsNode->user;
			char correctArgs = 1;
			for (int i = 0; i < f->numParams; i ++)
				if (f->paramTypes[i] != argTypes[i].type && !argTypes[i].isZero) {
					correctArgs = 0;
					yyerrorf("Wrong type for argument %i of function '%s': %s. Expected: %s",
						4, i + 1, id, yyname[argTypes[i].type], yyname[f->paramTypes[i]]);
				}
			if (correctArgs) {
				if (f->retType == VOID)
					callNode->info = NIL;
				else
					callNode->info = f->retType;
				free(argsNode->user);
				return;
			}
		}
	}
	else if (result != -1)
		yyerrorf("'%s' of type %s is not callable", 2, id, yyname[result]);
	
	free(argsNode->user);
	callNode->info = NIL;
}

void initArgs(Node* argsNode, Node* exprNode) {
	ArgType* argTypes = (ArgType*) malloc(sizeof(ArgType));
	argTypes->type = exprNode->info;
	argTypes->isZero = getFlags(exprNode) & F_ZERO;
	
	argsNode->user = (void*) argTypes;
	argsNode->info = 1; /* Number of arguments */
}

void addArg(Node* newArgsNode, Node* oldArgsNode, Node* exprNode) {
	int oldSize = oldArgsNode->info;	
	int newSize = oldSize + 1;
	ArgType* argTypes = (ArgType*) realloc(oldArgsNode->user, newSize * sizeof(ArgType));
	argTypes[oldSize].type = exprNode->info;
	argTypes[oldSize].isZero = getFlags(exprNode) & F_ZERO;
	
	newArgsNode->user = (void*) argTypes;
	newArgsNode->info = newSize;
}

void handleReturn(int type) {
	int fType;
	if (fName[0] != '\0') {
		Function* f;
		IDfind(fName, (void**) &f);
		fType = f->retType;
	}
	else { /* Main function */
		if (!ifDepth && !cycleDepth)
			yyerror("Return outside any block in main function");
		fType = NUMBER;
	}
	
	if (fType != type)
		yyerrorf("Wrong function return type: %s. Expected: %s",
			2, yyname[type], yyname[fType]);
}

void handleIndex(Node* idxNode, char* id, Node* expr) {
	void* data;
	int result = IDfind(id, &data);
	
	if (result == NUMBER) {
		yyerror("Can only index string or array. Got: NUMBER");
		result = -1;
	}
	else if (result == FUNCTION) {
		Function* f = (Function*) data;
		if (f->numParams > 0) {
			yyerrorf("Wrong number of parameters on function call: %i. Expected: 0",
				1, f->numParams);
		}
		if (!stringOrArray(f->retType)) {
			yyerrorf("Can only index string or array. Got: %s", 1, f->retType);
			result = -1;
		}
		idxNode->user = (void*) newChar(F_FUNC);
	}

	if (expr->info != NUMBER)
		yyerrorf("Can't access index of type %s. Expected: NUMBER", 1, yyname[expr->info]);

	idxNode->info = result == -1 ? NIL : NUMBER;
}

void handleID(Node* idNode, char* id) {
	void* data;
	int result = IDfind(id, &data);
	if (result == FUNCTION) {
		Function* f = (Function*) data;
		if (f->numParams > 0) {
			yyerrorf("Wrong number of parameters on function call: %i. Expected: 0",
				1, f->numParams);
		}
		idNode->info = f->retType;
		if (idNode->info == VOID)
			idNode->info = NIL;
		idNode->user = (void*) newChar(F_FUNC);
	}
	else if (result == -1) {
		idNode->info = NIL;
	}
	else {
		idNode->info = result;
		idNode->user = (void*) newChar(((Variable*) data)->flags);
	}
}

void handleUnOp(int type, Node* result, Node* n) {
	switch (type) {
		case '&':
			if (getFlags(n) & F_FUNC) {
				yyerror("Can't extract address of function");
				result->info = NIL;
			}
			else if (n->info == NUMBER)
				result->info = ARRAY;
			else {
				yyerrorf("Can only extract address of number. Got: %s",
					1, yyname[n->info]);
				result->info = NIL;
			}
			break;
		case '-': case '~':
			if (n->info == NUMBER)
				result->info = NUMBER;
			else {
				yyerrorf("%c: Operation only possible for numbers. Got: %s",
					2, (char) type, yyname[n->info]);
				result->info = NIL;
			}
			break;
	}
}

void handleBinOp(int type, Node* result, Node* l, Node* r) {
	switch (type) {
		case '^': case '*': case '/': case '%': case '&': case '|': case XOR:
			if (l->info == NUMBER && r->info == NUMBER)
				result->info = NUMBER;
			else {
				yyerrorf("%c: Operation only possible for numbers. Got: %s, %s",
					3, (char) type, yyname[l->info], yyname[r->info]);
				result->info = NIL;
			}
			break;
		case '-':
			if (l->info == ARRAY && r->info == ARRAY) {
				result->info = NUMBER;
				break;
			}
			/* Continue to '+' */
		case '+':
			if (l->info == NUMBER && r->info == NUMBER)
				result->info = NUMBER;
			else if (l->info == NUMBER && r->info == ARRAY
				  || l->info == ARRAY && r->info == NUMBER)
				result->info = ARRAY;
			else {
				type == '+' ? yyerrorf(
					"+: Operation only possible for two numbers or a number and an array. Got: %s, %s",
						2, yyname[l->info], yyname[r->info])
				  			: yyerrorf(
					"-: Each operator must be number or array. Got: %s, %s",
						2, yyname[l->info], yyname[r->info]);
				result->info = NIL;
			}
			break;
		case '>': case '<': case LEQ: case GEQ: case '=': case NEQ:
			if ((l->info == NUMBER || l->info == STRING) && l->info == r->info)
				result->info = NUMBER;
			else {
				yyerrorf("%s: Operation only possible for two numbers or two strings. Got: %s, %s",
					3, yyname[type], yyname[l->info], yyname[r->info]);
				result->info = NIL;
			}
			break;
		case ASSIGN:
			{
				char lFlags = getFlags(l);
				char rFlags = getFlags(r);
				if (lFlags & F_CONST)
					yyerror("Can't assign to const variable");
				else if (lFlags & F_FUNC)
					yyerror("Can't assign to function call");
				else if (stringOrArray(l->info) && r->info == NUMBER) {
					if (rFlags & F_INTLIT && rFlags & F_ZERO) { /* integer zero represents null pointer */
						result->info = l->info;
						return;
					}
				}
				if (l->info != r->info) {
					yyerrorf("Can't assign different types (%s and %s)",
						2, yyname[l->info], yyname[r->info]);
					result->info = NIL;
				}
				else 
					result->info = l->info;
			}
			break;
	}
}

void handleAlloc(Node* lvNode, Node* sizeNode) {
	if (sizeNode->info != NUMBER)
		yyerrorf("Allocation size must be a number. Got: %s",
			1, yyname[sizeNode->info]);
	if (!stringOrArray(lvNode->info))
		yyerrorf("Can only allocate memory for string or array. Got: %s",
			1, yyname[lvNode->info]);
}

char **yynames =
#if YYDEBUG > 0
		 (char**) yyname;
#else
		 0;
#endif
