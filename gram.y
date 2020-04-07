%{
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "lib/node.h"
#include "lib/tabid.h"

int yylex();
void yyerror(char *s);
int lbl;


%}

%union {
	int i;			/* integer value */
	char *s;		/* symbol name or string literal */
	Node *n;		/* node pointer */
};

%token PROGRAM MODULE START END
%token VOID CONST NUMBER ARRAY STRING FUNCTION PUBLIC FORWARD
%token IF THEN ELSE ELIF FI FOR UNTIL STEP DO DONE REPEAT STOP RETURN
%token ASSIGN LEQ GEQ NEQ

%token<i> NUMLIT
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
%nonassoc UMINUS
%nonassoc '('

%%

file		: program 
			| module
			;

program		: PROGRAM decls START body END
			;

module		: MODULE decls END
			;

decls		: decls ';' decl
			| decl
			| /**/
			;

decl		: func
	  		| qual const var assign
			;

qual		: PUBLIC
	  		| FORWARD
			| /**/
			;

const		: CONST
	   		| /**/
			;

var			: type ID litidx
	  		;

type		: NUMBER
	  		| STRING
			| ARRAY
			;

litidx		: '[' NUMLIT ']'
			| /**/
	  		;

assign		: ASSIGN lits
	  		| ASSIGN array
			| /**/
			;

lits		: lits lit
	  		| lit
			;

lit			: NUMLIT
	  		| TXTLIT
			;

array		: array ',' NUMLIT
	  		| NUMLIT ',' NUMLIT
			;

func		: FUNCTION qual ret ID params impl
	  		;

ret			: type
	  		| VOID
			;

params		: params ';' var
	  		| var
			| /**/
			;

impl		: DONE
			| DO body
			;

body		: bodydecls block
			;

bodydecls	: bodydecls var ';'
			| /**/
			;

block		: instrs endinstr
			;

instrs		: instrs instr
			| /**/
			;

instr		: IF expr THEN block elifs else FI
			| FOR expr UNTIL expr STEP expr DO block DONE
			| expr ';'
			| expr '!'
			| lval '#' expr ';'
			;

endinstr	: REPEAT
			| STOP
			| RETURN
			| RETURN expr
			| /**/
			;

elifs		: ELIF expr THEN block
			| /**/
			;

else		: ELSE block
			| /**/
			;

lval		: ID idx
			;

idx			: '[' expr ']'
			| /**/
			;

expr		: lval
			| lits
			| '(' expr ')'
			| ID '(' args ')'
			| '?'
			| '&' lval
			| '-' expr %prec UMINUS
			| expr '^' expr
			| expr '*' expr
			| expr '/' expr
			| expr '%' expr
			| expr '+' expr
			| expr '-' expr
			| expr '<' expr
			| expr '>' expr
			| expr LEQ expr
			| expr GEQ expr
			| expr '=' expr
			| expr NEQ expr
			| '~' expr
			| expr '&' expr
			| expr '|' expr
			| expr ASSIGN expr
			;

args		: args ',' expr
			| expr
			;

%%

char **yynames =
#if YYDEBUG > 0
		 (char**)yyname;
#else
		 0;
#endif
