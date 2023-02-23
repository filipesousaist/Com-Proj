syntax region Comment start=/\%^/ end=/\zeprogram/ 
syntax region Comment start=/^end/ end=/\%$/ contains=PreProc

syntax keyword PreProc end contained
syntax keyword Type void array number string forward public
syntax keyword PreProc start program
syntax keyword Statement for until step do if then fi stop done repeat elif else return
syntax match Identifier /[?#!]/
syntax keyword Function function

syntax match Comment /\$[^$]*\$/
syntax match Comment /$$.*/

syntax match Constant /[0-9]\+/
syntax match Constant /\"[^"]*\"/
syntax match Constant /\'[^']\'/

let b:current_syntax = 'minor'
