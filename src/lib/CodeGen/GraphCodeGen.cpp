
#include <cassert>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>


#include "AST/AST.h"
#include "CodeGen/GraphCodeGen.h"
#include "Sema/Sema.h"
#include "Sema/TensorType.h"


void GraphCodeGen::updateCurEnd(GCG_Node *n) {
  if (curEnd != nullptr) {
    curEnd->setSucc(n);
    n->setPred(curEnd);
  }

  curEnd = n;
  return;
}

GraphCodeGen::GraphCodeGen(const Sema *sema)
  : CodeGen(sema) {}

GraphCodeGen::~GraphCodeGen() {
  for (const auto &it : Graphs) delete it.second;
}

void GraphCodeGen::visitProgram(const Program *p) {
  visitProgramPrologue(p);

  curGraph = nullptr;
  curLegs.clear();
  curEnd = nullptr;

  ASTVisitor::visitProgram(p);
  
  for(auto &g : Graphs)
    emitGraph(g.first, g.second);

  visitProgramEpilogue(p);
}

void GraphCodeGen::visitDecl(const Decl *d) {
  visitDeclPrologue(d);
  visitDeclEpilogue(d);
}

void GraphCodeGen::visitStmt(const Stmt *s) {
  const Expr *expr = s->getExpr();

  curGraph = new GCG_Graph;
  curLegs.clear();
  expr->visit(this);

  const std::string &name = s->getIdentifier()->getName();
  Graphs.push_back(GCG_LabeledGraph(name, curGraph));
}

void GraphCodeGen::visitIdentifier(const Identifier *id) {
  const TensorType *type = getSema()->getType(id);
  const int rank = type->getRank();
  GCG_Node *n = curGraph->getNode(StringID(getTemp(), id->getName(), id), rank);

  for (int i = 0; i < rank; i++)
    curLegs.push_back(GCG_Edge::NodeIndexPair(n, i));

  updateCurEnd(n);
}

void GraphCodeGen::visitInteger(const Integer *i) {
  assert(0 &&
         "internal error: integer should not be visited for graph generation");
}

void GraphCodeGen::visitParenExpr(const ParenExpr *pe) {
  const Expr *e = pe->getExpr();
  e->visit(this);
}

std::string GraphCodeGen::emitGraphForExpr(const Expr *expr) {  
  GCG_Graph *savedGraph = curGraph;
  GCG_Legs savedLegs = curLegs;
  GCG_Node *savedEnd = curEnd;

  std::string result = getTemp();
  {
    // no need to construct and emit a graph if there is
    // only a single identifier node:
    if (expr->isIdentifier()) {
      return dynamic_cast<const Identifier *>(expr)->getName();
    }

    GCG_Graph temporaryGraph;
    curGraph = &temporaryGraph;
    curLegs.clear();
    curEnd = nullptr;

    expr->visit(this);

    emitGraph(result, curGraph);
  }

  curEnd = savedEnd;
  curLegs = savedLegs;
  curGraph = savedGraph;
  
  return result;
}

void GraphCodeGen::visitBrackExpr(const BrackExpr *be) {
  const ExprList &exprs = *be->getExprs();
  const std::string result = getTemp();
  std::stringstream ssLabel;
  std::list<std::string> temps;


  ssLabel << "[";
  for (unsigned i = 0; i < exprs.size(); i++) {
    std::string t;

    t = emitGraphForExpr(exprs[i]);
    
    if (i > 0) ssLabel << ", ";
    ssLabel << t;
  }
  ssLabel << "]";

  emitTensorStack(result, temps);

  const TensorType *type = getSema()->getType(be);
  const int rank = type->getRank();
  GCG_Node *n = curGraph->getNode(StringID(result, ssLabel.str(), be), rank);

  for (int i = 0; i < rank; i++)
    curLegs.push_back(GCG_Edge::NodeIndexPair(n, i));

  updateCurEnd(n);
} 

void GraphCodeGen::visitBinaryExpr(const BinaryExpr *be) {
  const ASTNode::NodeType nt = be->getNodeType();

  if (nt == ASTNode::NT_ContractionExpr) {
    const BinaryExpr *tensor = extractTensorExprOrNull(be->getLeft());
    if (!tensor)
      assert(0 && "internal error: cannot handle general contractions yet");

    TupleList contractionsList;
    if (!getSema()->isListOfLists(be->getRight(), contractionsList))
      assert(0 && "internal error: cannot have a non-list here");

    if (contractionsList.empty())
      assert(0 && "internal error: cannot have an empty list here");

    visitContraction(tensor, contractionsList);
    return;
  } else if (nt == ASTNode::NT_ProductExpr) {
    const Expr *left = be->getLeft();
    left->visit(this);
    const Expr *right = be->getRight();
    right->visit(this);
    return;
  }

  // binary expression is NOT a contraction
  //                  and NOT a tesnor product:
  assert(nt != ASTNode::NT_ContractionExpr &&
         nt != ASTNode::NT_ProductExpr &&
         "internal error: should not be here");

  const std::string tempLHS = emitGraphForExpr(be->getLeft());
  const std::string tempRHS = emitGraphForExpr(be->getRight());
  
  const std::string result = getTemp();

  std::string OperatorLabel;
  switch (nt) {
  case ASTNode::NT_AddExpr:
    emitAddExpr(result, tempLHS, tempRHS);
    OperatorLabel = "+";
    break;
  case ASTNode::NT_SubExpr:
    emitSubExpr(result, tempLHS, tempRHS);
    OperatorLabel = "-";
    break;
  case ASTNode::NT_MulExpr:
    emitMulExpr(result, tempLHS, tempRHS);
    OperatorLabel = "*";
    break;
  case ASTNode::NT_DivExpr:
    emitDivExpr(result, tempLHS, tempRHS);
    OperatorLabel = "/";
    break;
  default:
    assert(0 && "internal error: invalid binary expression");
  }
  
  std::stringstream ssLabel;
  ssLabel << tempLHS << " " << OperatorLabel << " " << tempRHS;

  const TensorType *type = getSema()->getType(be);
  const int rank = type->getRank();
  GCG_Node *n = curGraph->getNode(StringID(result, ssLabel.str(), be), rank);

  for (int i = 0; i < rank; i++)
    curLegs.push_back(GCG_Edge::NodeIndexPair(n, i));

  updateCurEnd(n);
}

void GraphCodeGen::visitContraction(const Expr *e, const TupleList &indices) {
  if (indices.empty()) {
    e->visit(this);
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

  GCG_Legs savedLegs = curLegs;
  curLegs.clear();
  visitContraction(tensorL, contrL);

  // determine the rank of the resulting left sub-expression after
  // contraction has been performed over the set of index pairs 'contrL':
  int rankContractedL = rankL - 2*contrL.size();

  // the index pairs of the right sub-expression must be adjusted by
  // the rank of the left sub-expression:
  TupleList shiftedR = contrR;
  shiftTupleList(-rankContractedL, shiftedR);

  visitContraction(tensorR, shiftedR);

  if (contrMixed.empty())
    return;

  List indL, indR;
  unpackPairList(contrMixed, indL, indR);
  // only contractions in 'contrL' affect the adjustments
  // of the left indices in 'indL':
  adjustForContractions(indL, contrL);
  // adjustments of the right indices in 'indR' are affected by
  // the contractions in both 'contrL' and 'contrR':
  adjustForContractions(indR, contrL); adjustForContractions(indR, contrR);
  
  assert(indL.size() == indR.size() &&
         "internal error: mis-matched numbers of indices to be contracted");

  for (int k = 0; k < indL.size(); k++) {
    int iL = indL[k];
    int iR = indR[k];

    // the 'src' node correcponds to the left tensor in the contraction:
    const GCG_Node *srcNode = curLegs[iL].first;
    const int srcIndex = curLegs[iL].second;
    const GCG_Node *tgtNode = curLegs[iR].first;
    const int tgtIndex = curLegs[iR].second;

    std::stringstream ssLabel;
    ssLabel << "("    << srcNode->getID().getLabel() << ":" << srcIndex
            << " -- " << tgtNode->getID().getLabel() << ":" << tgtIndex << ")";

    bool success = curGraph->addEdge(StringID(getTemp(), ssLabel.str(), e),
                                     srcNode, srcIndex, tgtNode, tgtIndex);
    assert(success && "internal error: should not fail to add edge");
  }

  int erased = 0;
  for (int k = 0; k < indL.size(); k++)
    curLegs.erase(curLegs.begin() + indL[k] - (erased++));
  for (int k = 0; k < indR.size(); k++)
    curLegs.erase(curLegs.begin() + indR[k] - (erased++));

  for (int k = 0; k < curLegs.size(); k++) savedLegs.push_back(curLegs[k]);
  curLegs = savedLegs;
}

void GraphCodeGen::dump(const GCG_Graph &g) {
  const std::string tmpFileName = std::tmpnam(nullptr) + std::string(".dot");
  std::ofstream of(tmpFileName);
  std::cout << "Writing graph to file \'" << tmpFileName << "\' ... \n";
  g.plot(of);
  of.close();
}

void GraphCodeGen::dump() const {
  for(const auto &g : Graphs)
    dump(*g.second);
}

static const std::string getNodeIdString(const GraphCodeGen::GCG_Node &n) {
  const StringID &nID = n.getID();
  if (dynamic_cast<const Identifier *>(nID.getAST()))
    return nID.getLabel();
  else
    return nID.str();
}

void GraphCodeGen::emitGraph(const std::string &name, GCG_Graph *graph) {
  bool success;

  while (graph->getNumEdges()) {
    EdgeSet toContract;
    selectEdgesToContract(toContract, *graph);
    assert(!toContract.empty() &&
           "internal error: graph should still have edges");

    const GCG_Edge *firstEdge = *toContract.begin();
    GCG_Node &src = *graph->getNode(firstEdge->getSrcID());
    GCG_Node &tgt = *graph->getNode(firstEdge->getTgtID());
  
    List srcIndices, tgtIndices;
    for (const auto *e : toContract) {
      const GCG_Node &edgeSrc = *e->getSrcNode();
      const GCG_Node &edgeTgt = *e->getTgtNode();

      // the order matters; 'src' carries lower indices:
      assert((edgeSrc == src && edgeTgt == tgt));
      srcIndices.push_back(e->getSrcIndex());
      tgtIndices.push_back(e->getTgtIndex());
    }
      
    const std::string result = getTemp();
    emitContraction(result,
                    getNodeIdString(src), srcIndices,
                    getNodeIdString(tgt), tgtIndices);

    // find edges that remain at 'src' or 'tgt' after the contraction:
    EdgeSet edgesAtSrc, edgesAtTgt;
    getRemainingEdgesAtNode(edgesAtSrc, src, toContract);
    getRemainingEdgesAtNode(edgesAtTgt, tgt, toContract);

    int rank = src.getRank() + tgt.getRank() - 2*toContract.size();
    GCG_Node *n = graph->getNode(StringID(result, result, nullptr), rank);
    n->updateSequence(&src, &tgt);

    replaceEdgesAtNode(*graph, src, edgesAtSrc, *n,
                       0, toContract);
    replaceEdgesAtNode(*graph, tgt, edgesAtTgt, *n,
                       src.getRank() - toContract.size(), toContract);

    // erase all edges that have been contracted over:
    for (const auto *e : toContract)
      graph->eraseEdge(e->getID());

    // erase the nodes that are contracted:
    success = graph->eraseNode(src.getID());
    assert(success && "internal error: should not fail to erase source node");
    success = graph->eraseNode(tgt.getID());
    assert(success && "internal error: should not fail to erase target node");
  }

  // graph has no edges left; hence, form the tensor product
  // of the remaining nodes from left to right:
  const GCG_Node *n = graph->getStartNode();
  std::string temp = getNodeIdString(*n);
  while (n->hasSucc()) {
    const GCG_Node *succ = n->getSucc();
    const std::string &result = getTemp();
    
    emitTensorProduct(result, temp, succ->getID().str());
    temp = result;
    n = succ;
  }

  emitAssignment(name, temp);
}

void GraphCodeGen::getRemainingEdgesAtNode(EdgeSet &result, const GCG_Node &n,
                                           const EdgeSet &toContract) const {
  for (int i = 0; i < n.getRank(); i++) {
    if (!n.isSet(i))
      continue;

    const GCG_Edge *e = n.at(i);
    if (toContract.count(e))
      continue;

    result.insert(e);
  }
}

void GraphCodeGen::replaceEdgesAtNode(GCG_Graph &graph,
                                      const GCG_Node &oldNode,
                                      const EdgeSet &edgesAtOldNode,
                                      const GCG_Node &newNode,
                                      int shift, const EdgeSet &toContract) {
  std::function<int(int)>
  adjustForContractions = [oldNode, toContract](int index) {
    int adjustment = 0;
    for (const auto *e : toContract) {
      int oldNodeIndex = (oldNode == *e->getSrcNode()) ? e->getSrcIndex()
                                                       : e->getTgtIndex();
      adjustment += (oldNodeIndex < index);
    }
    return index - adjustment;
  };

  for (const auto *e : edgesAtOldNode) {
    assert(oldNode == *e->getSrcNode() || oldNode == *e->getTgtNode());
      
    const GCG_Node *newSrcNode = (oldNode == *e->getSrcNode())
                                 ? &newNode
                                 : e->getSrcNode();
    const int newSrcIndex = (oldNode == *e->getSrcNode())
                            ? adjustForContractions(e->getSrcIndex()) + shift
                            : e->getSrcIndex();

    const GCG_Node *newTgtNode = (oldNode == *e->getTgtNode())
                                 ? &newNode
                                 : e->getTgtNode();
    const int newTgtIndex = (oldNode == *e->getTgtNode())
                            ? adjustForContractions(e->getTgtIndex()) + shift
                            : e->getTgtIndex();

    std::stringstream ssLabel;
    ssLabel << "["    << newSrcNode->getID().getLabel() << ":" << newSrcIndex
            << " -- " << newTgtNode->getID().getLabel() << ":" << newTgtIndex
            << "]";
    
    GCG_Edge newEdge(StringID(getTemp(), ssLabel.str(),
                              nullptr /* no AST node */),
                     newSrcNode, newSrcIndex, newTgtNode, newTgtIndex);

    bool success = graph.eraseEdge(e->getID());
    assert(success && "internal error: should not fail to erase edge");
    success = graph.addEdge(newEdge);
    assert(success && "internal error: should not fail to add edge");
  }
}


void GraphCodeGen::selectEdgesToContract(EdgeSet &result,
                                         const GCG_Graph &g) const {
  if (!g.getNumEdges())
    return;

  // first try to find edges that connect predecessor and succesor:
  const GCG_Node *n = g.getStartNode();
  GCG_Graph::EdgeMap tmp;

  while (n->hasSucc()) {
    const GCG_Node *succ = n->getSucc();
    
    tmp.clear();
    g.getEdgesBetweenNodes(tmp, n, succ);
    if (!tmp.empty())
      break;

    n = succ;
  }

  // cannot yet handle graphs with no contraction between
  // predecessor and successor node pairs:
  assert(!tmp.empty() && "internal error: malformed contraction");

  for (const auto &it : tmp) {
    result.insert(it.second);
  }
}

