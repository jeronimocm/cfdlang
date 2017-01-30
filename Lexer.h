
#ifndef __LEXER_H__
#define __LEXER_H__

#include <fstream>
#include <map>
#include <string>


typedef void* yyscan_t;

#include "ast.h"
#include "lang.tab.hh"
#include "lex.yy.h"


class Lexer {
private:
  const char *Input;

  yyscan_t Scanner;
  YYSTYPE Val;

public:
  Lexer(const char *input) : Input(input) {
    yylex_init(&Scanner);
    yy_scan_string(Input, Scanner);
  }

  ~Lexer() {
    yylex_destroy(Scanner);
  }

  int lex() {
    return yylex(&Val, Scanner);    
  }

  yyscan_t getScanner() const { return Scanner; }
  YYSTYPE getVal() const { return Val; }

  static std::map<int, const std::string> TokenStrings;

  static const std::string &getTokenString(int token) {
    return TokenStrings[token];
  }
};

#endif /* !__LEXER_H__ */

