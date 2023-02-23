
## **Project stages description**
* LEX - Should pass (i.e. reject) these tests after lex is done
* GRAM - Should pass (i.e. reject) these tests after grammar is done
* SYMB - Should pass (i.e. reject) these tests after symbol table is done
* SEM - Should pass (i.e. reject) these tests after semantic verifications (types, number of arguments, etc) are done

Note: What each student implements on each phsase can vary.

## **Brief description of each fail test**

Project stage | File | Description                                                   
|--------------|------|------------                                                                 
| SEM | 2atrib0.min | Tests chained atribution with different types
| LEX | 2char.min | Two characters inside a single char ('ab')
| GRAM | ack.min | Ackermann function, but function 'argc' has no return type
| GRAM | addr.min | Tests address of a number literal (&3)
| SEM | addr2.min | Tests address of a string variable
| GRAM | arraychar.min | A char in array initialization
| SEM | arraydecl.min | Initialize array with 1 element, without defining its size
| GRAM | arrayinit.min | Initialize array in the main function
| GRAM | arrayinit2.min | Array initializer used in an expression
| LEX | bignumber.min | 2^31 + 1 in decimal
| LEX | char_slash.min | Tests '\\'
| GRAM | chardim.min | Declare array with char as dimension (array x['\\2'])
| SEM | const.min | Tries to assign value to const variable, after declaration
| SEM | const_alloc.min | Tries to allocate memory to const variable
| SEM | di.min | Tries to index a number x (x[0])
| SEM | dim.min | Declares a string with a dimension
| GRAM | elif_no_if.min | 'elif' without a previous 'if'
| SEM | elifvoid.min | 'elif' condition is a void function call
| GRAM | else_no_if.min | 'else' without a previous 'if'
| LEX | empty.min | Empty file
| LEX | end_unaligned.min | 'end' is not in the start of the line
| LEX | end.min | File with only the keyword 'end'
| LEX | endquote1.min | Tests '\\"'
| SEM | failtypes.min | Calls function with one argument of the right type, and other of the wrong type
| SEM | fnatr.min | Trying to assign a value to a function call 
| SEM | fnindex.min | Trying to index a function
| SEM | forward_fail.min | Function forward declaration arguments' types don't match with the ones in the implementation
| SEM | forwardbody.min | A number that is declared as 'forward', but is initialized
| SEM | forwardbody2.min | A function that is declared as 'forward', but is implemented
| SEM | init_arr_no_size.min | Array initialized without declaring its size
| SEM | init0str.min | Initlializing string with value 0 (null pointer) (Note: this is valid in assignment, not initialization)
| SEM | invalid_args.min | Passes string arguments to function with number parameters
| SEM | invalid_args2.min | Few argumtents passed to function call
| SEM | invalid_repeat.min | 'repeat' outside a loop
| SEM | invalid_return.min | Return string literal in the main function
| SEM | invalid_stop.min | 'stop' outside a loop
| SEM | invalid_types1.min | Assign string to number
| SEM | invalid_types3.min | Initialize string with a number value
| SEM | invalid_types4.min | 'void' function that returns a number
| SEM | invalid_types6.min | Assign string to a position in an array
| SEM | invalid_types7.min | A function that should return 'string', but returns a number
| SEM | invaliddecl1.min | Function return value in forward declaration is different from return value in implementation
| SEM | invaliddim.min | Array initialized with size 0
| GRAM | invaliddim2.min | Array initialized with negative size
| GRAM | invaliddim3.min | Array initialized with an expresssion
| SEM | invalidfn.min | Trying to call a number x (x(10))
| SEM | invalidindex.min | Trying to index an array using a string literal (x["123"])
| SEM | invalidindex2.min | Trying to index an array using a string variable with value 0
| GRAM | lval_paren.min | Trying to assign to variable between parentheses
| SEM | missingforward.min | Function declared and not implemented, but not 'forward'
| LEX | module_unaligned.min | 'module' is not in the start of the line
| GRAM | multidim.min | Tests a 2D array (there are no multidimensional arrays in minor)
| GRAM | multiindex.min | Tests 2D array indexing (there are no multidimensional arrays in minor). Note: It should also fail because array declaration has no size
| LEX | no_end.min | 'program' with no 'end'
| GRAM | no_start.min | 'program' with no 'start'
| LEX | non_special_char.min | Tries to escape a non-special character ('\\v')
| SEM | pointer1.min | Tries to store pointer to allocated memory in a 'number'
| SEM | printvoid.min | Tries to print a void function call
| LEX | program_unaligned.min | 'program' is not in the start of the line
| LEX | program_var_fail.min | 'program' appears in the start of the line inside the program itself
| SYMB | repeated1.min | Local variable 'a' has the same name as function argument 'a'
| SYMB | repeated2.min | Two global variables with same name and type
| SYMB | repeated3.min | Two global variables with same name but different type
| SYMB | repeated4.min | A global variable and a function with same name
| SYMB | repeated5.min | Two local variables (local to main funcition) with same name
| SEM | return_at_end.min | 'return' in the main function, but outside any block
| SEM | return_in_main.min | 'return' in the main function, but outside any block
| GRAM | semicolon1.min | No semicolon (;) after an assignement expression (x := 3)
| GRAM | semicolon2.min | Semicolon after last function parameter
| GRAM | semicolon3.min | Semicolon after last declaration (in the declarations zone)
| SEM | straritm.min | Add number to string
| SEM | strdecl.min | Assign number to string
| SEM | string0char.min | Assign literal char 0 ('\0') to string (Note: this is only valid for literal int (e.g. 0, 0b0, ...), not literal char)
| SEM | strwithnumber.min | Compare string with number (<)
| SEM | strwithnumber2.min | Compare string with number (>)
| SEM | strwithnumber3.min | Compare string with number (=)
| SEM | strwithnumber4.min | Compare string with number (>=)
| SEM | strwithnumber5.min | Compare string with number (<=)
| SEM | strwithnumber6.min | Compare string with number (~=)
| LEX | too_large_bin.min | 2^31 + 1 in binary
| LEX | too_large_dec.min | 2^31 + 1 in decimal
| LEX | too_large_hex.min | 2^31 + 1 in hexadecimal
| LEX | too_large_in_str.min | Tests a very large number (not representable in minor) as part of a string
| LEX | too_large_oct.min | 2^31 + 1 in octal
| SYMB | undefined1.min | Undefined variable
| SYMB | undefined_func.min | Calling function before definition
| LEX | unfinished_comment_2.min | A multiline comment that is never finished, after a finished one
| LEX | unfinished_comment.min | A multiline comment that is never finished
| SEM | void.min | Tries to use the result of a void function call in an expression
| SEM | void2.min | Tries to use the result of a void function call in an 'until' condition
| SEM | void3.min | Tries to use the result of a void function call in a comparison expression (<)
| SEM | void4.min | 'void' function that returns a call to another 'void' function
