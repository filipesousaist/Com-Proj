%{
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "lib/node.h"
#include "lib/tabid.h"

int yylex();
void yyerror(char* s);
int errors;

/* Flags */
#define F_CONST   0b000001
#define F_PUBLIC  0b000010
#define F_FORWARD 0b000100
#define F_FUNC    0b001000 
#define F_INTLIT  0b010000
#define F_ZERO    0b100000

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

static long ifDepth = 0;
static long forDepth = 0;
static char* fName; /* 0 -> Outside function, "" -> main function */

%}

%union {
	int i;			/* integer value */
	char* s;		/* symbol name or string literal */
	Node* n;		/* node pointer */
}

%token PROGRAM MODULE START END
%token VOID CONST NUMBER ARRAY STRING FUNCTION PUBLIC FORWARD
%token IF THEN ELSE ELIF FI FOR UNTIL STEP DO DONE REPEAT STOP RETURN
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
%left '*' '/' '%'
%right '^'
%nonassoc UMINUS ADDR
%nonassoc '(' '['

/* Tokens used on syntax tree only */
%token NIL ERR DECL SIZE INIT PARAM
%token ARG INSTR BLOCK BODY VARS VAR

%type<n> program module decls decl qual const var type init lits lit
%type<n> array ret params impl body vars block instrs instr
%type<n> endinstr elifs else lval expr args
%%

file		: program							{ if (!errors) printNode($1, 0, (char**) yynames); }
			| module							{ if (!errors) printNode($1, 0, (char**) yynames); }
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

decls		: decls ';' decl					{ $$ = binNode(DECL, $1, $3); }
			| error ';' decl                    { yyerrok; $$ = binNode(DECL, nilNode(ERR), $3); }
			| decl                              { $$ = binNode(DECL, nilNode(NIL), $1); } 
			;

decl		: FUNCTION qual ret ID		 		{ IDpush(); fName = $4; }
			  params 							{ createFunction($2, $3, $4, $6); }
			  impl								{ $$ = pentNode(FUNCTION, $2, $3, 
													strNode(ID, $4), $6, $8);
												  addFuncImpl($8); IDpop(); }

			| FUNCTION qual ret ID 				{ IDpush(); fName = $4; createFunction($2, $3, $4, 0); }
			  impl								{ $$ = pentNode(FUNCTION, $2, $3,
													strNode(ID, $4), nilNode(NIL), $6); 
												  addFuncImpl($6); IDpop(); }

	  		| qual const var init				{ $$ = quadNode(VAR, $1, $2, $3, $4); createVar($1, $2, $3, $4); }
			;

qual		: PUBLIC							{ $$ = nilNode(PUBLIC); $$->info = PUBLIC; }
	  		| FORWARD							{ $$ = nilNode(FORWARD); $$->info = FORWARD; }
			| /**/								{ $$ = nilNode(NIL); $$->info = NIL; }
			;

const		: CONST								{ $$ = nilNode(CONST); $$->info = CONST; }
	   		| /**/								{ $$ = nilNode(NIL); $$->info = NIL; }
			;

var			: type ID '[' NUMLIT ']'			{ $$ = triNode(VAR, $1, strNode(ID, $2),
													intNode(NUMLIT, $4)); 
												  declareVar($$, $1, $2, $4); }
			| type ID							{ $$ = triNode(VAR, $1, strNode(ID, $2),
													nilNode(NIL)); 
												  declareVar($$, $1, $2, -1); }
	  		;

type		: NUMBER							{ $$ = nilNode(NUMBER); $$->info = NUMBER; }
	  		| STRING							{ $$ = nilNode(STRING); $$->info = STRING; }
			| ARRAY								{ $$ = nilNode(ARRAY); $$->info = ARRAY; }
			;

init		: ASSIGN lit                        { $$ = uniNode(INIT, $2); 
												  initValue($$, $2->info, $2); }
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

instr		: IF										{ ifDepth ++; }
			  expr THEN block elifs else FI				{ $$ = quadNode(IF, $3, $5, $6, $7); 
														  ifDepth --; if ($3->info == NIL) 
															  yyerror("Invalid type for 'if' test expression"); }	
			| FOR										{ forDepth ++; }
			  expr UNTIL expr STEP expr DO block DONE	{ $$ = quadNode(FOR, $3, $5, $7, $9); 
														  forDepth --; if ($5->info == NIL)
															  yyerror("Invalid type for 'until' test expression"); }
			| expr ';'									{ $$ = uniNode(';', $1); }
			| expr '!'									{ $$ = uniNode('!', $1); if ($1->info == NIL) 
															  yyerror("Can only print number, string or array"); }
			| lval '#' expr ';'     					{ $$ = binNode('#', $1, $3); handleAlloc($1, $3); }
			| IF error FI						{ yyerrok; $$ = quadNode(IF, nilNode(ERR),
													nilNode(ERR), nilNode(ERR), nilNode(ERR)); }
			| FOR error DONE					{ yyerrok; $$ = quadNode(FOR, nilNode(ERR), 
													nilNode(ERR), nilNode(ERR), nilNode(ERR)); }
			| expr error ';'					{ yyerrok; $$ = uniNode(';', nilNode(ERR)); }
			| expr error '!'					{ yyerrok; $$ = uniNode('!', nilNode(ERR)); }
			;

endinstr	: REPEAT							{ $$ = nilNode(REPEAT); 
												  if (forDepth <= 0) yyerror("Can't repeat outside loop"); }
			| STOP								{ $$ = nilNode(STOP); 
												  if (forDepth <= 0) yyerror("Can't stop outside loop"); }
			| RETURN							{ $$ = uniNode(RETURN, nilNode(NIL)); handleReturn(VOID); }
			| RETURN expr						{ $$ = uniNode(RETURN, $2); handleReturn($2->info); }
			| /**/								{ $$ = nilNode(NIL);}
			;

elifs		: elifs ELIF expr THEN block	{ $$ = triNode(ELIF, $1, $3, $5); if ($3->info == NIL)
												  yyerror("Invalid type for 'elif' test expression"); }
			| /**/							{ $$ = nilNode(NIL); }
			;

else		: ELSE block					{ $$ = uniNode(ELSE, $2); }
			| /**/							{ $$ = nilNode(NIL); }
			;

lval		: ID '[' expr ']'				{ $$ = binNode('[', strNode(ID, $1), $3); 
											  handleIndex($$, $1, $3); } 
			| ID							{ $$ = strNode(ID, $1); 
											  handleID($$, $1); }
			;

args		: args ',' expr					{ $$ = binNode(ARG, $1, $3); addArg($$, $1, $3); }
			| expr							{ $$ = binNode(ARG, nilNode(NIL), $1); initArgs($$, $1); }
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
			| '~' expr						{ $$ = uniNode('~', $2); handleUnOp('~', $$, $2); }
			| expr '&' expr                 { $$ = binNode('&', $1, $3); handleBinOp('&', $$, $1, $3); }
			| expr '|' expr                 { $$ = binNode('^', $1, $3); handleBinOp('|', $$, $1, $3); }
			| lval ASSIGN expr				{ $$ = binNode(ASSIGN, $1, $3); handleBinOp(ASSIGN, $$, $1, $3); }
			;
%%

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
			yyerror("Can't declare size for non-array variable");
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

void createVar(Node* qualNode, Node* constNode, Node* varNode, Node* initNode) {
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
				yyerror("Can't override non-forward variable");
			else if (qualNode->info == FORWARD)
				yyerror("Can't re-declare forward variable");
			else /* Non-forward overriding forward: OK */ {
				oldVar->flags &= ~F_FORWARD;
			}
			
			if (!(oldVar->flags & F_CONST) == (constNode->info == CONST))
				yyerror("Forward and non-forward variable declarations differ on 'const' qualifier");
		}
		else
			yyerror("Already declared but with different type");
	}

	/* Convert single number into array if needed */
	if (initNode->info == NUMBER && var->type == ARRAY) {
		Node* arrayNode = binNode(',', nilNode(NIL), (Node*) initNode->user);
		arrayNode->info = 1;
		
		removeNode(initNode, 0);
		addNode(initNode, arrayNode, 0);
		initNode->user = arrayNode;
		initNode->info = ARRAY;
	}

	if (initNode->info != NIL && var->type != initNode->info)
		yyerror("Invalid initializer type");
	else if (initNode->info == ARRAY) {
		Node* arrayNode = (Node*) initNode->user;
		if (varNode->info == -1) 
			yyerror("Cannot initialize array without declaring size");
		else if (varNode->info < arrayNode->info) 
			yyerror("Array initializer has more elements than declared size");
	}

	if (initNode->info == NIL
     && constNode->info == CONST
	 && qualNode->info != FORWARD)
        yyerror("Constant non-forward variable must be initialized");
	else if (initNode->info != NIL && qualNode->info == FORWARD)
		yyerror("Forward variable can't be initialized");
	
	if (repeated) {
		free(var);	
	}
	else {
		/* Set flags */
		if		 (constNode->info == CONST)		var->flags |= F_CONST;
		if		 (qualNode->info == PUBLIC)		var->flags |= F_PUBLIC;
		else if	 (qualNode->info == FORWARD)	var->flags |= F_FORWARD;
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

Node* createFunction(Node* qualNode, Node* retNode, char* id, Node* paramsNode) {
	Function* f = (Function*) malloc(sizeof(Function));
	f->flags = 0;
	if		(qualNode->info == PUBLIC)	f->flags |= F_PUBLIC;
	else if (qualNode->info == FORWARD) f->flags |= F_FORWARD;
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
                yyerror("Can't override non-forward function");
            else if (qualNode->info == FORWARD)
                yyerror("Can't re-declare forward function");
            else /* Non-forward overriding forward: OK */ {
                oldF->flags &= ~F_FORWARD;
            }

			if (f->retType != oldF->retType)
				yyerror("Function return type does not match with previous declaration");
			if (f->numParams != oldF->numParams)
				yyerror("Number of function parameters doesn't match with previous declaration");
			else for (int i = 0; i < f->numParams; i ++)
				if (f->paramTypes[i] != oldF->paramTypes[i])
					yyerror("Function parameter type does not match with previous declaration");
        }
        else
            yyerror("Already declared but with different type");
		
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
		yyerror("Non-forward function must have a body");
	else if (f->flags & F_FORWARD && implNode->info == DO)
		yyerror("Forward function can't have a body");
	fName = 0;
}

void handleCall(Node* callNode, char* id, Node* argsNode) {
	void* data;
	int result = IDsearch(id, &data, 1, 0);

	if (result == FUNCTION) {
		Function* f = (Function*) data;
		
		if (f->numParams != argsNode->info)
			yyerror("Wrong number of arguments");
		
		else {
			int* argTypes = (int*) argsNode->user;
			char correctArgs = 1;
			for (int i = 0; i < f->numParams; i ++)
				if (f->paramTypes[i] != argTypes[i]) {
					correctArgs = 0;
					yyerror("Wrong argument type");
				}
			if (correctArgs) {
				callNode->info = f->retType;
				return;
			}
		}
	}
	else if (result != -1)
		yyerror("Can only call function");
	
	callNode->info = NIL;
}

void initArgs(Node* argsNode, Node* exprNode) {
	int* argTypes = (int*) malloc(sizeof(int));
	*argTypes = exprNode->info;
	
	argsNode->user = (void*) argTypes;
	argsNode->info = 1; /* Number of arguments */
}

void addArg(Node* newArgsNode, Node* oldArgsNode, Node* exprNode) {	
	int newSize = oldArgsNode->info + 1;
	int* argTypes = (int*) realloc(oldArgsNode->user, newSize * sizeof(int));
	argTypes[newSize - 1] = exprNode->info;
	
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
		if (!ifDepth && !forDepth)
			yyerror("Return outside block in main function");
		fType = NUMBER;
	}
	
	if (fType != type)
		yyerror("Wrong function return type");
}

void handleIndex(Node* idxNode, char* id, Node* expr) {
	void* data;
	int result = IDfind(id, &data);
	
	if (result == NUMBER) {
		yyerror("Can only index string or array");
		result = -1;
	}
	else if (result == FUNCTION) {
		Function* f = (Function*) data;
		if (f->numParams > 0) {
			yyerror("Wrong number of parameters on function call");
			result = -1;
		}
		if (!stringOrArray(f->retType)) {
			yyerror("Can only index string or array");
			result = -1;
		}
		idxNode->user = (void*) newChar(F_FUNC);
	}


	if (expr->info != NUMBER)
		yyerror("Can't access non-number index");

	idxNode->info = result == -1 ? NIL : NUMBER;
}

void handleID(Node* idNode, char* id) {
	void* data;
	int result = IDfind(id, &data);
	if (result == FUNCTION) {
		idNode->info = ((Function*) data)->retType;
		if (idNode->info == VOID)
			idNode->info = NIL;
		idNode->user = (void*) newChar(F_FUNC);
	}
	else if (result == -1)
		idNode->info = NIL;
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
				yyerror("Can only extract address of number");
				result->info = NIL;
			}
			break;
		case '-': case '~':
			if (n->info == NUMBER)
				result->info = NUMBER;
			else {
				yyerror("Operation only possible for numbers");
				result->info = NIL;
			}
			break;
	}
}

void handleBinOp(int type, Node* result, Node* l, Node* r) {
	switch (type) {
		case '^': case '*': case '/': case '%': case '&': case '|':
			if (l->info == NUMBER && r->info == NUMBER)
				result->info = NUMBER;
			else {
				yyerror("Operation only possible for numbers");
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
				type == '+' ? yyerror("Operation only possible for two numbers or a number and an array")
				  /* '-' */	: yyerror("Each operator must be number or array");
				result->info = NIL;
			}
			break;
		case '>': case '<': case LEQ: case GEQ: case '=': case NEQ:
			if ((l->info == NUMBER || l->info == STRING) && l->info == r->info)
				result->info = NUMBER;
			else {
				yyerror("Operation only possible for two numbers or two strings");
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
					yyerror("Can't assign different types");
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
		yyerror("Allocation size must be a number");
	if (!stringOrArray(lvNode->info))
		yyerror("Can only allocate memory for string or array");
}

char **yynames =
#if YYDEBUG > 0
		 (char**) yyname;
#else
		 0;
#endif
