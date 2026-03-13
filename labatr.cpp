// синтаксический анализатор для управляющих конструкций и выражений
// форма грамматики: контекстно-свободная (тип 2 по Хомскому)
// <program> ::= <loop-construct>
// <statement> ::= <loop-construct> | <output-construct> | <compound>
// <compound> ::= "{" <statement-sequence> "}"
// <statement-sequence> ::= ε | <statement> <statement-sequence>
// <loop-construct> ::= "do" <statement> "while" "(" <expression> ")" ";"
// <output-construct> ::= "print" "(" <argument-sequence> ")" ";"
// <argument-sequence> ::= <expression> <argument-sequence-tail>
// <argument-sequence-tail> ::= "," <expression> <argument-sequence-tail> | ε
// <expression> ::= <prefix-operation> <expression-tail>
// <expression-tail> ::= <comparison> <prefix-operation> | ε
// <comparison> ::= "<" | ">"
// <prefix-operation> ::= "++" <atomic> | "--" <atomic> | <atomic>
// <atomic> ::= IDENTIFIER | NUMBER | "(" <expression> ")"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cctype>
#include <algorithm>

using std::string;
using std::vector;
using std::unordered_map;
using std::unique_ptr;
using std::make_unique;
using std::cout;
using std::cerr;


enum class Category {
    CYCLE_DO, CYCLE_WHILE, OUTPUT_PRINT,
    BRACE_OPEN, BRACE_CLOSE, PAREN_OPEN, PAREN_CLOSE, TERMINATOR, SEPARATOR,
    LESS, GREATER, INCREMENT, DECREMENT,
    IDENTIFIER, CONSTANT,
    END_MARKER
};

struct Lexeme {
    Category kind;
    string value;
    int row;
    int column;
};

struct ScanningError : std::exception {
    string message;
    int row;
    int column;
    
    ScanningError(const string& msg, int r, int c) 
        : message(msg), row(r), column(c) {}
    
    const char* what() const noexcept override {
        return message.c_str();
    }
};


class Tokenizer {
    string source;
    size_t position = 0;
    int currentRow = 1;
    int currentColumn = 1;
    
public:
    Tokenizer(const string& input) : source(input) {}
    
    bool isAtEnd() const {
        return position >= source.length();
    }
    
    int lookAhead() const {
        return isAtEnd() ? EOF : static_cast<unsigned char>(source[position]);
    }
    
    int fetchCharacter() {
        if (isAtEnd()) return EOF;
        char ch = source[position++];
        if (ch == '\n') {
            currentRow++;
            currentColumn = 1;
        } else {
            currentColumn++;
        }
        return static_cast<unsigned char>(ch);
    }
    
    void pushBack() {
        if (position == 0) return;
        position--;
        if (source[position] == '\n') {
            currentRow--;
            currentColumn = 1;
        } else {
            currentColumn = std::max(1, currentColumn - 1);
        }
    }
    
    void skipWhitespace() {
        while (!isAtEnd()) {
            int ch = lookAhead();
            if (std::isspace(ch)) {
                fetchCharacter();
                continue;
            }
            break;
        }
    }
    
    Lexeme extractNext() {
        skipWhitespace();
        int startRow = currentRow;
        int startColumn = currentColumn;
        
        if (isAtEnd()) {
            return {Category::END_MARKER, "", startRow, startColumn};
        }
        
        int ch = fetchCharacter();
     
        if (ch == '+') {
            if (lookAhead() == '+') {
                fetchCharacter();
                return {Category::INCREMENT, "++", startRow, startColumn};
            }
        }
        if (ch == '-') {
            if (lookAhead() == '-') {
                fetchCharacter();
                return {Category::DECREMENT, "--", startRow, startColumn};
            }
        }
        
      
        switch (ch) {
            case '{': return {Category::BRACE_OPEN, "{", startRow, startColumn};
            case '}': return {Category::BRACE_CLOSE, "}", startRow, startColumn};
            case '(': return {Category::PAREN_OPEN, "(", startRow, startColumn};
            case ')': return {Category::PAREN_CLOSE, ")", startRow, startColumn};
            case ';': return {Category::TERMINATOR, ";", startRow, startColumn};
            case ',': return {Category::SEPARATOR, ",", startRow, startColumn};
            case '<': return {Category::LESS, "<", startRow, startColumn};
            case '>': return {Category::GREATER, ">", startRow, startColumn};
        }
        
       
        if (std::isalpha(ch) || ch == '_') {
            string text;
            text.push_back(static_cast<char>(ch));
            while (std::isalnum(lookAhead()) || lookAhead() == '_') {
                text.push_back(static_cast<char>(fetchCharacter()));
            }
            if (text == "do") return {Category::CYCLE_DO, text, startRow, startColumn};
            if (text == "while") return {Category::CYCLE_WHILE, text, startRow, startColumn};
            if (text == "print") return {Category::OUTPUT_PRINT, text, startRow, startColumn};
            return {Category::IDENTIFIER, text, startRow, startColumn};
        }
     
        if (std::isdigit(ch)) {
            string text;
            text.push_back(static_cast<char>(ch));
            while (std::isdigit(lookAhead())) {
                text.push_back(static_cast<char>(fetchCharacter()));
            }
            return {Category::CONSTANT, text, startRow, startColumn};
        }
        
   
        throw ScanningError(
            string("недопустимый символ '") + static_cast<char>(ch) + "'", 
            startRow, startColumn
        );
    }
};


class RuntimeEnvironment {
public:
    unordered_map<string, int> storage;
    
    void signalError(const string& description, int row = -1, int col = -1) {
        string fullMessage = "ошибка выполнения: " + description;
        if (row >= 0) {
            fullMessage += " (строка " + std::to_string(row) + 
                          ", позиция " + std::to_string(col) + ")";
        }
        throw std::runtime_error(fullMessage);
    }
};


struct CalculationNode {
    virtual ~CalculationNode() = default;
    virtual int evaluate(RuntimeEnvironment& env) = 0;
};

using CalculationPtr = unique_ptr<CalculationNode>;

struct InstructionNode {
    virtual ~InstructionNode() = default;
    virtual void execute(RuntimeEnvironment& env) = 0;
};

using InstructionPtr = unique_ptr<InstructionNode>;


class ConstantLeaf : public CalculationNode {
    int value;
public:
    ConstantLeaf(int val) : value(val) {}
    int evaluate(RuntimeEnvironment&) override {
        return value;
    }
};


class VariableLeaf : public CalculationNode {
    string identifier;
public:
    VariableLeaf(string id) : identifier(std::move(id)) {}
    int evaluate(RuntimeEnvironment& env) override {
        if (!env.storage.count(identifier)) {
            env.storage[identifier] = 0;
        }
        return env.storage[identifier];
    }
};


enum class PrefixOperator { PLAIN, INCREASE, DECREASE };


class PrefixExpression : public CalculationNode {
    PrefixOperator operation;
    CalculationPtr operand;
    int errorRow;
    int errorCol;
    
public:
    PrefixExpression(PrefixOperator op, CalculationPtr arg, int row = 0, int col = 0)
        : operation(op), operand(std::move(arg)), errorRow(row), errorCol(col) {}
    
    int evaluate(RuntimeEnvironment& env) override {
        if (operation == PrefixOperator::PLAIN) {
            return operand->evaluate(env);
        }

        auto* variableNode = dynamic_cast<VariableLeaf*>(operand.get());
        if (!variableNode) {
            env.signalError("++/-- можно применять только к переменным", 
                          errorRow, errorCol);
        }
        
        int& storageLocation = env.storage[variableNode->identifier];
        if (operation == PrefixOperator::INCREASE) {
            return ++storageLocation;
        } else {
            return --storageLocation;
        }
    }
};

class ComparisonExpression : public CalculationNode {
    CalculationPtr leftSide;
    CalculationPtr rightSide;
    char comparator;
    
public:
    ComparisonExpression(CalculationPtr left, char comp, CalculationPtr right)
        : leftSide(std::move(left)), comparator(comp), rightSide(std::move(right)) {}
    
    int evaluate(RuntimeEnvironment& env) override {
        int leftValue = leftSide->evaluate(env);
        int rightValue = rightSide->evaluate(env);
        return (comparator == '<') ? (leftValue < rightValue) : (leftValue > rightValue);
    }
};


class PrintInstruction : public InstructionNode {
    vector<CalculationPtr> arguments;
    int errorRow;
    int errorCol;
    
public:
    PrintInstruction(vector<CalculationPtr> args, int row, int col)
        : arguments(std::move(args)), errorRow(row), errorCol(col) {}
    
    void execute(RuntimeEnvironment& env) override {
        bool firstElement = true;
        for (auto& expr : arguments) {
            int value = expr->evaluate(env);
            if (!firstElement) {
                cout << " ";
            }
            cout << value;
            firstElement = false;
        }
        cout << "\n";
    }
};


class BlockInstruction : public InstructionNode {
    vector<InstructionPtr> instructions;
    
public:
    BlockInstruction(vector<InstructionPtr> stmts) : instructions(std::move(stmts)) {}
    
    void execute(RuntimeEnvironment& env) override {
        for (auto& stmt : instructions) {
            stmt->execute(env);
        }
    }
};


class DoWhileLoop : public InstructionNode {
    InstructionPtr loopBody;
    CalculationPtr loopCondition;
    
public:
    DoWhileLoop(InstructionPtr body, CalculationPtr condition)
        : loopBody(std::move(body)), loopCondition(std::move(condition)) {}
    
    void execute(RuntimeEnvironment& env) override {
        do {
            loopBody->execute(env);
        } while (loopCondition->evaluate(env));
    }
};


struct SyntaxError : std::exception {
    string message;
    int row;
    int column;
    
    SyntaxError(const string& msg, int r, int c) 
        : message(msg), row(r), column(c) {}
    
    const char* what() const noexcept override {
        return message.c_str();
    }
};


class SyntaxAnalyzer {
    Tokenizer& tokenSource;
    Lexeme currentToken;
    bool tokenCached = false;
    
    Lexeme retrieveToken() {
        return tokenSource.extractNext();
    }
    
    void advance() {
        currentToken = retrieveToken();
        tokenCached = true;
    }
    
    void consumeToken() {
        tokenCached = false;
    }
    
    void verify(Category expected, const string& hint = "") {
        if (!tokenCached) {
            advance();
        }
        if (currentToken.kind != expected) {
            string expectation = hint.empty() ? describeCategory(expected) : hint;
            throw SyntaxError(
                "ожидается " + expectation + ", получено '" + currentToken.value + "'",
                currentToken.row,
                currentToken.column
            );
        }
        consumeToken();
    }
    
    static string describeCategory(Category cat) {
        switch(cat) {
            case Category::CYCLE_DO: return "'do'";
            case Category::CYCLE_WHILE: return "'while'";
            case Category::OUTPUT_PRINT: return "'print'";
            case Category::BRACE_OPEN: return "'{'";
            case Category::BRACE_CLOSE: return "'}'";
            case Category::PAREN_OPEN: return "'('";
            case Category::PAREN_CLOSE: return "')'";
            case Category::TERMINATOR: return "';'";
            case Category::SEPARATOR: return "','";
            case Category::LESS: return "'<'";
            case Category::GREATER: return "'>'";
            case Category::INCREMENT: return "'++'";
            case Category::DECREMENT: return "'--'";
            case Category::IDENTIFIER: return "идентификатор";
            case Category::CONSTANT: return "число";
            case Category::END_MARKER: return "конец файла";
            default: return "токен";
        }
    }
    
public:
    SyntaxAnalyzer(Tokenizer& src) : tokenSource(src) {
        advance();
    }
    
    // главная точка входа: <program> ::= <do-while-stmt>
    InstructionPtr parseWholeProgram() {
        InstructionPtr result = parseDoWhileConstruct();
        if (tokenCached && currentToken.kind != Category::END_MARKER) {
            throw SyntaxError(
                "лишние символы после завершения программы",
                currentToken.row,
                currentToken.column
            );
        }
        return result;
    }
    
    InstructionPtr parseAnyStatement() {
        if (!tokenCached) {
            advance();
        }
        
        if (currentToken.kind == Category::CYCLE_DO) {
            return parseDoWhileConstruct();
        }
        if (currentToken.kind == Category::OUTPUT_PRINT) {
            return parsePrintConstruct();
        }
        if (currentToken.kind == Category::BRACE_OPEN) {
            return parseBlockConstruct();
        }
        
        throw SyntaxError(
            "ожидается инструкция (do, print или '{')",
            currentToken.row,
            currentToken.column
        );
    }

    InstructionPtr parseBlockConstruct() {
        verify(Category::BRACE_OPEN);
        
        vector<InstructionPtr> sequence;
        while (true) {
            if (!tokenCached) {
                advance();
            }
            if (currentToken.kind == Category::BRACE_CLOSE) {
                break;
            }
            sequence.push_back(parseAnyStatement());
        }
        
        verify(Category::BRACE_CLOSE);
        return make_unique<BlockInstruction>(std::move(sequence));
    }
    
  
    InstructionPtr parseDoWhileConstruct() {
        Lexeme startToken = currentToken;
        verify(Category::CYCLE_DO);
        
        InstructionPtr body = parseAnyStatement();
        
        if (!tokenCached) {
            advance();
        }
        if (currentToken.kind != Category::CYCLE_WHILE) {
            throw SyntaxError(
                "после тела цикла ожидается 'while'",
                currentToken.row,
                currentToken.column
            );
        }
        
        verify(Category::CYCLE_WHILE);
        verify(Category::PAREN_OPEN);
        CalculationPtr condition = parseExpression();
        verify(Category::PAREN_CLOSE);
        verify(Category::TERMINATOR);
        
        return make_unique<DoWhileLoop>(std::move(body), std::move(condition));
    }
    
  
    InstructionPtr parsePrintConstruct() {
        Lexeme startToken = currentToken;
        verify(Category::OUTPUT_PRINT);
        verify(Category::PAREN_OPEN);
        
        if (!tokenCached) {
            advance();
        }
        
   
        bool canStartExpression = 
            currentToken.kind == Category::INCREMENT ||
            currentToken.kind == Category::DECREMENT ||
            currentToken.kind == Category::IDENTIFIER ||
            currentToken.kind == Category::CONSTANT ||
            currentToken.kind == Category::PAREN_OPEN;
            
        if (!canStartExpression) {
            throw SyntaxError(
                "print: ожидается выражение",
                currentToken.row,
                currentToken.column
            );
        }
        
        vector<CalculationPtr> arguments;
        arguments.push_back(parseExpression());
        
        while (true) {
            if (!tokenCached) {
                advance();
            }
            if (currentToken.kind == Category::SEPARATOR) {
                verify(Category::SEPARATOR);
                arguments.push_back(parseExpression());
                continue;
            }
            break;
        }
        
        verify(Category::PAREN_CLOSE);
        verify(Category::TERMINATOR);
        
        return make_unique<PrintInstruction>(
            std::move(arguments), 
            startToken.row, 
            startToken.column
        );
    }
    
   
    CalculationPtr parseExpression() {
        CalculationPtr leftPart = parseUnaryOperation();
        
        if (!tokenCached) {
            advance();
        }
        
        if (currentToken.kind == Category::LESS || 
            currentToken.kind == Category::GREATER) {
            char comparisonOp = (currentToken.kind == Category::LESS) ? '<' : '>';
            consumeToken();
            CalculationPtr rightPart = parseUnaryOperation();
            return make_unique<ComparisonExpression>(
                std::move(leftPart), comparisonOp, std::move(rightPart)
            );
        }
        
        return leftPart;
    }
   
    CalculationPtr parseUnaryOperation() {
        if (!tokenCached) {
            advance();
        }
        
        if (currentToken.kind == Category::INCREMENT) {
            Lexeme opToken = currentToken;
            verify(Category::INCREMENT);
            CalculationPtr atomic = parseAtomicElement();
            return make_unique<PrefixExpression>(
                PrefixOperator::INCREASE, 
                std::move(atomic),
                opToken.row,
                opToken.column
            );
        }
        
        if (currentToken.kind == Category::DECREMENT) {
            Lexeme opToken = currentToken;
            verify(Category::DECREMENT);
            CalculationPtr atomic = parseAtomicElement();
            return make_unique<PrefixExpression>(
                PrefixOperator::DECREASE, 
                std::move(atomic),
                opToken.row,
                opToken.column
            );
        }
        
        CalculationPtr atomic = parseAtomicElement();
        return make_unique<PrefixExpression>(
            PrefixOperator::PLAIN, 
            std::move(atomic)
        );
    }
    
 
    CalculationPtr parseAtomicElement() {
        if (!tokenCached) {
            advance();
        }
        
        if (currentToken.kind == Category::IDENTIFIER) {
            string name = currentToken.value;
            consumeToken();
            return make_unique<VariableLeaf>(name);
        }
        
        if (currentToken.kind == Category::CONSTANT) {
            int numericValue = std::stoi(currentToken.value);
            consumeToken();
            return make_unique<ConstantLeaf>(numericValue);
        }
        
        if (currentToken.kind == Category::PAREN_OPEN) {
            consumeToken();
            CalculationPtr enclosed = parseExpression();
            verify(Category::PAREN_CLOSE);
            return enclosed;
        }
        
        throw SyntaxError(
            "ожидается идентификатор, число или '('",
            currentToken.row,
            currentToken.column
        );
    }
};


int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    
    try {
        string programSource;
        
        if (argc >= 2) {
            std::ifstream sourceFile(argv[1]);
            if (!sourceFile) {
                cerr << "невозможно открыть файл: " << argv[1] << "\n";
                return 1;
            }
            programSource.assign(
                std::istreambuf_iterator<char>(sourceFile),
                std::istreambuf_iterator<char>()
            );
        } else {
            programSource.assign(
                std::istreambuf_iterator<char>(std::cin),
                std::istreambuf_iterator<char>()
            );
        }
        
        Tokenizer tokenizer(programSource);
        SyntaxAnalyzer parser(tokenizer);
        InstructionPtr program = parser.parseWholeProgram();
        
        RuntimeEnvironment environment;
        program->execute(environment);
        
        return 0;
        
    } catch (const ScanningError& se) {
        cerr << "лексическая ошибка в " << se.row << ":" << se.column 
             << " - " << se.what() << "\n";
        return 1;
    } catch (const SyntaxError& se) {
        cerr << "синтаксическая ошибка в " << se.row << ":" << se.column 
             << " - " << se.what() << "\n";
        return 1;
    } catch (const std::runtime_error& re) {
        cerr << re.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        cerr << "ошибка: " << e.what() << "\n";
        return 1;
    }
}