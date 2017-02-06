
#include <assert.h>
#include <list>
#include <set>
#include <string>
#include <vector>

#include "sema.h"
#include "symbol.h"
#include "type.h"


#define TYPE_MAP_ASSERT(expr) {                                    \
  if (ExprTypes.find((expr)) == ExprTypes.end())                   \
    assert(0 && "internal error: type of 'Expr' node not in map"); \
  }


bool Sema::isTypeName(const Expr *e, const TensorType *&type) const {
  const Identifier *id = dynamic_cast<const Identifier *>(e);
  if (!id)
    return false;

  const Symbol *sym = getSymbol(id->getName());
  if (!sym || (sym->getKind() != SK_Type))
    return false;

  type = &sym->getType();
  return true;
}

bool Sema::isIntegerList(const Expr *e, std::vector<int> &ints) const {
  const BrackExpr *be = dynamic_cast<const BrackExpr *>(e);
  if (!be)
    return false;

  const ExprList *list = be->getExprs();
  ints.clear();
  for (const auto &it : *list) {
    const Integer *integer = dynamic_cast<const Integer *>(it);
    if (!integer)
      return false;
    
    ints.push_back(integer->getValue());
  }

  return true;
}

bool Sema::isListOfLists(const Expr *e, std::vector<std::vector<int>> &lists) const {
  const BrackExpr *be = dynamic_cast<const BrackExpr *>(e);
  if (!be)
    return false;

  const ExprList *list = be->getExprs();
  lists.clear();
  for (const Expr * const &expr : *list) {
    std::vector<int> integers;
    if (!isIntegerList(expr, integers))
      return false;

    lists.push_back(integers);
  }

  return true;
}

const TensorType *Sema::visitTypeExpr(const Expr *e) {
  const TensorType *type;
  if (isTypeName(e, type))
    return type;
  
  std::vector<int> dims;
  if (isIntegerList(e, dims))
    return getType(dims);    
  
  assert(0 && "semantic error: invalid type expression");
  return nullptr;
}

void Sema::visitDecl(const Decl *d) {
  SymbolKind k = (d->getNodeType() == NT_VarDecl) ? SK_Variable : SK_Type;
  const std::string &name = d->getIdentifier()->getName();
  const TensorType *type = visitTypeExpr(d->getTypeExpr());

  if (getSymbol(name)) {
    assert(0 && ("semantic error: symbol \'" + name + "\' already declared")
                .c_str());
    return;
  }

  createSymbol(k, name, *type, d);
}

void Sema::visitStmt(const Stmt *s) {
  const Identifier *id = s->getIdentifier();
  const Expr *expr = s->getExpr();

  const Symbol *sym = getSymbol(id->getName());
  if (!sym) {
    assert(0 && ("semantic error: assignment to undeclared symbol \'"
                 + id->getName() + "\'").c_str());
    return;
  }

  expr->visit(this);
  TYPE_MAP_ASSERT(expr)

  const TensorType *type = ExprTypes[expr];
  if (*type != sym->getType()) {
    assert(0 && "semantic error: assigning non-equal types");
    return;
  }
}

void Sema::visitBinaryExpr(const BinaryExpr *be) {
  switch (be->getNodeType()) {
  case NT_TensorExpr: {
    const Expr *left = be->getLeft();
    left->visit(this);
    TYPE_MAP_ASSERT(left);
    const TensorType *type0 = ExprTypes[left];

    const Expr *right = be->getRight();
    right->visit(this);
    TYPE_MAP_ASSERT(right);
    const TensorType *type1 = ExprTypes[right];

    std::vector<int> dims;
    for (int i0 = 0; i0 < type0->getRank(); i0++)
      dims.push_back(type0->getDim(i0));
    for (int i1 = 0; i1 < type1->getRank(); i1++)
      dims.push_back(type1->getDim(i1));

    ExprTypes[be] = getType(dims);
    return;
  }
  case NT_DotExpr: {
    const Expr *left = be->getLeft();
    left->visit(this);
    TYPE_MAP_ASSERT(left);
    const TensorType *type0 = ExprTypes[left];
    
    std::vector<std::vector<int>> lists;
    if (!isListOfLists(be->getRight(), lists))
      assert(0 && "semantic error: right member of contraction not a list");

    if (lists.empty())
      assert(0 && "semantic error: contracting over empty index list");

    std::vector<int> res;
    for (int i = 0; i < type0->getRank(); i++)
      res.push_back(type0->getDim(i));
  
    std::set<int> index_set_to_erase;
    std::list<int> index_list_to_erase;
    for (const auto &list : lists) {
      // skip empty lists:
      if (!list.size())
        continue;

      int dim = type0->getDim(list[0]);
      for (int i: list) {
        if (type0->getDim(i) != dim) {
          assert(0 && "semantic error: incompatible indices in contraction");
        }
        if (index_set_to_erase.count(i)) {
          assert(0 && ("semantic error: index \'" + std::to_string(i)
                       + "\' appears multiple times").c_str());
        }
        index_set_to_erase.insert(i);
        index_list_to_erase.push_back(i);
      }
    }

    index_list_to_erase.sort();
    int erased = 0;
    for (int i : index_list_to_erase)
      res.erase(res.begin() + i - (erased++));

    ExprTypes[be] = getType(res);
    return;
  }

  default:
    assert(0 && "internal error: invalid binary expression");
  }
}

void Sema::visitIdentifier(const Identifier *id) {
  const Symbol *sym = getSymbol(id->getName());
  if (!sym) {
    assert(0 && ("semantic error: use of undeclared symbol \'"
                 + id->getName() + "\'").c_str());
  }

  ExprTypes[id] = &sym->getType();
}

void Sema::visitInteger(const Integer *i) {
  ExprTypes[i] = scalar;
}

void Sema::visitBrackExpr(const BrackExpr *be) {
  const ExprList &exprs = *be->getExprs();
  if (!exprs.size())
    assert(0 && "semantic error: tensor stack cannot be empty");

  exprs[0]->visit(this);
  TYPE_MAP_ASSERT(exprs[0]);
  const TensorType *type = getType(exprs[0]);

  for (const auto &e: exprs) {
    e->visit(this);
    TYPE_MAP_ASSERT(e);
    if (getType(e) != type)
      assert(0 && "semantic error: type mismatch in tensor stack");
  }

  std::vector<int> dims;
  dims.push_back(exprs.size());
  for (unsigned i = 0; i < type->getRank(); i++)
    dims.push_back(type->getDim(i));

  ExprTypes[be] = getType(dims);
}

void Sema::visitParenExpr(const ParenExpr *pe) {
  const Expr *e = pe->getExpr();
  e->visit(this);
  TYPE_MAP_ASSERT(e);
  ExprTypes[pe] = getType(e);
}

