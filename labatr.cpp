// классический сканер и рекурсивный нисходящий предикативный распознаватель
// Грамматика (БНФ):
// <program> ::= <do-while-stmt>
// <stmt> ::= <do-while-stmt> | <print-stmt> | <block>
// <block> ::= "{" <stmt-list> "}"
// <stmt-list> ::= e | <stmt> <stmt-list>
// <do-while-stmt> ::= "do" <stmt> "while" "(" <expr> ")" ";"
// <print-stmt> ::= "print" "(" <arg-list> ")" ";"
// <arg-list> ::= <expr> <arg-list-tail>
// <arg-list-tail> ::= "," <expr> <arg-list-tail> | e
// <expr> ::= <unary> <expr-tail>
// <expr-tail> ::= <relop> <unary> | e
// <relop> ::= "<" | ">" 
// <unary> ::= "++" <primary> | "--" <primary> | <primary>
// <primary> ::= VAR | NUM | "(" <expr> ")"

#include <bits/stdc++.h>
using namespace std;

enum class TokenType {
    DO, WHILE, PRINT,
    LBRACE, RBRACE, LPAREN, RPAREN, SEMI, COMMA,
    LT, GT, INC, DEC,
    VAR, NUM,
    END
};

struct Token {
    TokenType type;
    string text;
    int line;
    int col;
};

struct LexError : runtime_error {
    int line, col;
    LexError(const string &m, int l, int c): runtime_error(m), line(l), col(c) {}
};

class Scanner {
    string src;
    size_t i = 0;
    int line = 1, col = 1;
public:
    Scanner(const string &s): src(s) {}
    bool eof() const { return i >= src.size(); }

    int peekChar() const { return eof() ? EOF : (unsigned char)src[i]; }
    int getChar() {
        if (eof()) return EOF;
        char c = src[i++];
        if (c == '\n') { line++; col = 1; }
        else col++;
        return (unsigned char)c;
    }
    void ungetChar() {
        if (i==0) return;
        i--;
        if (src[i] == '\n') { line--; col = 1; }
        else col = max(1, col-1);
    }

    void skipWS() {
        while (!eof()) {
            int c = peekChar();
            if (isspace(c)) { getChar(); continue; }
            break;
        }
    }

    Token nextToken() {
        skipWS();
        int startLine = line, startCol = col;
        if (eof()) return {TokenType::END, "", startLine, startCol};
        int c = getChar();

        if (c == '+') {
            if (peekChar() == '+') { getChar(); return {TokenType::INC, "++", startLine, startCol}; }
        }
        if (c == '-') {
            if (peekChar() == '-') { getChar(); return {TokenType::DEC, "--", startLine, startCol}; }
        }
        if (c == '{') return {TokenType::LBRACE, "{", startLine, startCol};
        if (c == '}') return {TokenType::RBRACE, "}", startLine, startCol};
        if (c == '(') return {TokenType::LPAREN, "(", startLine, startCol};
        if (c == ')') return {TokenType::RPAREN, ")", startLine, startCol};
        if (c == ';') return {TokenType::SEMI, ";", startLine, startCol};
        if (c == ',') return {TokenType::COMMA, ",", startLine, startCol};
        if (c == '<') return {TokenType::LT, "<", startLine, startCol};
        if (c == '>') return {TokenType::GT, ">", startLine, startCol};

        if (isalpha(c) || c=='_') {
            string s; s.push_back((char)c);
            while (isalnum(peekChar()) || peekChar()=='_') s.push_back((char)getChar());
            if (s == "do") return {TokenType::DO, s, startLine, startCol};
            if (s == "while") return {TokenType::WHILE, s, startLine, startCol};
            if (s == "print") return {TokenType::PRINT, s, startLine, startCol};
            return {TokenType::VAR, s, startLine, startCol};
        }

        if (isdigit(c)) {
            string s; s.push_back((char)c);
            while (isdigit(peekChar())) s.push_back((char)getChar());
            return {TokenType::NUM, s, startLine, startCol};
        }

        throw LexError(string("Lexical error: unknown char '") + (char)c + "'", startLine, startCol);
    }
};


struct ExecContext {
    unordered_map<string,int> vars;
    void runtimeErr(const string &m, int line=-1, int col=-1) {
        string s = "Runtime error: " + m;
        if (line>=0) s += " at " + to_string(line) + ":" + to_string(col);
        throw runtime_error(s);
    }
};

struct Expr {
    virtual ~Expr() = default;
    virtual int eval(ExecContext &ctx) = 0;
};

using ExprP = unique_ptr<Expr>;

struct Stmt {
    virtual ~Stmt() = default;
    virtual void exec(ExecContext &ctx) = 0;
};

using StmtP = unique_ptr<Stmt>;

struct ENum : Expr {
    int v; ENum(int x): v(x) {}
    int eval(ExecContext&) override { return v; }
};
struct EVar : Expr {
    string name; EVar(string s): name(move(s)) {}
    int eval(ExecContext &ctx) override {
        if (!ctx.vars.count(name)) ctx.vars[name] = 0;
        return ctx.vars[name];
    }
};
enum class UnOp { NONE, INC, DEC };
struct EUnary : Expr {
    UnOp op;
    ExprP prim;
    int line, col;
    EUnary(UnOp o, ExprP p, int ln=0,int cc=0): op(o), prim(move(p)), line(ln), col(cc) {}
    int eval(ExecContext &ctx) override {
        if (op == UnOp::NONE) return prim->eval(ctx);
        EVar* vptr = dynamic_cast<EVar*>(prim.get());
        if (!vptr) ctx.runtimeErr("prefix ++/-- applied to non-variable", line, col);
        int &slot = ctx.vars[vptr->name]; 
        if (op == UnOp::INC) ++slot; else --slot;
        return slot;
    }
};
struct ERel : Expr {
    ExprP left; ExprP right; char op;
    ERel(ExprP l,char o,ExprP r): left(move(l)), right(move(r)), op(o) {}
    int eval(ExecContext &ctx) override {
        int L = left->eval(ctx), R = right->eval(ctx);
        if (op == '<') return L < R;
        return L > R;
    }
};

struct SPrint : Stmt {
    vector<ExprP> args; int line,col;
    SPrint(vector<ExprP> a,int ln,int cc): args(move(a)), line(ln), col(cc) {}
    void exec(ExecContext &ctx) override {
        bool first = true;
        for (auto &e : args) {
            int v = e->eval(ctx);
            if (!first) cout << " ";
            cout << v;
            first = false;
        }
        cout << "\n";
    }
};
struct SBlock : Stmt {
    vector<StmtP> stmts;
    SBlock(vector<StmtP> v): stmts(move(v)) {}
    void exec(ExecContext &ctx) override {
        for (auto &s : stmts) s->exec(ctx);
    }
};
struct SDoWhile : Stmt {
    StmtP body; ExprP cond;
    SDoWhile(StmtP b, ExprP c): body(move(b)), cond(move(c)) {}
    void exec(ExecContext &ctx) override {
        do {
            body->exec(ctx);
        } while (cond->eval(ctx));
    }
};


struct ParseError : runtime_error {
    int line, col;
    ParseError(const string &m,int l,int c): runtime_error(m), line(l), col(c) {}
};

class Parser {
    Scanner &sc;
    Token look;
    bool hasLook = false;

    Token nextTok() { return sc.nextToken(); }
    void read() { look = nextTok(); hasLook = true; }
    void consume() { hasLook = false; }
    void expect(TokenType t, const string &msg="") {
        if (!hasLook) read();
        if (look.type != t) {
            string want = msg.empty() ? tokenName(t) : msg;
            throw ParseError("Expected " + want + ", got '" + look.text + "'", look.line, look.col);
        }
        consume();
    }

    static string tokenName(TokenType t) {
        switch(t){
            case TokenType::DO: return "'do'"; case TokenType::WHILE: return "'while'"; case TokenType::PRINT: return "'print'";
            case TokenType::LBRACE: return "'{'"; case TokenType::RBRACE: return "'}'"; case TokenType::LPAREN: return "'('";
            case TokenType::RPAREN: return "')'"; case TokenType::SEMI: return "';'"; case TokenType::COMMA: return "','";
            case TokenType::LT: return "'<'"; case TokenType::GT: return "'>'"; case TokenType::INC: return "'++'"; case TokenType::DEC: return "'--'";
            case TokenType::VAR: return "VAR"; case TokenType::NUM: return "NUM"; case TokenType::END: return "EOF";
        }
        return "TOKEN";
    }

public:
    Parser(Scanner &s): sc(s) { read(); }

    StmtP parse_program() {
        StmtP s = parse_do_while_stmt();
        if (hasLook && look.type != TokenType::END) {
            throw ParseError("Extra tokens after program", look.line, look.col);
        }
        return s;
    }

    StmtP parse_stmt() {
        if (!hasLook) read();
        if (look.type == TokenType::DO) return parse_do_while_stmt();
        if (look.type == TokenType::PRINT) return parse_print_stmt();
        if (look.type == TokenType::LBRACE) return parse_block();
        throw ParseError("Expected statement (do/print/{)", look.line, look.col);
    }

    StmtP parse_block() {
        expect(TokenType::LBRACE);
        vector<StmtP> v;
        while (true) {
            if (!hasLook) read();
            if (look.type == TokenType::RBRACE) break;
            v.push_back(parse_stmt());
        }
        expect(TokenType::RBRACE);
        return make_unique<SBlock>(move(v));
    }

    StmtP parse_do_while_stmt() {
        Token t = look;
        expect(TokenType::DO);
        StmtP body = parse_stmt();
        if (!hasLook) read();
        if (look.type != TokenType::WHILE) throw ParseError("Expected 'while' after do body", look.line, look.col);
        expect(TokenType::WHILE);
        expect(TokenType::LPAREN);
        ExprP cond = parse_expr();
        expect(TokenType::RPAREN);
        expect(TokenType::SEMI);
        return make_unique<SDoWhile>(move(body), move(cond));
    }

    StmtP parse_print_stmt() {
        Token t = look;
        expect(TokenType::PRINT);
        expect(TokenType::LPAREN);
        if (!hasLook) read();
        if (!(look.type==TokenType::INC || look.type==TokenType::DEC || look.type==TokenType::VAR || look.type==TokenType::NUM || look.type==TokenType::LPAREN))
            throw ParseError("print: expected expression", look.line, look.col);
        vector<ExprP> args;
        args.push_back(parse_expr());
        while (true) {
            if (!hasLook) read();
            if (look.type == TokenType::COMMA) { expect(TokenType::COMMA); args.push_back(parse_expr()); continue; }
            break;
        }
        expect(TokenType::RPAREN);
        expect(TokenType::SEMI);
        return make_unique<SPrint>(move(args), t.line, t.col);
    }

    ExprP parse_expr() {
        ExprP left = parse_unary();
        if (!hasLook) read();
        if (look.type == TokenType::LT || look.type == TokenType::GT) {
            char op = (look.type==TokenType::LT)?'<':'>';
            consume();
            ExprP right = parse_unary();
            return make_unique<ERel>(move(left), op, move(right));
        }
        return left;
    }

    ExprP parse_unary() {
        if (!hasLook) read();
        if (look.type == TokenType::INC) {
            Token t = look; expect(TokenType::INC);
            ExprP p = parse_primary();
            return make_unique<EUnary>(UnOp::INC, move(p), t.line, t.col);
        }
        if (look.type == TokenType::DEC) {
            Token t = look; expect(TokenType::DEC);
            ExprP p = parse_primary();
            return make_unique<EUnary>(UnOp::DEC, move(p), t.line, t.col);
        }
        ExprP p = parse_primary();
        return make_unique<EUnary>(UnOp::NONE, move(p));
    }

    ExprP parse_primary() {
        if (!hasLook) read();
        if (look.type == TokenType::VAR) {
            string name = look.text; consume();
            return make_unique<EVar>(move(name));
        }
        if (look.type == TokenType::NUM) {
            int v = stoi(look.text); consume();
            return make_unique<ENum>(v);
        }
        if (look.type == TokenType::LPAREN) {
            consume();
            ExprP e = parse_expr();
            expect(TokenType::RPAREN);
            return e;
        }
        throw ParseError("Expected primary (VAR, NUM, or '(')", look.line, look.col);
    }
};


int main(int argc, char** argv){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    try {
        string input;
        if (argc >= 2) {
            ifstream f(argv[1]);
            if (!f) { cerr << "Cannot open file: " << argv[1] << "\n"; return 1; }
            input.assign(istreambuf_iterator<char>(f), {});
        } else {
            input.assign(istreambuf_iterator<char>(cin), {});
        }

        Scanner scanner(input);
        Parser parser(scanner);
        StmtP prog = parser.parse_program();

        ExecContext ctx;
        prog->exec(ctx);
        return 0;

    } catch (LexError &le) {
        cerr << "Lexical error at " << le.line << ":" << le.col << " - " << le.what() << "\n";
        return 1;
    } catch (ParseError &pe) {
        cerr << "Syntax error at " << pe.line << ":" << pe.col << " - " << pe.what() << "\n";
        return 1;
    } catch (runtime_error &re) {
        cerr << re.what() << "\n";
        return 1;
    } catch (exception &e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}