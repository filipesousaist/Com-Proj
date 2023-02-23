%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "node.h" 
char *dupstr(const char *s);
int yyerror(char *s), octal(char *s), character(char *s);
%}
%x BODY AFTER
%%

^program                        BEGIN BODY;
^module                         BEGIN BODY;
.|\n                            ; /* Ignore everything before program or module */
<AFTER>.|\n                     ; /* Ignore everything after end */

<BODY>\$[^$]+\$                 ; /* Ignore comments */
<BODY>"$$".*\n                  ; /* Ignore comments */
^end                            BEGIN AFTER;

%%
int main() {
    while (yylex() != 0)
        ;
}


int yywrap(void) {
  return 1;
} 
char *getyytext() { return yytext; }
int character(char *s)
{
  int val, i, pos;
  for (val = pos = i = 0; s[pos] != '\'' && i < 4; i++, pos++) {
    if (s[pos] == '*')
      switch(s[++pos]) {
        case '*': case '(': case ')':  case '\'': case '"':  break;
	case 't': s[pos] = '\t'; break;
	case 'n': s[pos] = '\n'; break;
	case '0': s[pos] = '\0'; break;
	default: yyerror("invalid character escape sequence"); break;
      }
    val |= s[pos] << (i*8);
  }
  if (i == 4 && s[pos] != '\'') yyerror("character sequence too long");
  return val;
}

int octal(char *s)
{
  int i, a = 0, b = 0;

  for (i = 0; i < strlen(s); i++) {
    if (s[i] < '0' || s[i] > '9') break;
    b = b * 8 + s[i] - '0';
    if (b < a) {
      yyerror("octal overflow");
      break;
    }
    a = b;
  }
  return a;
}
