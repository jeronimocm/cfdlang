
#ifndef __SEMA_H__
#define __SEMA_H__

#include <list>
#include <map>
#include <set>
#include <vector>


#include "AST/AST.h"
#include "Sema/Symbol.h"
#include "Sema/TensorType.h"


namespace CFDlang {

class Sema : public ASTVisitor {
private:
  const TensorType *scalar;
  std::list<const TensorType *> Types;

  SymbolTable Symbols;

  // map 'Expr' nodes in the AST to types:
  std::map<const Expr *, const TensorType *> ExprTypes;

  // record symbols of variables that are
  // declared with IO specifiers:
  std::vector<const Symbol *> Inputs;
  std::vector<const Symbol *> Outputs;

  // map user-defined types (i.e. named types) to the
  // corresponding symbols (which hold the type names):
  std::map<const TensorType *, const Symbol *> NamedTypes;

public:
  struct ElemInfo {
    // Is directive for element loop present:
    bool present;

    // Position of element index (first/last):
    ElemDirect::POSSpecifier pos;

    // Dimension, i.e. number of elements:
    unsigned dim;

    // Symbols (i.e. Identifiers) that take an element index:
    std::set<const Symbol *> syms;
  };

private:
  ElemInfo elemInfo;

public:
  Sema();
  ~Sema();

  const TensorType *createType(const std::vector<int> &dims);
  const TensorType *getType(const std::vector<int> &dims);
  const TensorType *getType(const std::vector<int> &dims) const;
  const TensorType *getType(const Expr *e) const { return ExprTypes.at(e); }
  const TensorType &getScalar() const { return *scalar; }
  bool isScalar(const TensorType &t) const { return (t == getScalar()); }

  const Symbol *createSymbol(Symbol::SymbolKind k, const std::string &name,
                             const TensorType &type,
                             const Decl *decl = nullptr);
  const Symbol *getSymbol(const std::string &name) const;

  bool isTypeName(const Expr *e, const TensorType *&type) const;
  static bool isIntegerList(const Expr *e, std::vector<int> &ints);
  static bool isListOfLists(const Expr *e,
                            std::vector<std::vector<int>> &lists);

  const TensorType *visitTypeExpr(const Expr *e);

  virtual void visitDecl(const Decl *d) override;
  virtual void visitStmt(const Stmt *s) override;

  virtual void visitElemDirect(const ElemDirect *ed) override;

  virtual void visitBinaryExpr(const BinaryExpr *be) override;
  virtual void visitIdentifier(const Identifier *id) override;
  virtual void visitInteger(const Integer *i) override;
  virtual void visitBrackExpr(const BrackExpr *be) override;
  virtual void visitParenExpr(const ParenExpr *pe) override;

  std::list<const TensorType *>::const_iterator types_begin() const {
    return Types.begin();
  }
  std::list<const TensorType *>::const_iterator types_end() const {
    return Types.end();
  }

  bool is_in_inputs(const std::string &name) const {
    const Symbol *sym = getSymbol(name);
    for (auto s: Inputs) {
      if (s == sym)
        return true;
    }
    return false;
  }
  std::vector<const Symbol *>::const_iterator inputs_begin() const {
    return Inputs.begin();
  }
  std::vector<const Symbol *>::const_iterator inputs_end() const {
    return Inputs.end();
  }
  int inputs_size() const { return Inputs.size(); }

  bool is_in_outputs(const std::string &name) const {
    const Symbol *sym = getSymbol(name);
    for (auto s: Outputs) {
      if (s == sym)
        return true;
    }
    return false;
  }
  std::vector<const Symbol *>::const_iterator outputs_begin() const {
    return Outputs.begin();
  }
  std::vector<const Symbol *>::const_iterator outputs_end() const {
    return Outputs.end();
  }
  int outputs_size() const { return Outputs.size(); }

  bool isNamedType(const TensorType *type) const;
  const Symbol *getTypeSymbol(const TensorType *type) const;

  const ElemInfo &getElemInfo() const { return elemInfo; }
};

};

#endif /* !__SEMA_H__ */
