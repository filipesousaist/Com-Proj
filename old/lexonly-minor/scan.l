%{
#include <stdlib.h>
#include <limits.h>
void yyerror(char*);
int parse_number(char*, int);
%}
LET [A-Za-z]
DEC [0-9]
OCT [0-7]
HEX [0-9A-Fa-f]
%x BEFORE AFTER TEXT CHAR
%%

 /* Before and after the code zone */
<BEFORE>^program                            BEGIN INITIAL;
<BEFORE>^module                             BEGIN INITIAL;
<BEFORE,AFTER>.|\n                          ; /* Ignore everything before 'program' */
                                              /* or 'module' and after 'end'        */
 /* Comments */
\$[^$]+\$                                   ; /* Ignore comments */
"$$".*\n                                    ; /* Ignore comments */

 /* Whitespace */
[ \n\t\r]+                                  ; /* Ignore whitespace */

 /* Keywords */
start                                       ECHO;
void                                        ECHO;
const	                                    ECHO;
number                                      ECHO;
array                                       ECHO;
string                                      ECHO;
function                                    ECHO;
public                                      ECHO;
forward                                     ECHO;
if                                          ECHO;
then                                        ECHO;
else                                        ECHO;
elif                                        ECHO;
fi                                          ECHO;
for                                         ECHO;
until                                       ECHO;
step                                        ECHO;
do                                          ECHO;
done                                        ECHO;
repeat                                      ECHO;
stop                                        ECHO;
return                                      ECHO;
^end                                        ECHO; BEGIN AFTER;
^program|module                             yyerror("program and module can only appear once in the start of the line");

 /* Identifiers */
{LET}({LET}|{DEC}|_)*                       ECHO;

 /* Numbers */
0                                           ECHO;
0b[01]+                                     parse_number(yytext + 2, 2);
0{OCT}+                                     parse_number(yytext + 1, 8);
[1-9]{DEC}*                                 parse_number(yytext, 10);
0x{HEX}+                                    parse_number(yytext + 2, 16);

 /* Text sequences */
\"([^\\\"]|\\(.|n))*\"                      printf("\""); yyless(1); BEGIN TEXT;
<TEXT>[^\\\"]+                              ECHO;
<TEXT>\\\"                                  printf("\"");
<TEXT>\"                                    ECHO; BEGIN INITIAL;

 /* Characters */
\'([^\\\']|\\([nrt\'\\]|{HEX}{HEX}?))\'     printf("\'"); yyless(1); BEGIN CHAR;
<CHAR>[^\\\']                               ECHO;
<CHAR>\\\'                                  printf("\'"); 
<CHAR>\'                                    ECHO; BEGIN INITIAL;

<TEXT,CHAR>\\n                              printf("\n");
<TEXT,CHAR>\\r                              printf("\r");
<TEXT,CHAR>\\t                              printf("\t");
<TEXT,CHAR>\\\\                             printf("\\");
<TEXT,CHAR>\\{HEX}{HEX}?                    printf("%c", (int) strtol(yytext + 1, 0, 16));
<TEXT,CHAR>.|\n                             yyerror("unexpected character");
 
 
 /* Operators */
":="                                        ECHO;
"<="                                        ECHO;
">="                                        ECHO;
"~="                                        ECHO;
[-+*/%^<>=|&~?]                             ECHO;

 /* Separators and delimiters */
[#\[\]();!,]                                ECHO;

.|\n                                        yyerror("unexpected symbol");

%%

int parse_number(char* str, int base) {
    long result = strtol(str, 0, base);
    if (errno != 0 || result > INT_MAX)
        yyerror("");
    else
        printf("%ld", result);
    return (int) result;
}

void yyerror(char* s) {
    fprintf(stderr, "Error: %s\n", s);
    exit(-1);
}

int yywrap(void) {
    if (YY_START != AFTER)
        yyerror("Missing end block");
    return 1;
}

int main(void) {
    BEGIN BEFORE;
    while (yylex());
}
