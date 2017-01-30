
#include <map>
#include <string>


#include "Lexer.h"


std::map<int, const std::string> Lexer::TokenStrings = {
  { KW_VAR, "KW_VAR" },
  { KW_TYPE, "KW_TYPE" },
  { COLON, "COLON" },
  { LPAREN, "LPAREN" },
  { RPAREN, "RPAREN" },
  { LBRACK, "LBRACK" },
  { RBRACK, "RBRACK" },
  { STAR, "STAR" },
  { DOT, "DOT" },
  { EQUAL, "EQUAL" },
  { INT, "INT" },
  { ID, "ID" }
};

