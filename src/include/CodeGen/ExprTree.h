
#ifndef __EXPR_TREE_H__
#define __EXPR_TREE_H__

#include <set>
#include <string>
#include <vector>
#include <cassert>
#include <map>


#include "CodeGen/CodeGen.h"
#include "Sema/Symbol.h"


class ExprTreeVisitor;
class ExprTreeTransformer;


class ExprNode {
public:
  enum ExprKind {
    EK_Add,
    EK_Sub,
    EK_Mul,
    EK_Div,
    EK_Contraction,
    EK_Product,
    EK_Stack,
    EK_Identifier,

    EK_EXPRKIND_COUNT
  };

  typedef std::vector<int> ExprDimensions;

private:
  const ExprKind ExKind;

  const int NumChildren;

  std::vector<ExprNode *> Children;

  ExprDimensions Dims;

public:
  int getNumChildren() const { return NumChildren; }

  const ExprNode *getChild(int i) const {
    assert(i < getNumChildren());
    return Children[i];
  }

  ExprNode *getChild(int i) {
    assert(i < getNumChildren());
    return Children[i];
  }

public:
  ExprNode(ExprKind ek, int numChildren,
           const ExprDimensions &dims = ExprDimensions());
  virtual ~ExprNode() {}

  enum ExprKind getExprKind() const { return ExKind; }

  void setChild(int i, ExprNode *en) {
    assert(i < getNumChildren());
    Children[i] = en;
  }
  
protected:
  void setDims(const ExprDimensions &dims) { Dims = dims; }

  static std::map<ExprKind, std::string> ExprLabel;

public:
  const ExprDimensions &getDims() const { return Dims; }

  virtual void deepDelete() const;

  virtual void print(unsigned indent = 0) const;

  virtual void visit(ExprTreeVisitor *v) const = 0;
  virtual void transform(ExprTreeTransformer *t) = 0;

  virtual bool isIdentifier() const { return false; }
  virtual std::string getName() const { return ""; }
  virtual bool isContractionExpr() const { return false; }
  virtual bool isStackExpr() const { return false; }
};


#define DECL_EXPR_NODE_CLASS(Kind)                     \
class Kind##Expr : public ExprNode {                   \
public:                                                \
  Kind##Expr(ExprNode *lhs,                            \
             ExprNode *rhs)                            \
    : ExprNode(EK_##Kind, 2) {                         \
    setChild(0, lhs);                                  \
    setChild(1, rhs);                                  \
    /* by type checking, the following must hold:
     * 'lhs->getDims() == rhs->getDims()'
     */                                                \
    setDims(lhs->getDims());                           \
  }                                                    \
                                                       \
  virtual void visit(ExprTreeVisitor *v) const;        \
  virtual void transform(ExprTreeTransformer *t);      \
                                                       \
  static Kind##Expr *create(ExprNode *lhs,             \
                            ExprNode *rhs) {           \
    return new Kind##Expr(lhs, rhs);                   \
  }                                                    \
};

DECL_EXPR_NODE_CLASS(Add)
DECL_EXPR_NODE_CLASS(Sub)
DECL_EXPR_NODE_CLASS(Mul)
DECL_EXPR_NODE_CLASS(Div)

#undef DECL_EXPR_NODE_CLASS


class ProductExpr : public ExprNode {
public:
  ProductExpr(ExprNode *lhs, ExprNode *rhs);

  virtual void visit(ExprTreeVisitor *v) const;
  virtual void transform(ExprTreeTransformer *t);

  static ProductExpr *create(ExprNode *lhs, ExprNode *rhs) {
    return new ProductExpr(lhs, rhs);
  }
};


class ContractionExpr : public ExprNode {
private:
  const CodeGen::List LeftIndices;
  const CodeGen::List RightIndices;

public:
  ContractionExpr(ExprNode *lhs, const CodeGen::List &leftIndices,
                  ExprNode *rhs, const CodeGen::List &rightIndices);

  virtual bool isContractionExpr() const override { return true; }

  const CodeGen::List &getLeftIndices() const { return LeftIndices; }
  const CodeGen::List &getRightIndices() const { return RightIndices; }

  virtual void print(unsigned indent = 0) const override;

  virtual void visit(ExprTreeVisitor *v) const override;
  virtual void transform(ExprTreeTransformer *t) override;

  static ContractionExpr *create(ExprNode *lhs,
                                 const CodeGen::List &leftIndices,
                                 ExprNode *rhs,
                                 const CodeGen::List &rightIndices) {
    return new ContractionExpr(lhs, leftIndices, rhs, rightIndices);
  }
};


class StackExpr : public ExprNode {
public:
  StackExpr(const std::vector<ExprNode *> &members);

  virtual bool isStackExpr() const override { return true; }

  virtual void visit(ExprTreeVisitor *v) const override;
  virtual void transform(ExprTreeTransformer *t) override;

  static StackExpr *create(const std::vector<ExprNode *> &members) {
    return new StackExpr(members);
  }
};


class IdentifierExpr : public ExprNode {
private:
  const std::string Name;

public:
  IdentifierExpr(const std::string &name, const ExprDimensions &dims)
    : ExprNode(EK_Identifier, 0, dims), Name(name) {}

  virtual bool isIdentifier() const override { return true; }
  virtual std::string getName() const override { return Name; }

  virtual void print(unsigned indent = 0) const override;
 
  virtual void visit(ExprTreeVisitor *v) const override;
  virtual void transform(ExprTreeTransformer *t) override;

  static IdentifierExpr *create(const std::string &name,
                                const ExprDimensions &dims) {
    return new IdentifierExpr(name, dims);
  }
};



    /****************************/
    /* visitor class/interface: */
    /****************************/

class ExprTreeVisitor {
public:
  #define DECL_VISIT_EXPR_NODE(Kind)                       \
  virtual void visit##Kind##Expr(const Kind##Expr *e) = 0;

  DECL_VISIT_EXPR_NODE(Add)
  DECL_VISIT_EXPR_NODE(Sub)
  DECL_VISIT_EXPR_NODE(Mul)
  DECL_VISIT_EXPR_NODE(Div)
  DECL_VISIT_EXPR_NODE(Contraction)
  DECL_VISIT_EXPR_NODE(Product)
  DECL_VISIT_EXPR_NODE(Stack)
  DECL_VISIT_EXPR_NODE(Identifier)

  #undef DECL_VISIT_EXPR_NODE
};

    /********************************/
    /* transformer class/interface: */
    /********************************/

class ExprTreeTransformer {
public:
  #define DECL_TRANSFORM_EXPR_NODE(Kind)                 \
  virtual void transform##Kind##Expr(Kind##Expr *e) = 0;

  DECL_TRANSFORM_EXPR_NODE(Add)
  DECL_TRANSFORM_EXPR_NODE(Sub)
  DECL_TRANSFORM_EXPR_NODE(Mul)
  DECL_TRANSFORM_EXPR_NODE(Div)
  DECL_TRANSFORM_EXPR_NODE(Contraction)
  DECL_TRANSFORM_EXPR_NODE(Product)
  DECL_TRANSFORM_EXPR_NODE(Stack)
  DECL_TRANSFORM_EXPR_NODE(Identifier)

  #undef DECL_TRANSFORM_EXPR_NODE
};


    /****************************************/
    /* factory class for memory management: */
    /****************************************/

class ExprNodeBuilder {
private:
  std::set<const ExprNode *> AllocatedNodes;

public:
  ~ExprNodeBuilder();

  #define DECL_BUILDER_CREATE_EXPR_NODE(Kind)                   \
  Kind##Expr *create##Kind##Expr(ExprNode *lhs, ExprNode *rhs);

  DECL_BUILDER_CREATE_EXPR_NODE(Add)
  DECL_BUILDER_CREATE_EXPR_NODE(Sub)
  DECL_BUILDER_CREATE_EXPR_NODE(Mul)
  DECL_BUILDER_CREATE_EXPR_NODE(Div)
  DECL_BUILDER_CREATE_EXPR_NODE(Product)

  #undef DECL_BUILDER_CREATE_EXPR_NODE

  ContractionExpr *createContractionExpr(ExprNode *lhs,
                                         const CodeGen::List &leftIndices,
                                         ExprNode *rhs,
                                         const CodeGen::List &rightIndices);
  StackExpr *createStackExpr(const std::vector<ExprNode *> &members);
  IdentifierExpr *createIdentifierExpr(const std::string &name,
                                       const ExprNode::ExprDimensions &dims);
};

#endif /* __EXPR_TREE_H__ */
