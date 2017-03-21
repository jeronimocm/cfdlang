
#ifndef __TENSOR_GRAPH_HPP__
#define __TENSOR_GRAPH_HPP__

#include <cassert>
#include <sstream>


template<typename NodeID, typename EdgeID>
GraphNode<NodeID, EdgeID>::GraphNode(NodeID &&id, int rank)
  : ID(id), Rank(rank) {
  for (int i = 0; i < this->getRank(); i++)
    Legs.push_back(nullptr);

  setPred(nullptr);
  setSucc(nullptr);
}

template<typename NodeID, typename EdgeID>
const GraphEdge<NodeID, EdgeID> *const &
GraphNode<NodeID, EdgeID>::at(int i) const {
  assert(i < this->getRank());
  return Legs.at(i);
}

template<typename NodeID, typename EdgeID>
const GraphEdge<NodeID, EdgeID> *&
GraphNode<NodeID, EdgeID>::operator[](int i) {
  assert(i < this->getRank());
  return Legs[i];
}

template<typename NodeID, typename EdgeID>
void
GraphNode<NodeID, EdgeID>::unset(int i) {
  assert(i < this->getRank());
  Legs[i] = nullptr;
}

template<typename NodeID, typename EdgeID>
bool
GraphNode<NodeID, EdgeID>::isSet(int i) const {
  assert(i < this->getRank());
  return Legs[i] != nullptr;
}

template<typename NodeID, typename EdgeID>
bool
GraphNode<NodeID, EdgeID>::anySet() const {
  for (int i = 0; i < this->getRank(); i++)
    if (isSet(i))
      return true;

  return false;
}

template<typename NodeID, typename EdgeID>
bool
GraphNode<NodeID, EdgeID>::anyUnset() const {
  for (int i = 0; i < this->getRank(); i++)
    if (isUnset(i))
      return true;

  return false;
}

template<typename NodeID, typename EdgeID>
int
GraphNode<NodeID, EdgeID>::countSet() const {
  int result = 0;
  for (int i = 0; i < this->getRank(); i++)
    result += isSet(i);

  return result;
}

template<typename NodeID, typename EdgeID>
void
GraphNode<NodeID, EdgeID>::updateSequence(GraphNode<NodeID, EdgeID> *pred,
                                          GraphNode<NodeID, EdgeID> *succ) {
  setPred(pred);
  if (pred != nullptr)
    pred->setSucc(this);

  setSucc(succ);
  if (succ != nullptr)
    succ->setPred(this);
}

template<typename NodeID, typename EdgeID>
GraphEdge<NodeID, EdgeID>::GraphEdge(EdgeID &&id,
                                     const NodeIndexPair &src,
                                     const NodeIndexPair &tgt)
  : ID(id), Edge(src, tgt) {}

template<typename NodeID, typename EdgeID>
GraphEdge<NodeID, EdgeID>::GraphEdge(EdgeID &&id,
                                     const GraphNode<NodeID, EdgeID> *srcNode,
                                     int srcIndex,
                                     const GraphNode<NodeID, EdgeID> *tgtNode,
                                     int tgtIndex)
  : GraphEdge(EdgeID(id),
              NodeIndexPair(srcNode, srcIndex),
              NodeIndexPair(tgtNode, tgtIndex)) {}


template<typename NodeID, typename EdgeID>
TensorGraph<NodeID, EdgeID>::~TensorGraph() {
  bool success = true;
  
  while (!Edges.empty())
    success &= eraseEdge(Edges.begin()->first);

  while (!Nodes.empty())
    success &= eraseNode(Nodes.begin()->first);

  assert(success &&
         "internal error: erasing of graph components should not fail"); 
}

template<typename NodeID, typename EdgeID>
bool
TensorGraph<NodeID, EdgeID>::empty() const {
  if (!Nodes.empty())
    return false;

  assert(!getNumEdges() && "internal error: cannot have edges without nodes");
  return true;
}

template<typename NodeID, typename EdgeID>
int
TensorGraph<NodeID, EdgeID>::getNumEdges() const {
  return Edges.size();
}

template<typename NodeID, typename EdgeID>
int
TensorGraph<NodeID, EdgeID>::getNumEdges(const NodeID &id) const {
  if (!isNode(id))
    return 0;

  return Nodes.at(id).countSet();
}

template<typename NodeID, typename EdgeID>
void
TensorGraph<NodeID, EdgeID>::getEdgesFromNode(
  EdgeMap &result,
  const GraphNode<NodeID, EdgeID> *n) const
{
  for (const auto &e : Edges) {
    const GraphNode<NodeID, EdgeID> &src = *e.second->getSrcNode();

    if (*n == src)
      result[e.first] = e.second;
  }
}

template<typename NodeID, typename EdgeID>
void
TensorGraph<NodeID, EdgeID>::getEdgesToNode(
  EdgeMap &result,
  const GraphNode<NodeID, EdgeID> *n) const
{
  for (const auto &e : Edges) {
    const GraphNode<NodeID, EdgeID> &tgt = *e.second->getTgtNode();
    
    if (*n == tgt)
      result[e.first] = e.second;
  }
}

template<typename NodeID, typename EdgeID>
void
TensorGraph<NodeID, EdgeID>::getEdgesBetweenNodes(
  EdgeMap &result,
  const GraphNode<NodeID, EdgeID> *src,
  const GraphNode<NodeID, EdgeID> *tgt) const
{
  EdgeMap tmp;
  getEdgesFromNode(tmp, src);

  for (const auto &e : tmp) {
    const GraphNode<NodeID, EdgeID> &edgeSrc = *e.second->getSrcNode();
    const GraphNode<NodeID, EdgeID> &edgeTgt = *e.second->getTgtNode();

    assert(edgeSrc == *src);
    if (edgeTgt == *tgt)
      result[e.first] = e.second;
  }
}

template<typename NodeID, typename EdgeID>
bool
TensorGraph<NodeID, EdgeID>::isNode(const NodeID &id) const {
  return Nodes.count(id);
}

template<typename NodeID, typename EdgeID>
bool
TensorGraph<NodeID, EdgeID>::isEdge(const EdgeID &id) const {
  return Edges.count(id);
}

template<typename NodeID, typename EdgeID>
bool
TensorGraph<NodeID, EdgeID>::addNode(const NodeID &id, int rank) {
  if (isNode(id))
    return false;

  auto *n = new GraphNode<NodeID, EdgeID>(NodeID(id), rank);
  Nodes[id] = n;
  return true;
}

template<typename NodeID, typename EdgeID>
const GraphNode<NodeID, EdgeID> *
TensorGraph<NodeID, EdgeID>::getNode(const NodeID &id) const {
  assert(isNode(id));
  return Nodes.at(id);
}

template<typename NodeID, typename EdgeID>
GraphNode<NodeID, EdgeID> *
TensorGraph<NodeID, EdgeID>::getNode(const NodeID &id) {
  assert(isNode(id));
  return Nodes[id];
}

template<typename NodeID, typename EdgeID>
GraphNode<NodeID, EdgeID> *
TensorGraph<NodeID, EdgeID>::getNode(const NodeID &id, int rank) {
  if (!isNode(id)) {
    addNode(id, rank);
    return getNode(id);
  }

  GraphNode<NodeID, EdgeID> *n = getNode(id);
  assert(n->getRank() == rank);
  return n;
}

template<typename NodeID, typename EdgeID>
bool
TensorGraph<NodeID, EdgeID>::eraseNode(const NodeID &id) {
  if (!isNode(id))
    return false;

  GraphNode<NodeID, EdgeID> *n = Nodes[id];
  if (n->anySet())
      // Cannot erase this node if it has outgoing edges:
      return false;

  if (n->hasPred()) {
    GraphNode<NodeID, EdgeID> *pred = n->getPred();
    pred->setSucc(n->getSucc());
  }
  if (n->hasSucc()) {
    GraphNode<NodeID, EdgeID> *succ = n->getSucc();
    succ->setPred(n->getPred());
  }

  Nodes.erase(id);
  delete n;
  return true;
}

template<typename NodeID, typename EdgeID>
bool
TensorGraph<NodeID, EdgeID>::addEdge(const EdgeID &id,
                                     const GraphNode<NodeID, EdgeID> *srcNode,
                                     int srcIndex,
                                     const GraphNode<NodeID, EdgeID> *tgtNode,
                                     int tgtIndex) {
  if (!isNode(srcNode->getID()) || !isNode(tgtNode->getID()))
    return false;
  if (srcNode->isSet(srcIndex) || tgtNode->isSet(tgtIndex))
    return false;

  auto *e = new GraphEdge<NodeID, EdgeID>(EdgeID(id),
                                          srcNode, srcIndex,
                                          tgtNode, tgtIndex);
  Edges[id] = e;
  auto *writableSrcNode = getNode(srcNode->getID());
  auto *writableTgtNode = getNode(tgtNode->getID());
  (*writableSrcNode)[srcIndex] = e;
  (*writableTgtNode)[tgtIndex] = e;
  return true;
}

template<typename NodeID, typename EdgeID>
bool
TensorGraph<NodeID, EdgeID>::addEdge(const GraphEdge<NodeID, EdgeID> &e) {
  return addEdge(e.getID(),
                 e.getSrcNode(), e.getSrcIndex(),
                 e.getTgtNode(), e.getTgtIndex());
}

template<typename NodeID, typename EdgeID>
const GraphEdge<NodeID, EdgeID> *
TensorGraph<NodeID, EdgeID>::getEdge(const EdgeID &id) const {
  assert(isEdge(id));
  return Edges.at(id);
}

template<typename NodeID, typename EdgeID>
GraphEdge<NodeID, EdgeID> *
TensorGraph<NodeID, EdgeID>::getEdge(const EdgeID &id) {
  assert(isEdge(id));
  return Edges[id];
}

template<typename NodeID, typename EdgeID>
GraphEdge<NodeID, EdgeID> *
TensorGraph<NodeID, EdgeID>::getEdge(const EdgeID &id,
                                     const GraphNode<NodeID, EdgeID> *srcNode,
                                     int srcIndex,
                                     const GraphNode<NodeID, EdgeID> *tgtNode,
                                     int tgtIndex) {
  if (!isEdge(id))
    return addEdge(id, srcNode, srcIndex, tgtNode, tgtIndex);

  GraphEdge<NodeID, EdgeID> *e = getEdge(id);
  assert(e->getSrcNode() == srcNode && e->getSrcIndex == srcIndex &&
         e->getTgtNode() == tgtNode && e->getTgtIndex == tgtIndex);
  return e;
}

template<typename NodeID, typename EdgeID>
bool
TensorGraph<NodeID, EdgeID>::eraseEdge(const EdgeID &id) {
  if (!isEdge(id))
    return false;

  GraphEdge<NodeID, EdgeID> *e = getEdge(id);
  auto *srcNode = getNode(e->getSrcNode()->getID());
  auto *tgtNode = getNode(e->getTgtNode()->getID());
  srcNode->unset(e->getSrcIndex());
  tgtNode->unset(e->getTgtIndex());
  Edges.erase(id);
  delete e;
  return true;
}

template<typename NodeID, typename EdgeID>
void
TensorGraph<NodeID, EdgeID>::plot(std::ofstream &of) const {
  of << "digraph <" << this << "> {\n";
    
  for (const auto &n : Nodes) {
    const NodeID &id = n.first;
    of << "\"" << id.str()  << "\""
       << " [label=\"" << id.getLabel() << "\"];\n";
  }

  for (const auto &e : Edges) {
    const EdgeID &id = e.first;
    const NodeID &src = e.second->getSrcNode()->getID();
    const NodeID &tgt = e.second->getTgtNode()->getID();

    of << "\"" << src.str() << "\" -> \"" << tgt.str() << "\""
       << " [label=\"" << id.getLabel() << "\"];\n";
  }

  const GraphNode<NodeID, EdgeID> *n = getStartNode();
  while (n->hasSucc()) {
    const GraphNode<NodeID, EdgeID> *succ = n->getSucc();
    const NodeID &nID = n->getID();
    const NodeID &succID = succ->getID();

    of << "\"" << nID.str() << "\" -> \"" << succID.str() << "\""
       << " [style=dashed];\n";

    n = succ;
  }

  of << "}\n";
}

template<typename NodeID, typename EdgeID>
const GraphNode<NodeID, EdgeID> *
TensorGraph<NodeID, EdgeID>::getStartNode() const {
  const GraphNode<NodeID, EdgeID> *n = nodes_begin()->second;

  while (n->hasPred())
    n = n->getPred();

  return n;
}

#endif /* __TENSOR_GRAPH_HPP__ */

