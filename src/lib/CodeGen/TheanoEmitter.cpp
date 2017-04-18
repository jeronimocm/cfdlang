
#include <vector>


#include "AST/AST.h"
#include "CodeGen/TheanoEmitter.h"
#include "Sema/Sema.h"
#include "Sema/TensorType.h"


void TheanoEmitter::codeGen(const Program *p) {
  append("from theano import function as theano_function\n");
  append("import theano.tensor as " + getModulePrefix() + "\n\n");

  CG->visitProgram(p);
  append("\n");

  const Sema &sema = *getSema();

  std::map<const TensorType *, std::string> EmittedTypes;

  for (const auto *d: CG->getDeclarations()) {
    if (d->getNodeType() == ASTNode::NT_TypeDecl)
      continue;

    const std::string &name = d->getIdentifier()->getName();
    const Symbol *sym = sema.getSymbol(name);
    const TensorType *type = &sym->getType();

    std::string typeName;
    if (EmittedTypes.count(type)) {
      typeName = EmittedTypes[type];
    } else {
      typeName = (sema.isNamedType(type)) ? sema.getTypeSymbol(type)->getName()
      : getTemp();

      append(typeName + " = " + getModulePrefix()
             + ".TensorType('float64', (False,)*"
             + std::to_string(type->getRank()) + ")\n");
      EmittedTypes[type] = typeName;
    }

    append(name + " = " + getModulePrefix() + ".TensorVariable("
           + typeName + ")\n");
  }

  for (const auto *s: CG->getStatements()) {
    const ExprNode *en = getExprNode(s->getExpr());
    const std::string result = s->getIdentifier()->getName();

    // we need this if-clause since code emission
    // for identifiers has been optimized out:
    if (en->isIdentifier())
      append(result + " = " + en->getName());
    else {
      setResultTemp(result);
      en->visit(this);
    }
  }

  if (sema.inputs_size() == 0 || sema.outputs_size() == 0)
    return;

  #define IO_SYMBOL_LIST(inout)                                     \
    std::string inout##List;                                        \
    {                                                               \
      inout##List = "[";                                            \
      bool first = true;                                            \
      for (auto i = sema.inout##_begin(), e = sema.inout##_end(); \
           i != e; i++) {                                           \
        const Symbol *sym = *i;                                     \
        if (!first) inout##List += ", ";                            \
        inout##List += sym->getName();                              \
        first = false;                                              \
      }                                                             \
      inout##List += "]";                                           \
    }

  IO_SYMBOL_LIST(inputs)

  const std::string &functionName = getFunctionName();
  std::string output;
  if (sema.outputs_size() == 1) {
    const Symbol *sym = *sema.outputs_begin();
    append(getTemp() + " = " + functionName + "(" + inputsList + ", "
                                                  + sym->getName() + ")\n");
  } else {
    IO_SYMBOL_LIST(outputs)
    append(getTemp() + " = " + functionName + "(" + inputsList + ", "
                                                  + outputsList + ")\n");
  }

  for (auto in = sema.inputs_begin(), e = sema.inputs_end(); in != e; in++) {
    const Symbol *sym = *in;
    addFunctionArgument(sym->getName());
  }
}

void TheanoEmitter::visitBinOpExpr(const ExprNode *en, const std::string &op) {
  const std::string result = getResultTemp();
  std::string temps[2];

  assert(en->getNumChildren() == 2);
  for (int i = 0; i < 2; i++) {
    if (en->getChild(i)->isIdentifier()) {
      temps[i] = en->getChild(i)->getName();
    } else {
      temps[i] = getTemp();
      setResultTemp(temps[i]);
      en->getChild(i)->visit(this);
    }
  }

  append(result + " = " + temps[0] + " " + op + " " + temps[1] + "\n");
  setResultTemp(result);
}

void TheanoEmitter::visitAddExpr(const AddExpr *en) {
  visitBinOpExpr(en, "+");
}

void TheanoEmitter::visitSubExpr(const SubExpr *en) {
  visitBinOpExpr(en, "-");
}

void TheanoEmitter::visitMulExpr(const MulExpr *en) {
  visitBinOpExpr(en, "*");
}

void TheanoEmitter::visitScalarMulExpr(const ScalarMulExpr *en) {
  visitBinOpExpr(en, "*");
}

void TheanoEmitter::visitDivExpr(const DivExpr *en) {
  visitBinOpExpr(en, "/");
}

void TheanoEmitter::visitScalarDivExpr(const ScalarDivExpr *en) {
  visitBinOpExpr(en, "/");
}

void TheanoEmitter::visitTensordotExpr(const ExprNode *en,
                                        const std::string &axes) {
  const std::string result = getResultTemp();
  std::string temps[2];

  assert(en->getNumChildren() == 2);
  for (int i = 0; i < 2; i++) {
    if (en->getChild(i)->isIdentifier()) {
      temps[i] = en->getChild(i)->getName();
    } else {
      temps[i] = getTemp();
      setResultTemp(temps[i]);
      en->getChild(i)->visit(this);
    }
  }

  append(result + " = " + getModulePrefix() + ".tensordot("
                        + temps[0] + ", " + temps[1] + ", "
                        + "axes=" + axes + ")\n");
  setResultTemp(result);
}

void TheanoEmitter::visitContractionExpr(const ContractionExpr *en) {
  CodeGen::TupleList axes;
  axes.push_back(en->getLeftIndices());
  axes.push_back(en->getRightIndices());
  visitTensordotExpr(en, CodeGen::getTupleListString(axes));
}

void TheanoEmitter::visitProductExpr(const ProductExpr *en) {
  visitTensordotExpr(en, "0");
}

void TheanoEmitter::visitStackExpr(const StackExpr *en) {
  std::string result = getResultTemp();

  std::string stack;
  for (int i = 0; i < en->getNumChildren(); i++) {
    const ExprNode *child = en->getChild(i);

    if (child->isIdentifier()) {
      stack += child->getName();
    } else {
      const std::string temp = getTemp();
      setResultTemp(temp);
      child->visit(this);
      stack += temp;
    }

    if (i != en->getNumChildren()-1)
      stack += ", ";
  }
  append(result + " = " + getModulePrefix() +
         ".stack([" + stack + "])\n");

  setResultTemp(result);
}

void TheanoEmitter::visitIdentifierExpr(const IdentifierExpr *en) {
  assert(0 &&
         "internal error: code emission for identifier has been optimized out");
}
