# Транслятор языка do-while

**Автор:** Мальчихин Егор
**Группа:** КМБО-05-23
**Вариант:** 21

## Грамматика языка (БНФ)
<program> ::= <do-while-stmt>
<stmt> ::= <do-while-stmt> | <print-stmt> | <block>
<block> ::= "{" <stmt-list> "}"
<stmt-list> ::= ε | <stmt> <stmt-list>
<do-while-stmt> ::= "do" <stmt> "while" "(" <expr> ")" ";"
<print-stmt> ::= "print" "(" <arg-list> ")" ";"
<arg-list> ::= <expr> <arg-list-tail>
<arg-list-tail> ::= "," <expr> <arg-list-tail> | ε
<expr> ::= <unary> <expr-tail>
<expr-tail> ::= <relop> <unary> | ε
<relop> ::= "<" | ">"
<unary> ::= "++" <primary> | "--" <primary> | <primary>
<primary> ::= VAR | NUM | "(" <expr> ")"

## Сборка программы
Вариант 1: Makefile
make

Вариант 2: CMake
mkdir build && cd build
cmake ..
make

Вариант 3: Вручную
g++ -std=c++17 -o trns labatr.cpp

## Использование
Запуск с файлом:
./trns файл_с_программой.txt

Запуск с вводом с клавиатуры:
./trns
(введите программу, затем Ctrl+D для завершения ввода)

## Пример программы
do
    print(1, ++x, x > y);
while (x < 10);

## Тестирование
Все тесты в папке tests/:
correct.txt - корректная программа
lexical.txt - лексическая ошибка (@)
syntax.txt - синтаксическая ошибка (пропущена ;)
semantic.txt - семантическая ошибка (++5)

Ручная проверка:
./trns tests/correct.txt
./trns tests/lexical.txt
./trns tests/syntax.txt
./trns tests/semantic.txt

## Обработка ошибок
Лексическая: @ -> Lexical error at X:Y - unknown char '@'
Синтаксическая: пропущена ; -> Syntax error at X:Y - Expected ';', got ...
Семантическая: ++5 -> Runtime error: prefix ++/-- applied to non-variable at X:Y
