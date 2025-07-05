#pragma once
#include <algorithm>
#include <assert.h>
#include <math.h>
#include <stdio.h>

namespace kdTree
{ 
	struct BinaryTree
	{
		static int rootNode() { return 0; }
		static int parentOf(int nodeID) { return (nodeID - 1) / 2; }
		static int isLeftSibling(int nodeID) { return (nodeID & 1); }
		static int leftChildOf(int nodeID) { return 2 * nodeID + 1; }
		static int rightChildOf(int nodeID) { return 2 * nodeID + 2; }
		static int firstNodeInLevel(int L) { return (1 << L) - 1; }
		static int levelOf(int nodeID)
		{
			int k = 63 - __builtin_clzll(nodeID + 1);
			return k;
		}
		static int numLevelsFor(int numPoints)
		{
			return levelOf(numPoints - 1) + 1;
		}
		static int numSiblingsToLeftOf(int n)
		{
			int levelOf_n = BinaryTree::levelOf(n);
			return n - BinaryTree::firstNodeInLevel(levelOf_n);
		}
	};

	struct FullBinaryTreeOf
	{
		FullBinaryTreeOf(int numLevels) : numLevels(numLevels) {}
		int numNodes() const { return (1 << numLevels) - 1; }
		int numOnLastLevel() const { return (1 << (numLevels - 1)); }
		const int numLevels;
	};

	struct SubTreeInFullTreeOf
	{
		SubTreeInFullTreeOf(int numLevelsTree, int subtreeRoot)
			: numLevelsTree(numLevelsTree), subtreeRoot(subtreeRoot),
			levelOfSubtree(BinaryTree::levelOf(subtreeRoot)),
			numLevelsSubtree(numLevelsTree - levelOfSubtree)
		{
		}
		int lastNodeOnLastLevel() const
		{
			// return ((subtreeRoot+2) << (numLevelsSubtree-1)) - 2;
			int first = (subtreeRoot + 1) << (numLevelsSubtree - 1);
			int onLast = (1 << (numLevelsSubtree - 1)) - 1;
			return first + onLast;
		}
		int numOnLastLevel() const
		{
			return FullBinaryTreeOf(numLevelsSubtree).numOnLastLevel();
		}
		int numNodes() const
		{
			return FullBinaryTreeOf(numLevelsSubtree).numNodes();
		}

		const int numLevelsTree;
		const int subtreeRoot;
		const int levelOfSubtree;
		const int numLevelsSubtree;
	};

	inline int clamp(int val, int lo, int hi)
	{
		return std::max(std::min(val, hi), lo);
	}

	struct ArbitraryBinaryTree
	{
		ArbitraryBinaryTree(int numNodes) : numNodes(numNodes) {}
		int numNodesInSubtree(int n)
		{
			auto fullSubtree =
				SubTreeInFullTreeOf(BinaryTree::numLevelsFor(numNodes), n);
			const int lastOnLastLevel = fullSubtree.lastNodeOnLastLevel();
			const int numMissingOnLastLevel =
				clamp(lastOnLastLevel - numNodes, 0, fullSubtree.numOnLastLevel());
			const int result = fullSubtree.numNodes() - numMissingOnLastLevel;
			return result;
		}

		const int numNodes;
	};

	struct ArrayLayoutInStep
	{
		ArrayLayoutInStep(int step, /* num nodes in three: */ int numPoints)
			: numLevelsDone(step), numPoints(numPoints)
		{
		}

		int numSettledNodes() const
		{
			return FullBinaryTreeOf(numLevelsDone).numNodes();
		}

		int segmentBegin(int subtreeOnLevel)
		{
			int numSettled = FullBinaryTreeOf(numLevelsDone).numNodes();
			int numLevelsTotal = BinaryTree::numLevelsFor(numPoints);
			int numLevelsRemaining = numLevelsTotal - numLevelsDone;

			int firstNodeInThisLevel = FullBinaryTreeOf(numLevelsDone).numNodes();
			int numEarlierSubtreesOnSameLevel =
				subtreeOnLevel - firstNodeInThisLevel;

			int numToLeftIfFull = numEarlierSubtreesOnSameLevel *
								FullBinaryTreeOf(numLevelsRemaining).numNodes();

			int numToLeftOnLastIfFull =
				numEarlierSubtreesOnSameLevel *
				FullBinaryTreeOf(numLevelsRemaining).numOnLastLevel();

			int numTotalOnLastLevel =
				numPoints - FullBinaryTreeOf(numLevelsTotal - 1).numNodes();

			int numReallyToLeftOnLast =
				std::min(numTotalOnLastLevel, numToLeftOnLastIfFull);
			int numMissingOnLast = numToLeftOnLastIfFull - numReallyToLeftOnLast;

			int result = numSettled + numToLeftIfFull - numMissingOnLast;
			return result;
		}

		int pivotPosOf(int subtree)
		{
			int segBegin = segmentBegin(subtree);
			int pivotPos = segBegin + sizeOfLeftSubtreeOf(subtree);
			return pivotPos;
		}

		int sizeOfLeftSubtreeOf(int subtree)
		{
			int leftChildRoot = BinaryTree::leftChildOf(subtree);
			if (leftChildRoot >= numPoints)
				return 0;
			return ArbitraryBinaryTree(numPoints).numNodesInSubtree(leftChildRoot);
		}

		int sizeOfSegment(int n) const
		{
			return ArbitraryBinaryTree(numPoints).numNodesInSubtree(n);
		}

		const int numLevelsDone;
		const int numPoints;
	};
}

