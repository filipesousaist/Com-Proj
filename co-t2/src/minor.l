%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include "node.h"
#include "y.tab.h"

char *dupstr(const char *s);
int yyerror(char *s), octal(char *s), character(char *s);
int parse_number(char *str, int base);
%}
LET [A-Za-z]
DEC [0-9]
OCT [0-7]
HEX [0-9A-Fa-f]
%x BEFORE AFTER TEXT CHAR
%%
 /* Before and after the code zone */
<BEFORE>^program                            BEGIN INITIAL; return PROGRAM;
<BEFORE>^module                             BEGIN INITIAL; return MODULE;
<BEFORE,AFTER>.|\n                          ; /* Ignore everything before 'program' */
                                              /* or 'module' and after 'end'        */
 /* Comments */
\$[^$]+\$                                   ; /* Ignore comments */
"$$".*\n                                    ; /* Ignore comments */

 /* Whitespace */
[ \n\t\r]+                                  ; /* Ignore whitespace */

 /* Keywords */
start                                       return START;
void                                        return VOID;
const	                                    return CONST;
number                                      return NUMBER;
array                                       return ARRAY;
string                                      return STRING;
function                                    return FUNCTION;
public                                      return PUBLIC;
forward                                     return FORWARD;
if                                          return IF;
then                                        return THEN;
else                                        return ELSE;
elif                                        return ELIF;
fi                                          return FI;
for                                         return FOR;
until                                       return UNTIL;
step                                        return STEP;
do                                          return DO;
done                                        return DONE;
repeat                                      return REPEAT;
stop                                        return STOP;
return                                      return RETURN;
or                                          return BOR;
assert                                      return ASSERT;
^end                                        BEGIN AFTER; return END;
^program|module                             yyerror("program and module can only appear once in the start of the line");

 /* Identifiers */
{LET}({LET}|{DEC}|_)*                       yylval.s = dupstr(yytext); return ID;

 /* Numbers */
0                                           yylval.i = 0; return NUMLIT;
0b[01]+                                     yylval.i = parse_number(yytext + 2, 2); return NUMLIT;
0{OCT}+                                     yylval.i = parse_number(yytext + 1, 8); return NUMLIT;
[1-9]{DEC}*                                 yylval.i = parse_number(yytext, 10); return NUMLIT;
0x{HEX}+                                    yylval.i = parse_number(yytext + 2, 16); return NUMLIT;

 /* Text sequences */
\"([^\\\"]|\\(.|n))*\"                      yylval.s = malloc(yyleng - 1); *yylval.s = 0; yyless(1); BEGIN TEXT;
<TEXT>[^\\\"]+                              strcat(yylval.s, yytext);
<TEXT>\\n                                   strcat(yylval.s, "\n");
<TEXT>\\r                                   strcat(yylval.s, "\r");
<TEXT>\\t                                   strcat(yylval.s, "\t");
<TEXT>\\\\                                  strcat(yylval.s, "\\");
<TEXT>\\\"                                  strcat(yylval.s, "\"");
<TEXT>\\{HEX}{HEX}?                         { char len = strlen(yylval.s); 
                                              yylval.s[len] = (char) strtol(yytext + 1, 0, 16);
                                              yylval.s[len + 1] = 0; }
<TEXT>\"                                    BEGIN INITIAL; return TXTLIT;

 /* Characters */
\'([^\\\']|\\([nrt\'\\]|{HEX}{HEX}?))\'     yyless(1); BEGIN CHAR;
<CHAR>[^\\\']                               yylval.i = *yytext;
<CHAR>\\n                                   yylval.i = '\n';
<CHAR>\\r                                   yylval.i = '\r';
<CHAR>\\t                                   yylval.i = '\t';
<CHAR>\\\\                                  yylval.i = '\\';
<CHAR>\\\'                                  yylval.i = '\'';
<CHAR>\\{HEX}{HEX}?                         yylval.i = (char) strtol(yytext + 1, 0, 16);
<CHAR>\'                                    BEGIN INITIAL; return CHRLIT;

<TEXT,CHAR>.|\n                             yyerror("unexpected character");
 
 /* Operators */
":="                                        return ASSIGN;
"<="                                        return LEQ;
">="                                        return GEQ;
"~="                                        return NEQ;
[-+*/%^<>=|&~?]                             return *yytext;

 /* Separators and delimiters */
[#\[\]();!,]                                return *yytext;

.|\n                                        yyerror("unexpected symbol");
%%
void yystart(void) { BEGIN BEFORE; }

int yywrap(void) {
    switch (YY_START) {
        case BEFORE:
            yyerror("program or module not found"); break;
        case INITIAL:
            yyerror("missing end of program or module"); break;
        case TEXT:
            yyerror("unfinished text sequence"); break;
        case CHAR:
            yyerror("unfinished character"); break;
    }
    return 1; 
} 
char *getyytext() { return yytext; }

int parse_number(char* str, int base) {
    errno = 0;
    long result = strtol(str, 0, base);
    if (errno != 0 || result > INT_MAX)
        yyerror("Number out of range");
    return (int) result;
}
