
#include <assert.h>
#include <vector>
#include <string>


#include "AST/AST.h"
#include "CodeGen/DirectCodeGen.h"
#include "Sema/Sema.h"
#include "Sema/TensorType.h"


using namespace CFDlang;

DirectCodeGen::DirectCodeGen(const Sema *sema, const std::string &functionName)
  : CodeGen(sema, functionName) {}

void DirectCodeGen::visitStmt(const Stmt *s) {
  const Expr *expr = s->getExpr();
  expr->visit(this);
  EXPR_TREE_MAP_ASSERT(expr);

  CodeGen::visitStmt(s);
}

void DirectCodeGen::visitContraction(const Expr *e, const TupleList &indices) {
  if (indices.empty()) {
    e->visit(this);
    EXPR_TREE_MAP_ASSERT(e);
    return;
  }
  
  const BinaryExpr *tensor = extractTensorExprOrNull(e);
  if (!tensor)
    assert(0 && "internal error: cannot handle general contractions yet");

  if (!isPairList(indices))
    assert(0 && "internal error: only pairs of indices can be contracted");

  const Expr *tensorL = tensor->getLeft();
  const Expr *tensorR = tensor->getRight();
  const TensorType *typeL = getSema()->getType(tensorL);
  int rankL = typeL->getRank();

  TupleList contrL, contrR, contrMixed;
  // classify index pairs into the following categories:
  // - belonging to contractions of the left sub-expression ('contrL')
  // - belonging to contractions of the right sub-expression ('contrR')
  // - having one index from each sub-expression ('contrMixed')
  partitionPairList(rankL, indices, contrL, contrR, contrMixed);

  visitContraction(tensorL, contrL);
  EXPR_TREE_MAP_ASSERT(tensorL);

  // determine the rank of the resulting left sub-expression after
  // contraction has been performed over the set of index pairs 'contrL':
  int rankContractedL = rankL - 2*contrL.size();

  // the index pairs of the right sub-expression must be adjusted by
  // the rank of the left sub-expression:
  TupleList shiftedR = contrR;
  shiftTupleList(-rankL, shiftedR);
  visitContraction(tensorR, shiftedR);
  EXPR_TREE_MAP_ASSERT(tensorR);

  if (contrMixed.empty()) {
    addExprNode(e, ENBuilder->createProductExpr(getExprNode(tensorL),
                                                getExprNode(tensorR)));
    return;
  }

  List indL, indR;
  unpackPairList(contrMixed, indL, indR);
  // only contractions in 'contrL' affect the adjustments
  // of the left indices in 'indL':
  adjustForContractions(indL, contrL);
  // adjustments of the right indices in 'indR' are affected by
  // the contractions in both 'contrL' and 'contrR':
  adjustForContractions(indR, contrL); adjustForContractions(indR, contrR);
  // the indices to be contracted over in the right sub-expression
  // must be relative to the first index of the right sub-expresion:
  shiftList(-rankContractedL, indR);

  addExprNode(e, ENBuilder->createContractionExpr(getExprNode(tensorL), indL,
                                                  getExprNode(tensorR), indR));
}

void DirectCodeGen::visitBinaryExpr(const BinaryExpr *be) {
  const Sema &sema = *getSema();
  const ASTNode::NodeType nt = be->getNodeType();

  if (nt == ASTNode::NT_ContractionExpr)
  {
    TupleList contractionsList;
    if (Sema::isListOfLists(be->getRight(), contractionsList))
    {
      const BinaryExpr *tensor = extractTensorExprOrNull(be->getLeft());
      if (!tensor)
        assert(0 && "internal error: cannot handle general contractions yet");

      if (contractionsList.empty())
        assert(0 && "internal error: cannot have an empty list here");

      visitContraction(tensor, contractionsList);
      EXPR_TREE_MAP_ASSERT(tensor);
      addExprNode(be, getExprNode(tensor));
    }
    else
    {
      const Expr *left = be->getLeft();
      left->visit(this);
      EXPR_TREE_MAP_ASSERT(left);

      const Expr *right = be->getRight();
      right->visit(this);
      EXPR_TREE_MAP_ASSERT(right);

      const int leftRank = sema.getType(left)->getRank();
      addExprNode(be, ENBuilder->createContractionExpr(getExprNode(left),
                                                       {leftRank-1},
                                                       getExprNode(right),
                                                       {0}));
    }
    return;
  } else if (nt == ASTNode::NT_TranspositionExpr) {
    const Expr *left = be->getLeft();
    left->visit(this);
    EXPR_TREE_MAP_ASSERT(left);

    TupleList indexPairs;
    if (!Sema::isListOfLists(be->getRight(), indexPairs))
      assert(0 && "internal error: right member of transposition not a list");

    if (indexPairs.empty())
      assert(0 && "internal error: cannot have an empty list here");

    addExprNode(be, ENBuilder->createTranspositionExpr(getExprNode(left),
                                                       indexPairs));
    return;
  }

  // binary expression is NOT a contraction and NOT a transposition:
  assert(nt != ASTNode::NT_ContractionExpr &&
         nt != ASTNode::NT_TranspositionExpr &&
         "internal error: should not be here");

  const Expr *left = be->getLeft();
  left->visit(this);
  EXPR_TREE_MAP_ASSERT(left);

  const Expr *right = be->getRight();
  right->visit(this);
  EXPR_TREE_MAP_ASSERT(right);

  ExprNode *result,
           *lhs = getExprNode(left),
           *rhs = getExprNode(right);
  switch(nt) {
  case ASTNode::NT_AddExpr:
    result = ENBuilder->createAddExpr(lhs, rhs);
    break;
  case ASTNode::NT_SubExpr:
    result = ENBuilder->createSubExpr(lhs, rhs);
    break;
  case ASTNode::NT_MulExpr:
    if (sema.isScalar(*sema.getType(left)))
      result = ENBuilder->createScalarMulExpr(lhs, rhs);
    else
      result = ENBuilder->createMulExpr(lhs, rhs);
    break;
  case ASTNode::NT_DivExpr:
    if (sema.isScalar(*sema.getType(right)))
      result = ENBuilder->createScalarDivExpr(lhs, rhs);
    else
      result = ENBuilder->createDivExpr(lhs, rhs);
    break;
  case ASTNode::NT_ProductExpr:
    result = ENBuilder->createProductExpr(lhs, rhs);
    break;
  default:
    assert(0 && "internal error: invalid binary expression");
  }

  addExprNode(be, result);
}

void DirectCodeGen::visitIdentifier(const Identifier *id) {
  const Sema &sema = *getSema();
  const std::string &name = id->getName();
  const TensorType &type = sema.getSymbol(name)->getType();

  addExprNode(id, ENBuilder->createIdentifierExpr(name, type.getDims()));
}

void DirectCodeGen::visitBrackExpr(const BrackExpr *be) {
  const ExprList &exprs = *be->getExprs();
  assert(exprs.size() &&
         "internal error: tensor stack should not be empty here");

  std::vector<ExprNode *> members;
  for (unsigned i = 0; i < exprs.size(); i++) {
    const Expr *e = exprs[i];
    e->visit(this);
    EXPR_TREE_MAP_ASSERT(e);

    members.push_back(getExprNode(e));
  }

  addExprNode(be, ENBuilder->createStackExpr(members));
} 

void DirectCodeGen::visitParenExpr(const ParenExpr *pe) {
  const Expr *e = pe->getExpr();
  e->visit(this);
  EXPR_TREE_MAP_ASSERT(e);

  addExprNode(pe, getExprNode(e));
}
