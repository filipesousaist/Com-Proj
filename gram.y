%{
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "lib/node.h"
#include "lib/tabid.h"

int yylex();
void yyerror(char* s);
int lbl, errors;

/* Variable */

#define V_CONST     0b1
#define V_PUBLIC   0b10
#define V_FORWARD 0b100

typedef union {
	int n;      /* number */
	char* s;    /* string */
	Node* a;    /* array */
} Value;

typedef struct { 
	int type;
	Value value;
	char flags;
} Variable;

/* Number */

#define N_CHAR  0b1
#define N_ZERO 0b10

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
%type<n> endinstr elifs else lval call expr args
%%

file		: program							{ if (!errors) printNode($1, 0, (char**) yynames); }
			| module							{ if (!errors) printNode($1, 0, (char**) yynames); }
			;

program		: PROGRAM decls START 				{ IDpush(); }
			  body END 						    { $$ = binNode(PROGRAM, $2, $4); IDpop(); }
			| PROGRAM START						{ IDpush(); }
			  body END							{ $$ = binNode(PROGRAM, nilNode(NIL), $3); IDpop(); }
			| PROGRAM error START				{ IDpush(); }
			  body END							{ yyerrok; $$ = binNode(PROGRAM, nilNode(ERR), $4); IDpop(); }
			;		

module		: MODULE decls END                  { $$ = uniNode(MODULE, $2); }
			| MODULE END						{ $$ = uniNode(MODULE, nilNode(NIL)); }
			;

decls		: decls ';' decl					{ $$ = binNode(DECL, $1, $3); }
			| error ';' decl                    { yyerrok; $$ = binNode(DECL, nilNode(ERR), $3); }
			| decl                              { $$ = binNode(DECL, nilNode(NIL), $1); } 
			;

decl		: FUNCTION qual ret ID		 		{ IDnew(FUNCTION, $4, 0); IDpush(); }
			  params impl						{ $$ = pentNode(FUNCTION, $2, $3, 
													strNode(ID, $4), $5, $6); 
												  parseFunction($$, $2, $3, $4, $5); IDpop(); }

			| FUNCTION qual ret ID 				{ IDnew(FUNCTION, $4, 0); IDpush(); }
			  impl								{ $$ = pentNode(FUNCTION, $2, $3, 
													strNode(ID, $4), nilNode(NIL), $5);
												  parseFunction($$, $2, $3, $4, 0); IDpop(); }

			| FUNCTION error					{ IDpush(); }
			  impl								{ yyerrok; $$ = pentNode(FUNCTION, nilNode(ERR), 
													nilNode(ERR), nilNode(ERR), nilNode(ERR), $3);
												  IDpop(); }

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

params		: params ';' var					{ $$ = binNode(PARAM, $1, $3); }					
	  		| var								{ $$ = binNode(PARAM, nilNode(NIL), $1); }
			;

impl		: DONE								{ $$ = nilNode(DONE); }
			| DO body							{ $$ = uniNode(DO, $2); }
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

instr		: IF expr THEN block elifs else FI				{ $$ = quadNode(IF, $2, $4, $5, $6); }	
			| FOR expr UNTIL expr STEP expr DO block DONE	{ $$ = quadNode(FOR, $2, $4, $6, $8); }
			| expr ';'										{ $$ = uniNode(';', $1); }
			| expr '!'										{ $$ = uniNode('!', $1); }
			| lval '#' expr ';'     						{ $$ = binNode('#', $1, $3); }
			| IF error FI						{ yyerrok; $$ = quadNode(IF, nilNode(ERR),
													nilNode(ERR), nilNode(ERR), nilNode(ERR)); }
			| FOR error DONE					{ yyerrok; $$ = quadNode(FOR, nilNode(ERR), 
													nilNode(ERR), nilNode(ERR), nilNode(ERR)); }
			| expr error ';'					{ yyerrok; $$ = uniNode(';', nilNode(ERR)); }
			| expr error '!'					{ yyerrok; $$ = uniNode('!', nilNode(ERR)); }
			;

endinstr	: REPEAT							{ $$ = nilNode(REPEAT); }
			| STOP								{ $$ = nilNode(STOP); }
			| RETURN							{ $$ = uniNode(RETURN, nilNode(NIL)); }
			| RETURN expr						{ $$ = uniNode(RETURN, $2); }
			| /**/								{ $$ = nilNode(NIL);}
			;

elifs		: elifs ELIF expr THEN block	{ $$ = triNode(ELIF, $1, $3, $5); }
			| /**/							{ $$ = nilNode(NIL); }
			;

else		: ELSE block					{ $$ = uniNode(ELSE, $2); }
			| /**/							{ $$ = nilNode(NIL); }
			;

lval		: ID '[' expr ']'				{ $$ = binNode('[', strNode(ID, $1), $3); 
											  handleIndex($$, $1, $3); } 
			| ID							{ $$ = strNode(ID, $1); 
											  int result = IDfind($1, 0); 
											  $$->info = result == -1 ? NIL : result; }
			;

call		: ID '(' args ')'				{ IDfind($1, 0);
											  $$ = binNode('(', strNode(ID, $1), $3); }
			;

expr		: lval
			| lit							
			| lits							{ $$ = $1; $$->info = STRING; }
			| '(' expr ')'					{ $$ = $2; }
			| call
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
			| lits '[' expr ']'				{ $$ = binNode('[', $1, $3); }
			| call '[' expr ']'				{ $$ = binNode('[', $1, $3); }
			;

args		: args ',' expr					{ $$ = binNode(ARG, $1, $3); }
			| expr							{ $$ = binNode(ARG, nilNode(NIL), $1); }
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

char* flagInt(int value, int type) {
	char flags = 0;
	if (type == CHRLIT)	flags |= N_CHAR;
	if (value == 0)		flags |= N_ZERO;
	return newChar(flags);
}

void declareVar(Node* varNode, Node* typeNode, char* id, int size) {
	Variable* var = (Variable*) malloc(sizeof(Variable));
	int type = typeNode->info;
	IDnew(type, id, var);
	
	var->type = typeNode->info;
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

	Value* val = (Value*) malloc(sizeof(Value));	
	switch (type) {
		case NUMBER:
			val->n = valueNode->value.i; break;
		case STRING:
			val->s = strdup(valueNode->value.s); break;
		case ARRAY:
			val->a = valueNode; break;
	}
	
	initNode->user = (void*) val;
}

void createVar(Node* qualNode, Node* constNode, Node* varNode, Node* initNode) {
	Variable* var = (Variable*) varNode->user;
	Value value = *((Value*) initNode->user);
	var->value = value;
	
	/* Convert single number into array if needed */
	if (initNode->info == NUMBER && var->type == ARRAY) {
		Node* intNode = initNode->CHILD(0);
		Node* arrayNode = binNode(',', nilNode(NIL), intNode);
		arrayNode->info = 1;
		
		value.a = initNode->CHILD(0) = arrayNode;
		initNode->info = ARRAY;
	}
	printf("InitNode Info %i\n", initNode->info);
	printf("Var Type %i\n", var->type);

	if (initNode->info != NIL && var->type != initNode->info)
		yyerror("Cannot assign different types");
	else if (initNode->info == ARRAY) { /* */
		if (varNode->info == -1) 
			yyerror("Cannot initialize array without declaring size");
		else if (varNode->info < value.a->info) 
			yyerror("Array initializer has more elements than declared size");
	}

	if (initNode->info == NIL
     && constNode->info == CONST
	 && qualNode->info != FORWARD)
        yyerror("Constant non-forward variable must be initialized");
	
	/* Set flags */
	if		 (constNode->info == CONST)		var->flags |= V_CONST;
	if		 (qualNode->info == PUBLIC)		var->flags |= V_PUBLIC;
	else if	 (qualNode->info == FORWARD)	var->flags |= V_FORWARD;
}

void parseFunction(Node* funcNode, Node* qualNode, Node* retNode, char* id, Node* paramsNode) {
}

void handleIndex(Node* idxNode, char* id, Node* expr) {
	int result = IDfind(id, 0);

	if (result == NUMBER)
		yyerror("Can only index string or array");

	if (expr->info != NUMBER)
		yyerror("Can't access non-number index");

	idxNode->info = result == -1 ? NIL : result;
}

void handleUnOp(int type, Node* result, Node* n) {
	switch (type) {
		case '&':
			if (n->info == NUMBER)
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
						  	: yyerror("Each operator must be number or array");
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
			if ((l->info == STRING || l->info == ARRAY)
			 && r->info == NUMBER && r->user) { /* assign number literal to string or array */
				char flags = *((char*) r->user);
				if (!(flags & N_CHAR) && flags & N_ZERO) { /* integer zero represents null pointer */
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
			break;
	}
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

char **yynames =
#if YYDEBUG > 0
		 (char**) yyname;
#else
		 0;
#endif
