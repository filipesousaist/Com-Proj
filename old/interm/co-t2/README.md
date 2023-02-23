## Testes para Compiladores - Parte 2

**Renomear minor-RENAME-THIS.brg para minor.brg na pasta src.**

**Lê isto com atenção:**

O diretório `/src` contém o código da base do projeto
como dado pelo professor (o **pburg está incompleto**).

O diretório `/test` tem todos os testes da primeira entrega, dados pelo professor, mais os testes para a segunda entrega.

## Correr os testes

```sh
cd test
chmod +x p2.sh # Este comando só precisa de ser feito 1 vez
./p2.sh
make clean # Isto limpa os resultados anteriores
```

## Testes da segunda entrega

Encontram-se em `test/tests_p2`.

Cada grupo de testes possui a sua pastinha com um ficheiro e várias pastas:
* `test.min` - contém o código do programa a ser testado
* `t*` - pasta com um teste específico (t1, t2, etc.) 

Cada pasta `t*` possui os seguintes ficheiros:

* `in` - input dado 
* `out` - output esperado
* `ret` - exit code esperado (return da main)

Ao correr os testes, as pastas vão ter mais uns ficheiros:
* `myret` - exit code obtido
* `myout` - output obtido
* `diff` - diferenças entre o `myout` e `out` (literalmente um diff)

Os testes passam se os outputs e exit codes obtidos são iguais aos esperados. Ou seja, se `out == myout` e `ret == myret`.

## Makefile da root

O makefile da root corre o make da `src` e corre os testes do `test`.
```sh
make # Compila e corre os testes
make clean # Limpa tudo
```
## Como contribuir

Para contribuir, podes:

Fazer um fork do projeto, fazer as tuas alterações e depois um pull request.

*ou*

Pedir-me para ter acesso de escrita (através do slack (Afonso Matos)), e assim podes criar o teu branch e fazer pull request.
