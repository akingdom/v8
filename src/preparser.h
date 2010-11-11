// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_PREPARSER_H
#define V8_PREPARSER_H

#include "unicode.h"

namespace v8 {
namespace preparser {

// Preparsing checks a JavaScript program and emits preparse-data that helps
// a later parsing to be faster.
// See preparser-data.h for the data.

// The PreParser checks that the syntax follows the grammar for JavaScript,
// and collects some information about the program along the way.
// The grammar check is only performed in order to understand the program
// sufficiently to deduce some information about it, that can be used
// to speed up later parsing. Finding errors is not the goal of pre-parsing,
// rather it is to speed up properly written and correct programs.
// That means that contextual checks (like a label being declared where
// it is used) are generally omitted.

namespace i = v8::internal;

enum StatementType {
  kUnknownStatement
};

enum ExpressionType {
  kUnknownExpression,
  kIdentifierExpression,  // Used to detect labels.
  kThisExpression,
  kThisPropertyExpression
};

enum IdentifierType {
  kUnknownIdentifier
};

enum SourceElementTypes {
  kUnknownSourceElements
};


typedef int SourceElements;
typedef int Expression;
typedef int Statement;
typedef int Identifier;
typedef int Arguments;


template <typename Scanner, typename PreParserLog>
class PreParser {
 public:
  PreParser() : scope_(NULL), allow_lazy_(true) { }
  ~PreParser() { }

  // Pre-parse the program from the character stream; returns true on
  // success (even if parsing failed, the pre-parse data successfully
  // captured the syntax error), and false if a stack-overflow happened
  // during parsing.
  bool PreParseProgram(Scanner* scanner,
                       PreParserLog* log,
                       bool allow_lazy) {
    allow_lazy_ = allow_lazy;
    scanner_ = scanner;
    log_ = log;
    Scope top_scope(&scope_, kTopLevelScope);
    bool ok = true;
    ParseSourceElements(i::Token::EOS, &ok);
    bool stack_overflow = scanner_->stack_overflow();
    if (!ok && !stack_overflow) {
      ReportUnexpectedToken(scanner_->current_token());
    }
    return !stack_overflow;
  }

 private:
  enum ScopeType {
    kTopLevelScope,
    kFunctionScope
  };

  class Scope {
   public:
    Scope(Scope** variable, ScopeType type)
        : variable_(variable),
          prev_(*variable),
          type_(type),
          materialized_literal_count_(0),
          expected_properties_(0),
          with_nesting_count_(0) {
      *variable = this;
    }
    ~Scope() { *variable_ = prev_; }
    void NextMaterializedLiteralIndex() { materialized_literal_count_++; }
    void AddProperty() { expected_properties_++; }
    ScopeType type() { return type_; }
    int expected_properties() { return expected_properties_; }
    int materialized_literal_count() { return materialized_literal_count_; }
    bool IsInsideWith() { return with_nesting_count_ != 0; }
    void EnterWith() { with_nesting_count_++; }
    void LeaveWith() { with_nesting_count_--; }

   private:
    Scope** const variable_;
    Scope* const prev_;
    const ScopeType type_;
    int materialized_literal_count_;
    int expected_properties_;
    int with_nesting_count_;
  };

  // Types that allow us to recognize simple this-property assignments.
  // A simple this-property assignment is a statement on the form
  // "this.propertyName = {primitive constant or function parameter name);"
  // where propertyName isn't "__proto__".
  // The result is only relevant if the function body contains only
  // simple this-property assignments.

  // Report syntax error
  void ReportUnexpectedToken(i::Token::Value token);
  void ReportMessageAt(int start_pos,
                       int end_pos,
                       const char* type,
                       const char* name_opt) {
    log_->LogMessage(start_pos, end_pos, type, name_opt);
  }

  // All ParseXXX functions take as the last argument an *ok parameter
  // which is set to false if parsing failed; it is unchanged otherwise.
  // By making the 'exception handling' explicit, we are forced to check
  // for failure at the call sites.
  SourceElements ParseSourceElements(int end_token, bool* ok);
  Statement ParseStatement(bool* ok);
  Statement ParseFunctionDeclaration(bool* ok);
  Statement ParseNativeDeclaration(bool* ok);
  Statement ParseBlock(bool* ok);
  Statement ParseVariableStatement(bool* ok);
  Statement ParseVariableDeclarations(bool accept_IN, int* num_decl, bool* ok);
  Statement ParseExpressionOrLabelledStatement(bool* ok);
  Statement ParseIfStatement(bool* ok);
  Statement ParseContinueStatement(bool* ok);
  Statement ParseBreakStatement(bool* ok);
  Statement ParseReturnStatement(bool* ok);
  Statement ParseWithStatement(bool* ok);
  Statement ParseSwitchStatement(bool* ok);
  Statement ParseDoWhileStatement(bool* ok);
  Statement ParseWhileStatement(bool* ok);
  Statement ParseForStatement(bool* ok);
  Statement ParseThrowStatement(bool* ok);
  Statement ParseTryStatement(bool* ok);
  Statement ParseDebuggerStatement(bool* ok);

  Expression ParseExpression(bool accept_IN, bool* ok);
  Expression ParseAssignmentExpression(bool accept_IN, bool* ok);
  Expression ParseConditionalExpression(bool accept_IN, bool* ok);
  Expression ParseBinaryExpression(int prec, bool accept_IN, bool* ok);
  Expression ParseUnaryExpression(bool* ok);
  Expression ParsePostfixExpression(bool* ok);
  Expression ParseLeftHandSideExpression(bool* ok);
  Expression ParseNewExpression(bool* ok);
  Expression ParseMemberExpression(bool* ok);
  Expression ParseMemberWithNewPrefixesExpression(unsigned new_count, bool* ok);
  Expression ParsePrimaryExpression(bool* ok);
  Expression ParseArrayLiteral(bool* ok);
  Expression ParseObjectLiteral(bool* ok);
  Expression ParseRegExpLiteral(bool seen_equal, bool* ok);
  Expression ParseV8Intrinsic(bool* ok);

  Arguments ParseArguments(bool* ok);
  Expression ParseFunctionLiteral(bool* ok);

  Identifier ParseIdentifier(bool* ok);
  Identifier ParseIdentifierName(bool* ok);
  Identifier ParseIdentifierOrGetOrSet(bool* is_get, bool* is_set, bool* ok);

  Identifier GetIdentifierSymbol();
  unsigned int HexDigitValue(char digit);
  Expression GetStringSymbol();


  i::Token::Value peek() { return scanner_->peek(); }
  i::Token::Value Next() {
    i::Token::Value next = scanner_->Next();
    return next;
  }

  void Consume(i::Token::Value token) {
    Next();
  }

  void Expect(i::Token::Value token, bool* ok) {
    if (Next() != token) {
      *ok = false;
    }
  }

  bool Check(i::Token::Value token) {
    i::Token::Value next = peek();
    if (next == token) {
      Consume(next);
      return true;
    }
    return false;
  }
  void ExpectSemicolon(bool* ok);

  static int Precedence(i::Token::Value tok, bool accept_IN);

  Scanner* scanner_;
  PreParserLog* log_;
  Scope* scope_;
  bool allow_lazy_;
};


#define CHECK_OK  ok);  \
  if (!*ok) return -1;  \
  ((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY


template <typename Scanner, typename Log>
void PreParser<Scanner, Log>::ReportUnexpectedToken(i::Token::Value token) {
  // We don't report stack overflows here, to avoid increasing the
  // stack depth even further.  Instead we report it after parsing is
  // over, in ParseProgram.
  if (token == i::Token::ILLEGAL && scanner_->stack_overflow()) {
    return;
  }
  typename Scanner::Location source_location = scanner_->location();

  // Four of the tokens are treated specially
  switch (token) {
  case i::Token::EOS:
    return ReportMessageAt(source_location.beg_pos, source_location.end_pos,
                           "unexpected_eos", NULL);
  case i::Token::NUMBER:
    return ReportMessageAt(source_location.beg_pos, source_location.end_pos,
                           "unexpected_token_number", NULL);
  case i::Token::STRING:
    return ReportMessageAt(source_location.beg_pos, source_location.end_pos,
                           "unexpected_token_string", NULL);
  case i::Token::IDENTIFIER:
    return ReportMessageAt(source_location.beg_pos, source_location.end_pos,
                           "unexpected_token_identifier", NULL);
  default:
    const char* name = i::Token::String(token);
    ReportMessageAt(source_location.beg_pos, source_location.end_pos,
                    "unexpected_token", name);
  }
}


template <typename Scanner, typename Log>
SourceElements PreParser<Scanner, Log>::ParseSourceElements(int end_token,
                                                            bool* ok) {
  // SourceElements ::
  //   (Statement)* <end_token>

  while (peek() != end_token) {
    ParseStatement(CHECK_OK);
  }
  return kUnknownSourceElements;
}


template <typename Scanner, typename Log>
Statement PreParser<Scanner, Log>::ParseStatement(bool* ok) {
  // Statement ::
  //   Block
  //   VariableStatement
  //   EmptyStatement
  //   ExpressionStatement
  //   IfStatement
  //   IterationStatement
  //   ContinueStatement
  //   BreakStatement
  //   ReturnStatement
  //   WithStatement
  //   LabelledStatement
  //   SwitchStatement
  //   ThrowStatement
  //   TryStatement
  //   DebuggerStatement

  // Note: Since labels can only be used by 'break' and 'continue'
  // statements, which themselves are only valid within blocks,
  // iterations or 'switch' statements (i.e., BreakableStatements),
  // labels can be simply ignored in all other cases; except for
  // trivial labeled break statements 'label: break label' which is
  // parsed into an empty statement.

  // Keep the source position of the statement
  switch (peek()) {
    case i::Token::LBRACE:
      return ParseBlock(ok);

    case i::Token::CONST:
    case i::Token::VAR:
      return ParseVariableStatement(ok);

    case i::Token::SEMICOLON:
      Next();
      return kUnknownStatement;

    case i::Token::IF:
      return  ParseIfStatement(ok);

    case i::Token::DO:
      return ParseDoWhileStatement(ok);

    case i::Token::WHILE:
      return ParseWhileStatement(ok);

    case i::Token::FOR:
      return ParseForStatement(ok);

    case i::Token::CONTINUE:
      return ParseContinueStatement(ok);

    case i::Token::BREAK:
      return ParseBreakStatement(ok);

    case i::Token::RETURN:
      return ParseReturnStatement(ok);

    case i::Token::WITH:
      return ParseWithStatement(ok);

    case i::Token::SWITCH:
      return ParseSwitchStatement(ok);

    case i::Token::THROW:
      return ParseThrowStatement(ok);

    case i::Token::TRY:
      return ParseTryStatement(ok);

    case i::Token::FUNCTION:
      return ParseFunctionDeclaration(ok);

    case i::Token::NATIVE:
      return ParseNativeDeclaration(ok);

    case i::Token::DEBUGGER:
      return ParseDebuggerStatement(ok);

    default:
      return ParseExpressionOrLabelledStatement(ok);
  }
}


template <typename Scanner, typename Log>
Statement PreParser<Scanner, Log>::ParseFunctionDeclaration(bool* ok) {
  // FunctionDeclaration ::
  //   'function' Identifier '(' FormalParameterListopt ')' '{' FunctionBody '}'
  Expect(i::Token::FUNCTION, CHECK_OK);
  ParseIdentifier(CHECK_OK);
  ParseFunctionLiteral(CHECK_OK);
  return kUnknownStatement;
}


// Language extension which is only enabled for source files loaded
// through the API's extension mechanism.  A native function
// declaration is resolved by looking up the function through a
// callback provided by the extension.
template <typename Scanner, typename Log>
Statement PreParser<Scanner, Log>::ParseNativeDeclaration(bool* ok) {
  Expect(i::Token::NATIVE, CHECK_OK);
  Expect(i::Token::FUNCTION, CHECK_OK);
  ParseIdentifier(CHECK_OK);
  Expect(i::Token::LPAREN, CHECK_OK);
  bool done = (peek() == i::Token::RPAREN);
  while (!done) {
    ParseIdentifier(CHECK_OK);
    done = (peek() == i::Token::RPAREN);
    if (!done) {
      Expect(i::Token::COMMA, CHECK_OK);
    }
  }
  Expect(i::Token::RPAREN, CHECK_OK);
  Expect(i::Token::SEMICOLON, CHECK_OK);
  return kUnknownStatement;
}


template <typename Scanner, typename Log>
Statement PreParser<Scanner, Log>::ParseBlock(bool* ok) {
  // Block ::
  //   '{' Statement* '}'

  // Note that a Block does not introduce a new execution scope!
  // (ECMA-262, 3rd, 12.2)
  //
  Expect(i::Token::LBRACE, CHECK_OK);
  while (peek() != i::Token::RBRACE) {
    ParseStatement(CHECK_OK);
  }
  Expect(i::Token::RBRACE, CHECK_OK);
  return kUnknownStatement;
}


template <typename Scanner, typename Log>
Statement PreParser<Scanner, Log>::ParseVariableStatement(bool* ok) {
  // VariableStatement ::
  //   VariableDeclarations ';'

  Statement result = ParseVariableDeclarations(true, NULL, CHECK_OK);
  ExpectSemicolon(CHECK_OK);
  return result;
}


// If the variable declaration declares exactly one non-const
// variable, then *var is set to that variable. In all other cases,
// *var is untouched; in particular, it is the caller's responsibility
// to initialize it properly. This mechanism is also used for the parsing
// of 'for-in' loops.
template <typename Scanner, typename Log>
Statement PreParser<Scanner, Log>::ParseVariableDeclarations(bool accept_IN,
                                                  int* num_decl,
                                                  bool* ok) {
  // VariableDeclarations ::
  //   ('var' | 'const') (Identifier ('=' AssignmentExpression)?)+[',']

  if (peek() == i::Token::VAR) {
    Consume(i::Token::VAR);
  } else if (peek() == i::Token::CONST) {
    Consume(i::Token::CONST);
  } else {
    *ok = false;
    return 0;
  }

  // The scope of a variable/const declared anywhere inside a function
  // is the entire function (ECMA-262, 3rd, 10.1.3, and 12.2). .
  int nvars = 0;  // the number of variables declared
  do {
    // Parse variable name.
    if (nvars > 0) Consume(i::Token::COMMA);
    ParseIdentifier(CHECK_OK);
    nvars++;
    if (peek() == i::Token::ASSIGN) {
      Expect(i::Token::ASSIGN, CHECK_OK);
      ParseAssignmentExpression(accept_IN, CHECK_OK);
    }
  } while (peek() == i::Token::COMMA);

  if (num_decl != NULL) *num_decl = nvars;
  return kUnknownStatement;
}


template <typename Scanner, typename Log>
Statement PreParser<Scanner, Log>::ParseExpressionOrLabelledStatement(
    bool* ok) {
  // ExpressionStatement | LabelledStatement ::
  //   Expression ';'
  //   Identifier ':' Statement

  Expression expr = ParseExpression(true, CHECK_OK);
  if (peek() == i::Token::COLON && expr == kIdentifierExpression) {
    Consume(i::Token::COLON);
    return ParseStatement(ok);
  }
  // Parsed expression statement.
  ExpectSemicolon(CHECK_OK);
  return kUnknownStatement;
}


template <typename Scanner, typename Log>
Statement PreParser<Scanner, Log>::ParseIfStatement(bool* ok) {
  // IfStatement ::
  //   'if' '(' Expression ')' Statement ('else' Statement)?

  Expect(i::Token::IF, CHECK_OK);
  Expect(i::Token::LPAREN, CHECK_OK);
  ParseExpression(true, CHECK_OK);
  Expect(i::Token::RPAREN, CHECK_OK);
  ParseStatement(CHECK_OK);
  if (peek() == i::Token::ELSE) {
    Next();
    ParseStatement(CHECK_OK);
  }
  return kUnknownStatement;
}


template <typename Scanner, typename Log>
Statement PreParser<Scanner, Log>::ParseContinueStatement(bool* ok) {
  // ContinueStatement ::
  //   'continue' [no line terminator] Identifier? ';'

  Expect(i::Token::CONTINUE, CHECK_OK);
  i::Token::Value tok = peek();
  if (!scanner_->has_line_terminator_before_next() &&
      tok != i::Token::SEMICOLON &&
      tok != i::Token::RBRACE &&
      tok != i::Token::EOS) {
    ParseIdentifier(CHECK_OK);
  }
  ExpectSemicolon(CHECK_OK);
  return kUnknownStatement;
}


template <typename Scanner, typename Log>
Statement PreParser<Scanner, Log>::ParseBreakStatement(bool* ok) {
  // BreakStatement ::
  //   'break' [no line terminator] Identifier? ';'

  Expect(i::Token::BREAK, CHECK_OK);
  i::Token::Value tok = peek();
  if (!scanner_->has_line_terminator_before_next() &&
      tok != i::Token::SEMICOLON &&
      tok != i::Token::RBRACE &&
      tok != i::Token::EOS) {
    ParseIdentifier(CHECK_OK);
  }
  ExpectSemicolon(CHECK_OK);
  return kUnknownStatement;
}


template <typename Scanner, typename Log>
Statement PreParser<Scanner, Log>::ParseReturnStatement(bool* ok) {
  // ReturnStatement ::
  //   'return' [no line terminator] Expression? ';'

  // Consume the return token. It is necessary to do the before
  // reporting any errors on it, because of the way errors are
  // reported (underlining).
  Expect(i::Token::RETURN, CHECK_OK);

  // An ECMAScript program is considered syntactically incorrect if it
  // contains a return statement that is not within the body of a
  // function. See ECMA-262, section 12.9, page 67.
  // This is not handled during preparsing.

  i::Token::Value tok = peek();
  if (!scanner_->has_line_terminator_before_next() &&
      tok != i::Token::SEMICOLON &&
      tok != i::Token::RBRACE &&
      tok != i::Token::EOS) {
    ParseExpression(true, CHECK_OK);
  }
  ExpectSemicolon(CHECK_OK);
  return kUnknownStatement;
}


template <typename Scanner, typename Log>
Statement PreParser<Scanner, Log>::ParseWithStatement(bool* ok) {
  // WithStatement ::
  //   'with' '(' Expression ')' Statement
  Expect(i::Token::WITH, CHECK_OK);
  Expect(i::Token::LPAREN, CHECK_OK);
  ParseExpression(true, CHECK_OK);
  Expect(i::Token::RPAREN, CHECK_OK);

  scope_->EnterWith();
  ParseStatement(CHECK_OK);
  scope_->LeaveWith();
  return kUnknownStatement;
}


template <typename Scanner, typename Log>
Statement PreParser<Scanner, Log>::ParseSwitchStatement(bool* ok) {
  // SwitchStatement ::
  //   'switch' '(' Expression ')' '{' CaseClause* '}'

  Expect(i::Token::SWITCH, CHECK_OK);
  Expect(i::Token::LPAREN, CHECK_OK);
  ParseExpression(true, CHECK_OK);
  Expect(i::Token::RPAREN, CHECK_OK);

  Expect(i::Token::LBRACE, CHECK_OK);
  i::Token::Value token = peek();
  while (token != i::Token::RBRACE) {
    if (token == i::Token::CASE) {
      Expect(i::Token::CASE, CHECK_OK);
      ParseExpression(true, CHECK_OK);
      Expect(i::Token::COLON, CHECK_OK);
    } else if (token == i::Token::DEFAULT) {
      Expect(i::Token::DEFAULT, CHECK_OK);
      Expect(i::Token::COLON, CHECK_OK);
    } else {
      ParseStatement(CHECK_OK);
    }
    token = peek();
  }
  Expect(i::Token::RBRACE, CHECK_OK);

  return kUnknownStatement;
}


template <typename Scanner, typename Log>
Statement PreParser<Scanner, Log>::ParseDoWhileStatement(bool* ok) {
  // DoStatement ::
  //   'do' Statement 'while' '(' Expression ')' ';'

  Expect(i::Token::DO, CHECK_OK);
  ParseStatement(CHECK_OK);
  Expect(i::Token::WHILE, CHECK_OK);
  Expect(i::Token::LPAREN, CHECK_OK);
  ParseExpression(true, CHECK_OK);
  Expect(i::Token::RPAREN, CHECK_OK);
  return kUnknownStatement;
}


template <typename Scanner, typename Log>
Statement PreParser<Scanner, Log>::ParseWhileStatement(bool* ok) {
  // WhileStatement ::
  //   'while' '(' Expression ')' Statement

  Expect(i::Token::WHILE, CHECK_OK);
  Expect(i::Token::LPAREN, CHECK_OK);
  ParseExpression(true, CHECK_OK);
  Expect(i::Token::RPAREN, CHECK_OK);
  ParseStatement(CHECK_OK);
  return kUnknownStatement;
}


template <typename Scanner, typename Log>
Statement PreParser<Scanner, Log>::ParseForStatement(bool* ok) {
  // ForStatement ::
  //   'for' '(' Expression? ';' Expression? ';' Expression? ')' Statement

  Expect(i::Token::FOR, CHECK_OK);
  Expect(i::Token::LPAREN, CHECK_OK);
  if (peek() != i::Token::SEMICOLON) {
    if (peek() == i::Token::VAR || peek() == i::Token::CONST) {
      int decl_count;
      ParseVariableDeclarations(false, &decl_count, CHECK_OK);
      if (peek() == i::Token::IN && decl_count == 1) {
        Expect(i::Token::IN, CHECK_OK);
        ParseExpression(true, CHECK_OK);
        Expect(i::Token::RPAREN, CHECK_OK);

        ParseStatement(CHECK_OK);
        return kUnknownStatement;
      }
    } else {
      ParseExpression(false, CHECK_OK);
      if (peek() == i::Token::IN) {
        Expect(i::Token::IN, CHECK_OK);
        ParseExpression(true, CHECK_OK);
        Expect(i::Token::RPAREN, CHECK_OK);

        ParseStatement(CHECK_OK);
        return kUnknownStatement;
      }
    }
  }

  // Parsed initializer at this point.
  Expect(i::Token::SEMICOLON, CHECK_OK);

  if (peek() != i::Token::SEMICOLON) {
    ParseExpression(true, CHECK_OK);
  }
  Expect(i::Token::SEMICOLON, CHECK_OK);

  if (peek() != i::Token::RPAREN) {
    ParseExpression(true, CHECK_OK);
  }
  Expect(i::Token::RPAREN, CHECK_OK);

  ParseStatement(CHECK_OK);
  return kUnknownStatement;
}


template <typename Scanner, typename Log>
Statement PreParser<Scanner, Log>::ParseThrowStatement(bool* ok) {
  // ThrowStatement ::
  //   'throw' [no line terminator] Expression ';'

  Expect(i::Token::THROW, CHECK_OK);
  if (scanner_->has_line_terminator_before_next()) {
    typename Scanner::Location pos = scanner_->location();
    ReportMessageAt(pos.beg_pos, pos.end_pos,
                    "newline_after_throw", NULL);
    *ok = false;
    return NULL;
  }
  ParseExpression(true, CHECK_OK);
  ExpectSemicolon(CHECK_OK);

  return kUnknownStatement;
}


template <typename Scanner, typename Log>
Statement PreParser<Scanner, Log>::ParseTryStatement(bool* ok) {
  // TryStatement ::
  //   'try' Block Catch
  //   'try' Block Finally
  //   'try' Block Catch Finally
  //
  // Catch ::
  //   'catch' '(' Identifier ')' Block
  //
  // Finally ::
  //   'finally' Block

  // In preparsing, allow any number of catch/finally blocks, including zero
  // of both.

  Expect(i::Token::TRY, CHECK_OK);

  ParseBlock(CHECK_OK);

  bool catch_or_finally_seen = false;
  if (peek() == i::Token::CATCH) {
    Consume(i::Token::CATCH);
    Expect(i::Token::LPAREN, CHECK_OK);
    ParseIdentifier(CHECK_OK);
    Expect(i::Token::RPAREN, CHECK_OK);
    scope_->EnterWith();
    ParseBlock(ok);
    scope_->LeaveWith();
    if (!*ok) return kUnknownStatement;
    catch_or_finally_seen = true;
  }
  if (peek() == i::Token::FINALLY) {
    Consume(i::Token::FINALLY);
    ParseBlock(CHECK_OK);
    catch_or_finally_seen = true;
  }
  if (!catch_or_finally_seen) {
    *ok = false;
  }
  return kUnknownStatement;
}


template <typename Scanner, typename Log>
Statement PreParser<Scanner, Log>::ParseDebuggerStatement(bool* ok) {
  // In ECMA-262 'debugger' is defined as a reserved keyword. In some browser
  // contexts this is used as a statement which invokes the debugger as if a
  // break point is present.
  // DebuggerStatement ::
  //   'debugger' ';'

  Expect(i::Token::DEBUGGER, CHECK_OK);
  ExpectSemicolon(CHECK_OK);
  return kUnknownStatement;
}


// Precedence = 1
template <typename Scanner, typename Log>
Expression PreParser<Scanner, Log>::ParseExpression(bool accept_IN, bool* ok) {
  // Expression ::
  //   AssignmentExpression
  //   Expression ',' AssignmentExpression

  Expression result = ParseAssignmentExpression(accept_IN, CHECK_OK);
  while (peek() == i::Token::COMMA) {
    Expect(i::Token::COMMA, CHECK_OK);
    ParseAssignmentExpression(accept_IN, CHECK_OK);
    result = kUnknownExpression;
  }
  return result;
}


// Precedence = 2
template <typename Scanner, typename Log>
Expression PreParser<Scanner, Log>::ParseAssignmentExpression(bool accept_IN,
                                                              bool* ok) {
  // AssignmentExpression ::
  //   ConditionalExpression
  //   LeftHandSideExpression AssignmentOperator AssignmentExpression

  Expression expression = ParseConditionalExpression(accept_IN, CHECK_OK);

  if (!i::Token::IsAssignmentOp(peek())) {
    // Parsed conditional expression only (no assignment).
    return expression;
  }

  i::Token::Value op = Next();  // Get assignment operator.
  ParseAssignmentExpression(accept_IN, CHECK_OK);

  if ((op == i::Token::ASSIGN) && (expression == kThisPropertyExpression)) {
    scope_->AddProperty();
  }

  return kUnknownExpression;
}


// Precedence = 3
template <typename Scanner, typename Log>
Expression PreParser<Scanner, Log>::ParseConditionalExpression(bool accept_IN,
                                                               bool* ok) {
  // ConditionalExpression ::
  //   LogicalOrExpression
  //   LogicalOrExpression '?' AssignmentExpression ':' AssignmentExpression

  // We start using the binary expression parser for prec >= 4 only!
  Expression expression = ParseBinaryExpression(4, accept_IN, CHECK_OK);
  if (peek() != i::Token::CONDITIONAL) return expression;
  Consume(i::Token::CONDITIONAL);
  // In parsing the first assignment expression in conditional
  // expressions we always accept the 'in' keyword; see ECMA-262,
  // section 11.12, page 58.
  ParseAssignmentExpression(true, CHECK_OK);
  Expect(i::Token::COLON, CHECK_OK);
  ParseAssignmentExpression(accept_IN, CHECK_OK);
  return kUnknownExpression;
}


template <typename Scanner, typename Log>
int PreParser<Scanner, Log>::Precedence(i::Token::Value tok, bool accept_IN) {
  if (tok == i::Token::IN && !accept_IN)
    return 0;  // 0 precedence will terminate binary expression parsing

  return i::Token::Precedence(tok);
}


// Precedence >= 4
template <typename Scanner, typename Log>
Expression PreParser<Scanner, Log>::ParseBinaryExpression(int prec,
                                                          bool accept_IN,
                                                          bool* ok) {
  Expression result = ParseUnaryExpression(CHECK_OK);
  for (int prec1 = Precedence(peek(), accept_IN); prec1 >= prec; prec1--) {
    // prec1 >= 4
    while (Precedence(peek(), accept_IN) == prec1) {
      Next();
      ParseBinaryExpression(prec1 + 1, accept_IN, CHECK_OK);
      result = kUnknownExpression;
    }
  }
  return result;
}


template <typename Scanner, typename Log>
Expression PreParser<Scanner, Log>::ParseUnaryExpression(bool* ok) {
  // UnaryExpression ::
  //   PostfixExpression
  //   'delete' UnaryExpression
  //   'void' UnaryExpression
  //   'typeof' UnaryExpression
  //   '++' UnaryExpression
  //   '--' UnaryExpression
  //   '+' UnaryExpression
  //   '-' UnaryExpression
  //   '~' UnaryExpression
  //   '!' UnaryExpression

  i::Token::Value op = peek();
  if (i::Token::IsUnaryOp(op) || i::Token::IsCountOp(op)) {
    op = Next();
    ParseUnaryExpression(ok);
    return kUnknownExpression;
  } else {
    return ParsePostfixExpression(ok);
  }
}


template <typename Scanner, typename Log>
Expression PreParser<Scanner, Log>::ParsePostfixExpression(bool* ok) {
  // PostfixExpression ::
  //   LeftHandSideExpression ('++' | '--')?

  Expression expression = ParseLeftHandSideExpression(CHECK_OK);
  if (!scanner_->has_line_terminator_before_next() &&
      i::Token::IsCountOp(peek())) {
    Next();
    return kUnknownExpression;
  }
  return expression;
}


template <typename Scanner, typename Log>
Expression PreParser<Scanner, Log>::ParseLeftHandSideExpression(bool* ok) {
  // LeftHandSideExpression ::
  //   (NewExpression | MemberExpression) ...

  Expression result;
  if (peek() == i::Token::NEW) {
    result = ParseNewExpression(CHECK_OK);
  } else {
    result = ParseMemberExpression(CHECK_OK);
  }

  while (true) {
    switch (peek()) {
      case i::Token::LBRACK: {
        Consume(i::Token::LBRACK);
        ParseExpression(true, CHECK_OK);
        Expect(i::Token::RBRACK, CHECK_OK);
        if (result == kThisExpression) {
          result = kThisPropertyExpression;
        } else {
          result = kUnknownExpression;
        }
        break;
      }

      case i::Token::LPAREN: {
        ParseArguments(CHECK_OK);
        result = kUnknownExpression;
        break;
      }

      case i::Token::PERIOD: {
        Consume(i::Token::PERIOD);
        ParseIdentifierName(CHECK_OK);
        if (result == kThisExpression) {
          result = kThisPropertyExpression;
        } else {
          result = kUnknownExpression;
        }
        break;
      }

      default:
        return result;
    }
  }
}


template <typename Scanner, typename Log>
Expression PreParser<Scanner, Log>::ParseNewExpression(bool* ok) {
  // NewExpression ::
  //   ('new')+ MemberExpression

  // The grammar for new expressions is pretty warped. The keyword
  // 'new' can either be a part of the new expression (where it isn't
  // followed by an argument list) or a part of the member expression,
  // where it must be followed by an argument list. To accommodate
  // this, we parse the 'new' keywords greedily and keep track of how
  // many we have parsed. This information is then passed on to the
  // member expression parser, which is only allowed to match argument
  // lists as long as it has 'new' prefixes left
  unsigned new_count = 0;
  do {
    Consume(i::Token::NEW);
    new_count++;
  } while (peek() == i::Token::NEW);

  return ParseMemberWithNewPrefixesExpression(new_count, ok);
}


template <typename Scanner, typename Log>
Expression PreParser<Scanner, Log>::ParseMemberExpression(bool* ok) {
  return ParseMemberWithNewPrefixesExpression(0, ok);
}


template <typename Scanner, typename Log>
Expression PreParser<Scanner, Log>::ParseMemberWithNewPrefixesExpression(
    unsigned new_count, bool* ok) {
  // MemberExpression ::
  //   (PrimaryExpression | FunctionLiteral)
  //     ('[' Expression ']' | '.' Identifier | Arguments)*

  // Parse the initial primary or function expression.
  Expression result = NULL;
  if (peek() == i::Token::FUNCTION) {
    Consume(i::Token::FUNCTION);
    if (peek() == i::Token::IDENTIFIER) {
      ParseIdentifier(CHECK_OK);
    }
    result = ParseFunctionLiteral(CHECK_OK);
  } else {
    result = ParsePrimaryExpression(CHECK_OK);
  }

  while (true) {
    switch (peek()) {
      case i::Token::LBRACK: {
        Consume(i::Token::LBRACK);
        ParseExpression(true, CHECK_OK);
        Expect(i::Token::RBRACK, CHECK_OK);
        if (result == kThisExpression) {
          result = kThisPropertyExpression;
        } else {
          result = kUnknownExpression;
        }
        break;
      }
      case i::Token::PERIOD: {
        Consume(i::Token::PERIOD);
        ParseIdentifierName(CHECK_OK);
        if (result == kThisExpression) {
          result = kThisPropertyExpression;
        } else {
          result = kUnknownExpression;
        }
        break;
      }
      case i::Token::LPAREN: {
        if (new_count == 0) return result;
        // Consume one of the new prefixes (already parsed).
        ParseArguments(CHECK_OK);
        new_count--;
        result = kUnknownExpression;
        break;
      }
      default:
        return result;
    }
  }
}


template <typename Scanner, typename Log>
Expression PreParser<Scanner, Log>::ParsePrimaryExpression(bool* ok) {
  // PrimaryExpression ::
  //   'this'
  //   'null'
  //   'true'
  //   'false'
  //   Identifier
  //   Number
  //   String
  //   ArrayLiteral
  //   ObjectLiteral
  //   RegExpLiteral
  //   '(' Expression ')'

  Expression result = kUnknownExpression;
  switch (peek()) {
    case i::Token::THIS: {
      Next();
      result = kThisExpression;
      break;
    }

    case i::Token::IDENTIFIER: {
      ParseIdentifier(CHECK_OK);
      result = kIdentifierExpression;
      break;
    }

    case i::Token::NULL_LITERAL:
    case i::Token::TRUE_LITERAL:
    case i::Token::FALSE_LITERAL:
    case i::Token::NUMBER: {
      Next();
      break;
    }
    case i::Token::STRING: {
      Next();
      result = GetStringSymbol();
      break;
    }

    case i::Token::ASSIGN_DIV:
      result = ParseRegExpLiteral(true, CHECK_OK);
      break;

    case i::Token::DIV:
      result = ParseRegExpLiteral(false, CHECK_OK);
      break;

    case i::Token::LBRACK:
      result = ParseArrayLiteral(CHECK_OK);
      break;

    case i::Token::LBRACE:
      result = ParseObjectLiteral(CHECK_OK);
      break;

    case i::Token::LPAREN:
      Consume(i::Token::LPAREN);
      result = ParseExpression(true, CHECK_OK);
      Expect(i::Token::RPAREN, CHECK_OK);
      if (result == kIdentifierExpression) result = kUnknownExpression;
      break;

    case i::Token::MOD:
      result = ParseV8Intrinsic(CHECK_OK);
      break;

    default: {
      Next();
      *ok = false;
      return kUnknownExpression;
    }
  }

  return result;
}


template <typename Scanner, typename Log>
Expression PreParser<Scanner, Log>::ParseArrayLiteral(bool* ok) {
  // ArrayLiteral ::
  //   '[' Expression? (',' Expression?)* ']'
  Expect(i::Token::LBRACK, CHECK_OK);
  while (peek() != i::Token::RBRACK) {
    if (peek() != i::Token::COMMA) {
      ParseAssignmentExpression(true, CHECK_OK);
    }
    if (peek() != i::Token::RBRACK) {
      Expect(i::Token::COMMA, CHECK_OK);
    }
  }
  Expect(i::Token::RBRACK, CHECK_OK);

  scope_->NextMaterializedLiteralIndex();
  return kUnknownExpression;
}


template <typename Scanner, typename Log>
Expression PreParser<Scanner, Log>::ParseObjectLiteral(bool* ok) {
  // ObjectLiteral ::
  //   '{' (
  //       ((IdentifierName | String | Number) ':' AssignmentExpression)
  //     | (('get' | 'set') (IdentifierName | String | Number) FunctionLiteral)
  //    )*[','] '}'

  Expect(i::Token::LBRACE, CHECK_OK);
  while (peek() != i::Token::RBRACE) {
    i::Token::Value next = peek();
    switch (next) {
      case i::Token::IDENTIFIER: {
        bool is_getter = false;
        bool is_setter = false;
        ParseIdentifierOrGetOrSet(&is_getter, &is_setter, CHECK_OK);
        if ((is_getter || is_setter) && peek() != i::Token::COLON) {
            i::Token::Value name = Next();
            if (name != i::Token::IDENTIFIER &&
                name != i::Token::NUMBER &&
                name != i::Token::STRING &&
                !i::Token::IsKeyword(name)) {
              *ok = false;
              return kUnknownExpression;
            }
            ParseFunctionLiteral(CHECK_OK);
            if (peek() != i::Token::RBRACE) {
              Expect(i::Token::COMMA, CHECK_OK);
            }
            continue;  // restart the while
        }
        break;
      }
      case i::Token::STRING:
        Consume(next);
        GetStringSymbol();
        break;
      case i::Token::NUMBER:
        Consume(next);
        break;
      default:
        if (i::Token::IsKeyword(next)) {
          Consume(next);
        } else {
          // Unexpected token.
          *ok = false;
          return kUnknownExpression;
        }
    }

    Expect(i::Token::COLON, CHECK_OK);
    ParseAssignmentExpression(true, CHECK_OK);

    // TODO(1240767): Consider allowing trailing comma.
    if (peek() != i::Token::RBRACE) Expect(i::Token::COMMA, CHECK_OK);
  }
  Expect(i::Token::RBRACE, CHECK_OK);

  scope_->NextMaterializedLiteralIndex();
  return kUnknownExpression;
}


template <typename Scanner, typename Log>
Expression PreParser<Scanner, Log>::ParseRegExpLiteral(bool seen_equal,
                                                       bool* ok) {
  if (!scanner_->ScanRegExpPattern(seen_equal)) {
    Next();
    typename Scanner::Location location = scanner_->location();
    ReportMessageAt(location.beg_pos, location.end_pos,
                    "unterminated_regexp", NULL);
    *ok = false;
    return kUnknownExpression;
  }

  scope_->NextMaterializedLiteralIndex();

  if (!scanner_->ScanRegExpFlags()) {
    Next();
    typename Scanner::Location location = scanner_->location();
    ReportMessageAt(location.beg_pos, location.end_pos,
                    "invalid_regexp_flags", NULL);
    *ok = false;
    return kUnknownExpression;
  }
  Next();
  return kUnknownExpression;
}


template <typename Scanner, typename Log>
Arguments PreParser<Scanner, Log>::ParseArguments(bool* ok) {
  // Arguments ::
  //   '(' (AssignmentExpression)*[','] ')'

  Expect(i::Token::LPAREN, CHECK_OK);
  bool done = (peek() == i::Token::RPAREN);
  int argc = 0;
  while (!done) {
    ParseAssignmentExpression(true, CHECK_OK);
    argc++;
    done = (peek() == i::Token::RPAREN);
    if (!done) Expect(i::Token::COMMA, CHECK_OK);
  }
  Expect(i::Token::RPAREN, CHECK_OK);
  return argc;
}


template <typename Scanner, typename Log>
Expression PreParser<Scanner, Log>::ParseFunctionLiteral(bool* ok) {
  // Function ::
  //   '(' FormalParameterList? ')' '{' FunctionBody '}'

  // Parse function body.
  ScopeType outer_scope_type = scope_->type();
  bool inside_with = scope_->IsInsideWith();
  Scope function_scope(&scope_, kFunctionScope);

  //  FormalParameterList ::
  //    '(' (Identifier)*[','] ')'
  Expect(i::Token::LPAREN, CHECK_OK);
  bool done = (peek() == i::Token::RPAREN);
  while (!done) {
    ParseIdentifier(CHECK_OK);
    done = (peek() == i::Token::RPAREN);
    if (!done) {
      Expect(i::Token::COMMA, CHECK_OK);
    }
  }
  Expect(i::Token::RPAREN, CHECK_OK);

  Expect(i::Token::LBRACE, CHECK_OK);
  int function_block_pos = scanner_->location().beg_pos;

  // Determine if the function will be lazily compiled.
  // Currently only happens to top-level functions.
  // Optimistically assume that all top-level functions are lazily compiled.
  bool is_lazily_compiled =
      (outer_scope_type == kTopLevelScope && !inside_with && allow_lazy_);

  if (is_lazily_compiled) {
    log_->PauseRecording();
    ParseSourceElements(i::Token::RBRACE, ok);
    log_->ResumeRecording();
    if (!*ok) return kUnknownExpression;

    Expect(i::Token::RBRACE, CHECK_OK);

    int end_pos = scanner_->location().end_pos;
    log_->LogFunction(function_block_pos, end_pos,
                      function_scope.materialized_literal_count(),
                      function_scope.expected_properties());
  } else {
    ParseSourceElements(i::Token::RBRACE, CHECK_OK);
    Expect(i::Token::RBRACE, CHECK_OK);
  }
  return kUnknownExpression;
}


template <typename Scanner, typename Log>
Expression PreParser<Scanner, Log>::ParseV8Intrinsic(bool* ok) {
  // CallRuntime ::
  //   '%' Identifier Arguments

  Expect(i::Token::MOD, CHECK_OK);
  ParseIdentifier(CHECK_OK);
  ParseArguments(CHECK_OK);

  return kUnknownExpression;
}


template <typename Scanner, typename Log>
void PreParser<Scanner, Log>::ExpectSemicolon(bool* ok) {
  // Check for automatic semicolon insertion according to
  // the rules given in ECMA-262, section 7.9, page 21.
  i::Token::Value tok = peek();
  if (tok == i::Token::SEMICOLON) {
    Next();
    return;
  }
  if (scanner_->has_line_terminator_before_next() ||
      tok == i::Token::RBRACE ||
      tok == i::Token::EOS) {
    return;
  }
  Expect(i::Token::SEMICOLON, ok);
}


template <typename Scanner, typename Log>
Identifier PreParser<Scanner, Log>::GetIdentifierSymbol() {
  const char* literal_chars = scanner_->literal_string();
  int literal_length = scanner_->literal_length();
  int identifier_pos = scanner_->location().beg_pos;

  log_->LogSymbol(identifier_pos, literal_chars, literal_length);

  return kUnknownExpression;
}


template <typename Scanner, typename Log>
Expression PreParser<Scanner, Log>::GetStringSymbol() {
  const char* literal_chars = scanner_->literal_string();
  int literal_length = scanner_->literal_length();

  int literal_position = scanner_->location().beg_pos;
  log_->LogSymbol(literal_position, literal_chars, literal_length);

  return kUnknownExpression;
}


template <typename Scanner, typename Log>
Identifier PreParser<Scanner, Log>::ParseIdentifier(bool* ok) {
  Expect(i::Token::IDENTIFIER, ok);
  if (!*ok) return kUnknownIdentifier;
  return GetIdentifierSymbol();
}


template <typename Scanner, typename Log>
Identifier PreParser<Scanner, Log>::ParseIdentifierName(bool* ok) {
  i::Token::Value next = Next();
  if (i::Token::IsKeyword(next)) {
    int pos = scanner_->location().beg_pos;
    const char* keyword = i::Token::String(next);
    log_->LogSymbol(pos, keyword, strlen(keyword));
    return kUnknownExpression;
  }
  if (next == i::Token::IDENTIFIER) {
    return GetIdentifierSymbol();
  }
  *ok = false;
  return kUnknownIdentifier;
}


// This function reads an identifier and determines whether or not it
// is 'get' or 'set'.  The reason for not using ParseIdentifier and
// checking on the output is that this involves heap allocation which
// we can't do during preparsing.
template <typename Scanner, typename Log>
Identifier PreParser<Scanner, Log>::ParseIdentifierOrGetOrSet(bool* is_get,
                                                   bool* is_set,
                                                   bool* ok) {
  Expect(i::Token::IDENTIFIER, CHECK_OK);
  if (scanner_->literal_length() == 3) {
    const char* token = scanner_->literal_string();
    *is_get = strncmp(token, "get", 3) == 0;
    *is_set = !*is_get && strncmp(token, "set", 3) == 0;
  }
  return GetIdentifierSymbol();
}

#undef CHECK_OK
} }  // v8::preparser

#endif  // V8_PREPARSER_H
