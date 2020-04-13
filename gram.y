%{
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "lib/node.h"
#include "lib/tabid.h"

int yylex();
void yyerror(char* s);
int lbl;

%}

%union {
	int i;			/* integer value */
	char* s;		/* symbol name or string literal */
	Node* n;		/* node pointer */
};

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

file		: program							{ printNode($1, 0, (char**) yynames); }
			| module							{ printNode($1, 0, (char**) yynames); }
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

	  		| qual const var init				{ $$ = quadNode(VAR, $1, $2, $3, $4); }
			;

qual		: PUBLIC							{ $$ = nilNode(PUBLIC); }
	  		| FORWARD							{ $$ = nilNode(FORWARD); }
			| /**/								{ $$ = nilNode(NIL); }
			;

const		: CONST								{ $$ = nilNode(CONST); }
	   		| /**/								{ $$ = nilNode(NIL); }
			;

var			: type ID '[' NUMLIT ']'			{ $$ = triNode(VAR, $1, strNode(ID, $2),
													intNode(NUMLIT, $4)); IDnew($1->info, $2, 0); }
			| type ID							{ $$ = triNode(VAR, $1, strNode(ID, $2),
													nilNode(NIL)); IDnew($1->info, $2, 0); }
	  		;

type		: NUMBER							{ $$ = nilNode(NUMBER); $$->info = NUMBER; }
	  		| STRING							{ $$ = nilNode(STRING); $$->info = STRING; }
			| ARRAY								{ $$ = nilNode(ARRAY); $$->info = ARRAY; }
			;

init		: ASSIGN lit                        { $$ = uniNode(INIT, $2); $$->info = NUMBER; }
			| ASSIGN lits						{ $$ = uniNode(INIT, $2); $$->info = STRING; }
	  		| ASSIGN array						{ $$ = uniNode(INIT, $2); $$->info = ARRAY; }
			| /**/								{ $$ = nilNode(NIL); }
			;

lits		: lits lit							{ $$ = strNode(TXTLIT, getJoined($1->value.s, toStr($2))); }
	  		| lit lit							{ $$ = strNode(TXTLIT, getJoined(toStr($1), toStr($2))); }
			;

lit			: NUMLIT							{ $$ = intNode(NUMLIT, $1); $$->info = NUMBER; }
			| CHRLIT							{ $$ = intNode(CHRLIT, $1); $$->info = NUMBER; }
	  		| TXTLIT							{ $$ = strNode(TXTLIT, $1); $$->info = STRING; }
			;

array		: array ',' NUMLIT					{ $$ = binNode(',', $1, intNode(NUMLIT, $3)); }
	  		| NUMLIT ',' NUMLIT					{ $$ = binNode(',', intNode(NUMLIT, $1), 
													intNode(NUMLIT, $3)); }
			;

ret			: type
	  		| VOID								{ $$ = nilNode(VOID); }
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

lval		: ID '[' expr ']'				{ if (IDfind($1, 0) == -1) yyerror("Variable not declared"); 
											  $$ = binNode('[', strNode(ID, $1), $3); } 
			| ID							{ if (IDfind($1, 0) == -1) yyerror("Variable not declared");
											  $$ = strNode(ID, $1); }
			;

call		: ID '(' args ')'				{ if (IDfind($1, 0) == -1) yyerror("Variable not declared");
											  $$ = binNode('(', strNode(ID, $1), $3); }
			;

expr		: lval
			| lit							
			| lits
			| '(' expr ')'					{ $$ = $2; }
			| call
			| '?'							{ $$ = nilNode('?'); }
			| '&' lval %prec ADDR			{ $$ = uniNode(ADDR, $2); }
			| '-' expr %prec UMINUS			{ $$ = uniNode(UMINUS, $2); }
			| expr '^' expr					{ $$ = binNode('^', $1, $3); }
			| expr '*' expr					{ $$ = binNode('*', $1, $3); }
			| expr '/' expr					{ $$ = binNode('/', $1, $3); }
			| expr '%' expr                 { $$ = binNode('%', $1, $3); }
			| expr '+' expr                 { $$ = binNode('+', $1, $3); }
			| expr '-' expr                 { $$ = binNode('-', $1, $3); }
			| expr '<' expr                 { $$ = binNode('<', $1, $3); }
			| expr '>' expr                 { $$ = binNode('>', $1, $3); }
			| expr LEQ expr                 { $$ = binNode(LEQ, $1, $3); }
			| expr GEQ expr                 { $$ = binNode(GEQ, $1, $3); }
			| expr '=' expr                 { $$ = binNode('=', $1, $3); }
			| expr NEQ expr                 { $$ = binNode(NEQ, $1, $3); }
			| '~' expr						{ $$ = uniNode('~', $2); }
			| expr '&' expr                 { $$ = binNode('&', $1, $3); }
			| expr '|' expr                 { $$ = binNode('^', $1, $3); }
			| expr ASSIGN expr				{ $$ = binNode(ASSIGN, $1, $3); }
			| lits '[' expr ']'				{ $$ = binNode('[', $1, $3); }
			| call '[' expr ']'				{ $$ = binNode('[', $1, $3); }
			;

args		: args ',' expr					{ $$ = binNode(ARG, $1, $3); }
			| expr							{ $$ = binNode(ARG, nilNode(NIL), $1); }
			;

%%

void parseFunction(Node* funcNode, Node* qualNode, Node* retNode, char* id, Node* paramsNode) {
}

char* getJoined(char* s1, char* s2) {
	char* result = (char*) malloc(strlen(s1) + strlen(s2) + 1);
	*result = 0;
	strcat(result, s1);
	strcat(result, s2);
	return result;
}

char* toStr(Node* n) {
	if (n->info == NUMBER) {
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
