/*
 * parse.cc
 *
 *  Created on: 2018/7/6
 *      Author: xiaoc
 */


#include "parse.h"

using namespace kcc;

Parser::Parser(Lexer &lex) {
    pos = -1;
    tokenStream = lex.getTokenStream();
    int prec = 0;
    /*
     *   opPrec[","] = prec;
     * prec++;*/
    opPrec["="] = prec;
    opPrec["+="] = prec;
    opPrec["-="] = prec;
    opPrec["*="] = prec;
    opPrec["/="] = prec;
    opPrec[">>="] = prec;
    opPrec["<<="] = prec;
    opPrec["%="] = prec;
    opPrec["|="] = prec;
    opPrec["&="] = prec;
    opPrec["^="] = prec;
    opPrec["&&="] = prec;
    opPrec["||="] = prec;
    prec++;
    opPrec["?"] = ternaryPrec = prec;
    prec++;
    opPrec["||"] = prec;
    prec++;
    opPrec["&&"] = prec;
    prec++;
    opPrec["|"] = prec;
    prec++;
    opPrec["^"] = prec;
    opPrec["&"] = prec;
    prec++;
    opPrec["=="] = prec;
    opPrec["!="] = prec;
    prec++;
    opPrec[">="] = prec;
    opPrec["<="] = prec;
    opPrec[">"] = prec;
    opPrec["<"] = prec;

    prec++;
    opPrec[">>"] = prec;
    opPrec["<<"] = prec;
    prec++;
    opPrec["+"] = prec;
    opPrec["-"] = prec;
    prec++;
    opPrec["*"] = prec;
    opPrec["/"] = prec;
    opPrec["%"] = prec;
    prec++;
    opPrec["."] = prec;
    opPrec["->"] = prec;
    opAssoc = {
            {"+=",  0},
            {"-=",  0},
            {"*=",  0},
            {"/=",  0},
            {">>=", 0},
            {"<<=", 0},
            {"%=",  0},
            {"&=",  0},
            {"|=",  0},
            {":=",  0},
            {"::=", 0},
            {"=",   0},
            {".",   1},
            {"->",  1},
            {"+",   1},
            {"-",   1},
            {"*",   1},
            {"/",   1},
            {"!=",  1},
            {"==",  1},
            {">",   1},
            {">=",  1},
            {"<=",  1},
            {"<",   1},
            {"%",   1},
            {"&&",  1},
            {"&",   1},
            {"||",  1},
            {"|",   1}
    };

}

const Token &Parser::at(int idx) const {
    static Token nil = Token();
    if (idx >= this->tokenStream.size() || idx < 0) {
        return nil;
    } else {
        return tokenStream[idx];
    }
}

AST::AST *Parser::parse() {
    auto root = new AST::TopLevel();
    try {
        while (hasNext()) {
            root->add(parseGlobalDefs());
        }
    } catch (ParserException &e) {
        std::cerr << e.what() << std::endl;
    }
    return root;
}

AST::AST *Parser::parseExpr(int lev) {
    AST::AST *result = parseCastExpr();
    while (hasNext()) {
        auto next = peek();
        if (opPrec.find(next.tok) == opPrec.end())
            break;
        if (opPrec[next.tok] >= lev) {
            if (next.tok == "?") {
                result = parseTernary(result);
            } else {
                consume();
                AST::AST *rhs = parseExpr(opAssoc[next.tok] + opPrec[next.tok]);
                AST::BinaryExpression *op = makeNode<AST::BinaryExpression>(next);
                op->add(result);
                op->add(rhs);
                result = hackExpr(op);
            }
        } else
            break;
    }
    return result;
}

AST::AST *Parser::parseUnary() {
    if (has("+") || has("-") || has("++") || has("--")
        || has("*")
        || has("&")
        || has("!")) {
        consume();
        auto expr = makeNode<AST::UnaryExpression>(cur());
        expr->add(parseCastExpr());
        return expr;
    } else if (has("sizeof")) {
        consume();
        auto expr = makeNode<AST::UnaryExpression>(cur());
        expect("(");
        expr->add(parseTypeName());
        expect(")");
        return expr;
    }
    return parsePostfix();
}

AST::AST *Parser::parsePostfix() {
    auto postfix = parsePrimary();
    static std::set<std::string> postfixOperator = {
            "++", "--", "(", "[",//".","->"
    };
    while (hasNext() && postfixOperator.find(peek().tok) != postfixOperator.end()) {
        if (has("[")) {
            consume();/*
            auto index = new IndexExpression();
            index->add(postfix);
            index->add(parseExpr(0));*/
            auto add = makeNode<AST::BinaryExpression>(Token(Token::Type::Punctuator,
                                                             "+", -1, -1));
            add->add(postfix);
            add->add(parseExpr(0));
            auto index = makeNode<AST::UnaryExpression>(Token(Token::Type::Punctuator,
                                                              "*", -1, -1));
            index->add(add);
            postfix = index;
            expect("]");
        } else if (has("(")) {
            auto arg = parseArgumentExpressionList();
            auto call = makeNode<AST::CallExpression>();
            call->add(postfix);
            call->add(arg);
            postfix = call;
        } else {
            auto p = makeNode<AST::PostfixExpr>(peek());
            p->add(postfix);
            postfix = p;
            consume();
        }
    }
    return postfix;
}

AST::AST *Parser::parsePrimary() {
    auto &next = peek();
    if (next.type == Token::Type::Identifier) {
        consume();
        auto iden = cur();
        if (enums.find(iden.tok) != enums.end()) {
            Token tok(Token::Type::Int, fmt::format("{}", enums[iden.tok]), -1, -1);
            auto n = makeNode<AST::Number>(tok);
            return n;
        } else {
            return makeNode<AST::Identifier>(iden);
        }
    } else if (next.type == Token::Type::Int || next.type == Token::Type::Float) {
        consume();
        return makeNode<AST::Number>(cur());
    } else if (next.type == Token::Type::String) {
        consume();
        return makeNode<AST::Literal>(cur());
    } else if (has("(")) {
        consume();
        auto expr = parseExpr(0);
        expect(")");
        return expr;
    } else {
        return nullptr;
    }
}

AST::AST *Parser::parseTernary(AST::AST *cond) {
    expect("?");
    AST::AST *ternary = makeNode<AST::TernaryExpression>();
    ternary->add(cond);
    ternary->add(parseExpr(0));
    expect(":");
    ternary->add(parseExpr(0));
    return ternary;
}

AST::AST *Parser::parseCastExpr() {
    auto ty = at(pos + 2).tok;
    if (has("(") && canItBeSomeKindOfType(ty)) {  //( kind ) cast_expr
        consume();
        auto cast = makeNode<AST::CastExpression>();
        auto type = parseTypeSpecifier();
        declStack.emplace_back(type);
        parseAbstractDeclarator();
        type = declStack.back();
        declStack.pop_back();
        cast->add(type);
        expect(")");
        cast->add(parseCastExpr());
        return cast;
    } else
        return parseUnary();
}

AST::AST *Parser::parseArgumentExpressionList() {
    expect("(");
    auto arg = makeNode<AST::ArgumentExepressionList>();
    while (hasNext() && !has(")")) {
        arg->add(parseExpr(0));
        if (has(")"))
            break;
        expect(",");
    }
    expect(")");
    return arg;
}

AST::AST *Parser::parseBlock() {
    if (has("{")) {
        consume();
        types.push();
        auto block = makeNode<AST::Block>();
        while (hasNext() && !has("}")) {
            block->add(parseStmt());
        }
        expect("}");
        types.pop();
        return block;
    } else {
        return parseStmt();
    }
}

AST::AST *Parser::parseIf() {
    auto stmt = makeNode<AST::If>();
    expect("if");
    expect("(");
    stmt->add(parseExpr(0));
    expect(")");
    stmt->add(parseBlock());
    if (has("else")) {
        consume();
        stmt->add(parseBlock());
    }
    return stmt;
}

AST::AST *Parser::parseWhile() {
    auto w = makeNode<AST::While>();
    expect("while");
    expect("(");
    w->add(parseExpr(0));
    expect(")");
    w->add(parseBlock());
    return w;
}

AST::AST *Parser::parseDoWhile() {
    auto w = makeNode<AST::DoWhile>();
    expect("do");
    w->add(parseBlock());
    expect("while");
    expect("(");
    w->add(parseExpr(0));
    expect(")");
    expect(";");
    return w;
}


AST::AST *Parser::parseStmt() {
    if (has("if"))
        return parseIf();
    else if (has("while")) {
        return parseWhile();
    } else if (has("do")) {
        return parseDoWhile();
    } else if (has("for")) {
        return parseFor();
    } else if (has("return")) {
        return parseReturn();
    } else if (has("break")) {
        consume();
        expect(";");
        return makeNode<AST::Break>();
    } else if (has("continue")) {
        consume();
        expect(";");
        return makeNode<AST::Continue>();
    } else if (canItBeSomeKindOfType(peek().tok)) {
        auto e = parseDecl();
        expect(";");
        return e;
    } else if (has("{")) {
        return parseBlock();
    } else {
        auto a = parseExpr(0);
        expect(";");
        return a;
    }
}


void Parser::expect(const std::string &token) {
    if (peek().tok != token) {
        auto msg = fmt::format(
                "'{}' expected but found '{}'", token, peek().tok
        );
        if (config[quitIfError]) {
            throw ParserException(msg, cur().line, cur().col);
        } else {
            fmt::print("{}\n", msg);
        }
    } else {
        consume();
    }
}

bool Parser::has(const std::string &token) {
    return peek().tok == token;
}

AST::AST *Parser::parseFuncDef() {
    auto ty = parseTypeSpecifier();
    auto func = makeNode<AST::FuncDef>();
    func->add(ty);
    func->add(parsePrimary());
    func->add(parseFuncDefArg());
    func->add(parseBlock());
    return func;
}

AST::AST *Parser::parseFuncDefArg() {
    expect("(");
    auto arg = makeNode<AST::FuncDefArg>();
    while (hasNext() && !has(")")) {
        if (has("...")) {
            consume();
            arg->add(makeNode<AST::VariadicArgType>());
            break;
        }
        arg->add(parseParameterType()->first());
        if (has(")"))
            break;
        expect(",");
    }
    expect(")");
    return arg;
}

AST::AST *Parser::parseReturn() {
    expect("return");
    if (has(";")) {
        consume();
        return makeNode<AST::Return>();
    } else {
        auto ret = makeNode<AST::Return>();
        ret->add(parseExpr(0));
        expect(";");
        return ret;
    }
}


/*  Simplifier declaration
 *  declaration: kind-specifier direct-declarator
 *  declarator: {pointer}  direct-declarator
 *  direct-declarator: direct-declarator' {   array-declarator | function-declarator }
 *  direct-declarator': identifier | '('  declarator ')'
 *  array-declarator: '[' int-literal ']'
 *  function-declarator: '(' function-decl-arg ')'
 * */
AST::AST *Parser::parseTypeSpecifier() {
    if (types.contains(peek().tok)) {
        consume();
        return makeNode<AST::PrimitiveType>(cur());
    } else if (has("struct")) {
        if (types.contains(fmt::format("struct {}", at(pos + 2).tok))) {
            expect("struct");
            consume();
            if (peek().tok == ";") {
                auto ty = makeNode<AST::ForwardStructDecl>();
                ty->name = fmt::format("struct {}", cur().tok);
                return ty;
            } else {
                auto ty = makeNode<AST::StructType>();
                ty->name = fmt::format("struct {}", cur().tok);
                return ty;
            }
        } else {
            return parseStruct();
        }
    } else {
        error(fmt::format("??? {}", peek().tok));
        KCC_NOT_IMPLEMENTED();
    }
}

AST::AST *Parser::parseStruct() {
    expect("struct");
    static int counter = 0;
    std::string name;

    if (has("{")) {
        name = fmt::format("#uname{}", counter++);
    } else {
        if (auto identifier = dynamic_cast<AST::Identifier *>(parsePrimary())) {
            name = identifier->tok();
        } else {
            error("expected identifier");
        }
    }
    if (has(";")) {
        auto node = makeNode<AST::ForwardStructDecl>();
        node->name = name;
        return node;
    }
    auto node = makeNode<AST::StructDecl>();
    node->name = format("struct {}", name);
    types.add(node->name);
    expect("{");
    while (!has("}")) {
        node->add(parseDecl());
        expect(";");
    }
    expect("}");
    return node;
}

AST::AST *Parser::parseGlobalDefs() {
    if (has("enum")) {
        return parseEnum();
    } else {
        auto result = parseDecl();
        if (result->kind() != AST::FuncDef().kind()) {
            expect(";");
        }
        return result;
    }

}

AST::AST *Parser::parseDecl() {
    auto type = parseTypeSpecifier();
    bool isStruct = typeid(*type) == typeid(AST::StructDecl);
    if (isStruct) {
        if (has(";")) {
            return type;
        }
    }
    if (typeid(*type) == typeid(AST::ForwardStructDecl)) {
        return type;
    }
    declStack.emplace_back(type);
    auto decl = makeNode<AST::DeclarationList>();
    decl->add(parseDeclarationSpecifier());
    while (has(",")) {
        consume();
        declStack.pop_back();
        declStack.emplace_back(type);
        decl->add(parseDeclarationSpecifier());
    }
    declStack.pop_back();
    if (has("{")) {
        if (decl->size() != 1) {
            error(fmt::format("{}", "unexpected '{'"));
        } else {
            auto func = convertFuncTypetoFuncDef(decl->first());
            func->add(parseBlock());
            return func;
        }
    }

    return decl;
}

AST::AST *Parser::convertFuncTypetoFuncDef(AST::AST *decl) {
    auto func = makeNode<AST::FuncDef>();
    auto functype = decl->first();
    if (functype->kind() != AST::FuncType().kind()) {
        error("function expected");
    }
    func->add(functype->first());
    func->add(decl->second());
    func->add(functype->second());
    return func;
}

AST::AST *Parser::parseTypeName() {
    auto type = parseTypeSpecifier();
    declStack.emplace_back(type);
    if (has("*")) {
        parseAbstractDeclarator();
        auto ty = declStack.back();
        declStack.pop_back();
        declStack.pop_back();
        return ty;
    } else {
        declStack.pop_back();
        return type;
    }
}

void Parser::parseAbstractDirectDeclarator() {

}

void Parser::parseAbstractDeclarator() {
    if (has("*")) {
        auto p = makeNode<AST::PointerType>();
        consume();
        while (has("*")) {
            consume();
            auto p2 = makeNode<AST::PointerType>();
            p2->add(p);
            p = p2;
        }
        auto t = declStack.back();
        auto ptr = p;
        while (true) {
            if (ptr->size() == 0) {
                ptr->add(t);
                break;
            } else {
                ptr = dynamic_cast<AST::PointerType *>(ptr->first());
            }
        }
        declStack.emplace_back(p);
    }
}


AST::AST *Parser::parseDeclarationSpecifier() {
    parseDeclarator();
    auto t = declStack.back();
    declStack.pop_back();
    auto decl = extractIdentifier(t);
    if (has("=")) {
        consume();
        decl->add(parseExpr(0));
    }
    return decl;
}

void Parser::parseDirectDeclarator() {
    parseDirectDeclarator_();
    while (hasNext() && (has("(") || has("["))) {
        if (has("["))
            parseArrayDeclarator();
        else
            parseFunctionDeclarator();
    }
}

void Parser::parseDeclarator() {
    if (has("*")) {
        auto t = declStack.back();
        declStack.pop_back();
        auto p = makeNode<AST::PointerType>();
        p->add(t);
        consume();
        while (has("*")) {
            consume();
            auto p2 = makeNode<AST::PointerType>();
            p2->add(p);
            p = p2;
        }
        declStack.emplace_back(p);

    }
    parseDirectDeclarator();

}

void Parser::parseArrayDeclarator() {
    expect("[");
    AST::ArrayType *arr;
    if (!has("]")) {
        auto size = parsePrimary();
        if (size->kind() != "Number") {
            error("integer expected in array declaration");
        }
        int i = std::stol(size->tok());
        arr = makeNode<AST::ArrayType>(i);
        if (i < 0) {
            error("none-negative integer expected in array declaration");
        }

    } else {
        arr = makeNode<AST::ArrayType>();
    }
    expect("]");
    auto t = declStack.back();
    declStack.pop_back();
    arr->add(t);
    declStack.emplace_back(arr);

}

void Parser::parseFunctionDeclarator() {

    auto func = makeNode<AST::FuncType>();
    auto t = declStack.back();
    declStack.pop_back();
    func->add(t);
    auto arg = parseFuncDefArg();
    func->add(arg);
    declStack.emplace_back(func);
}

void Parser::parseDirectDeclarator_() {
    if (has("(")) {
        consume();
        parseDeclarator();
        expect(")");
    } else if (peek().type == Token::Type::Identifier) {
        auto iden = parsePrimary();
        if (iden->kind() != "Identifier") {
            error("identifier expected in direct declarator");
        }
        declStack.emplace_back(iden);
    }
}

void Parser::error(const std::string &message) {
    if (config[quitIfError]) {
        throw ParserException(message);
    } else {
        fmt::print("{}\n", message);
    }
}

AST::AST *Parser::extractIdentifier(AST::AST *ast) {
    AST::AST *ty = declStack.back();
    if (ast->kind() == "Identifier") {
        auto decl = makeNode<AST::Declaration>();
        decl->add(ty);
        decl->add(ast);
        return decl;
    } else {
        AST::AST *walker = ast;
        while (walker->size() && (walker->first()->kind() != "Identifier")) {
            walker = walker->first();
        }
        auto iden = walker->first();
        walker->set(0, ty);
        auto decl = makeNode<AST::Declaration>();
        decl->add(ast);
        decl->add(iden);
        return decl;
    }
}

AST::AST *Parser::parseFor() {
    expect("for");
    expect("(");
    auto result = makeNode<AST::For>();
    //init
    if (canItBeSomeKindOfType(peek().tok)) {
        result->add(parseDecl());
        expect(";");
    } else {
        if (has(";")) {
            consume();
            result->add(makeNode<AST::Empty>());
        } else {
            result->add(parseExpr(0));
            expect(";");
        }
    }
    //cond
    if (has(";")) {
        result->add(makeNode<AST::Empty>());
    } else {
        result->add(parseExpr(0));

    }
    expect(";");
    //step
    if (has(")")) {
        result->add(makeNode<AST::Empty>());
    } else {

        result->add(parseExpr(0));
    }
    expect(")");
    result->add(parseBlock());
    return result;
}

AST::AST *Parser::parseParameterType() {
    auto type = parseTypeSpecifier();
    declStack.emplace_back(type);
    auto decl = makeNode<AST::DeclarationList>();
    decl->add(parseDeclarationSpecifier());

//    parseDeclarator();
//    auto t = declStack.back();
//    declStack.pop_back();
//    auto decl = extractIdentifier(t);

    declStack.pop_back();
    return decl;
}

AST::BinaryExpression *Parser::hackExpr(AST::BinaryExpression *e) {
    auto op = e->getToken().tok;
    if (op == "." || op == "->") {
        auto node = makeNode<AST::MemberAccessExpression>(e->getToken());
        node->add(e->lhs());
        node->add(e->rhs());
        return node;
    }
    if (op == "+=" || op == "-=" || op == "*=" || op == "/="
        || op == "%=" || op == "<<=" || op == ">>=") {
        auto t = e->getToken();
        op.pop_back();
        t.tok = op;
        Token t2 = t;
        t2.tok = "=";
        auto e2 = makeNode<AST::BinaryExpression>(t2);
        auto e3 = makeNode<AST::BinaryExpression>(t);
        e3->add(e->first());
        e3->add(e->second());
        e2->add(e->first());
        e2->add(e3);
        return e2;
    } else {
        //const folding
        if (e->rhs()->kind() == AST::Number().kind() && e->lhs()->kind() == AST::Number().kind()) {
            Token::Type ty;

            if (e->rhs()->getToken().type == Token::Type::Int &&
                e->lhs()->getToken().type == Token::Type::Int)
                ty = Token::Type::Int;
            else
                ty = Token::Type::Float;
            if (ty == Token::Type::Float) {
                double result;
                double a, b;
                a = ((AST::Number *) e->lhs())->getFloat();
                b = ((AST::Number *) e->rhs())->getFloat();
                auto op = e->tok();
                if (op == "+") { result = a + b; }
                else if (op == "-") { result = a - b; }
                else if (op == "*") { result = a * b; }
                else if (op == "/") { result = a / b; }
                else return e;
                return (AST::BinaryExpression *) makeNode<AST::Number>(
                        Token(ty, fmt::format("{}", result), e->getToken().line,
                              e->getToken().col));
            } else {
                int64_t result;
                int64_t a, b;
                a = ((AST::Number *) e->lhs())->getInt();
                b = ((AST::Number *) e->rhs())->getInt();
                auto op = e->tok();
                if (op == "+") { result = a + b; }
                else if (op == "-") { result = a - b; }
                else if (op == "*") { result = a * b; }
                else if (op == "/") { result = a / b; }
                else if (op == "%") { result = a % b; }
                else if (op == "|") { result = (uint64_t) a | (uint64_t) b; }
                else if (op == "&") { result = (uint64_t) a & (uint64_t) b; }
                else if (op == "^") { result = (uint64_t) a ^ (uint64_t) b; }
                else if (op == "<<") { result = (uint64_t) a << (uint64_t) b; }
                else if (op == ">>") { result = (uint64_t) a >> (uint64_t) b; }
                else return e;
                return (AST::BinaryExpression *) makeNode<AST::Number>(
                        Token(ty, fmt::format("{}", (int) result), e->getToken().line,
                              e->getToken().col));
            }
        } else
            return e;
    }
}

AST::AST *Parser::parseEnum() {
    expect("enum");
    auto e = makeNode<AST::Enum>();
    expect("{");
    int counter = 0;
    while (hasNext() && !has("}")) {
        auto expr = parseExpr(0);
        if (typeid(*expr) == typeid(AST::BinaryExpression)) {
            auto identifier = cast<AST::Identifier *>(expr->first());
            auto value = cast<AST::Number *>(expr->second());
            counter = std::stoi(value->tok());
            enums[identifier->tok()] = counter++;
        } else if (typeid(*expr) == typeid(AST::Identifier)) {
            auto identifier = cast<AST::Identifier *>(expr);
            enums[identifier->tok()] = counter++;
        }
        e->add(expr);
        if (has("}"))
            break;
        else
            expect(",");
    }
    expect("}");
    expect(";");
    return e;
}

bool Parser::canItBeSomeKindOfType(const std::string &s) {
    if (types.contains(s)) {
        return true;
    }
    if (s == "const" || s == "struct" || s == "volatile") {
        return true;
    }
    return false;
}

