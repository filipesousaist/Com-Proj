`./run-tests.sh sourceDir`
OR
`./stages.sh sourceDir` (which is a little slower, but also lists passed/total FAIL tests by project stage)

Where `sourceDir` contains a Makefile for your compiler.

The Makefile should generate a `minor` executable file.

`PASSED TESTS` are files which your compiler should compile without errors.

`FAILED TESTS` are files which your compiler should (or not) compile _with_ errors. 

**EXAMPLE**

```
co-test (this project)
├── test
├── run-tests.sh
co-minor (your project)
├── lib
├── Makefile
├── minor.l
├── minor.y
├── run
└── mino
```

```
cd co-test
./run-tests.sh ../co-minor
no byacc conflicts, well done
 ===== PASS TESTS ===== 
PASSED test/pass/enunciado/iter.min
PASSED test/pass/enunciado/hondt.min
PASSED test/pass/enunciado/cast.min
PASSED test/pass/enunciado/isbn10.min
...
 ===== FAIL TESTS ===== 
PASSED test/fail/ack.min
```
