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

%%
start : 'x'

%%


char **yynames =
#if YYDEBUG > 0
		 (char**)yyname;
#else
		 0;
#endif
