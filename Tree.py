#Rudy Chavez 2/12/25#
##Module designed to create a working binary tree
import os
from math import exp, log2
from tkinter import NO
from tokenize import Exponent
from turtle import left

class BNode():
    nodecount =1
    blocknum = 4
    leafs = []
    def __init__(self):
        self.parent = None
        self.left = None
        self.right = None
        self.bucket = []
        self.nodex = 0
        self.nodeID = os.urandom(2)
    def aright(self,node):
        self.right = node
    def aleft(self,node):
        self.left = node
    def aparent(self,node):
        self.parent = node
    

    #adds block to node, and gives it neccesary info for creation
    def addblock(self, block):
        self.bucket.append(block)
    def removeblock(self, block):
        self.bucket.remove(block)
    def addleaf(self):
        self.nodex = BNode.nodecount
        BNode.nodecount += 1
        BNode.leafs.append(self)


class realBlock():
    bsize = 32 #bytes aka a lot of bits, hopefully a factor of 16 perhaps each block
    def __init__(self) -> None:
        self.addr = id(self)
        self.leafmap = None #leaf that this block is mapped to
        self.data = None #the encrypted data that the client uses
        #might make each node 32 bytees to make 64 bits, allowing for the block to be used
    
    #make dummy block to fill when creating a node
    def fakeblock(self):
        #fils data with random stuff
        #sets leafmap to 0
        #everything is a dummy block when initalized
        self.leafmap = 0
        self.data = bytes(os.urandom(16))

        pass

    #makes realblock with given data & padding

#height of tree is L and 2^L leaves (which will be used to map our stuff)
#idealy we need L = log2(N) in which N is the total number of blocks for the server
class OTree():
    def __init__(self, blockNumber) -> None:
        self.height = int(log2(blockNumber))
        self.blockNumber = blockNumber
        self.leafnumber = 2**self.height
        self.root = self.maketree()

    #makes tree and fills with dummy info, stash not being made yet 
    def maketree(self,level=0, pNode=None, posmap=[]):
        #if at height level, stop, if not, make left and right child
        
        n = BNode()
        n.parent = pNode
        if(level < self.height - 1): #to the n-1 level (since we are counting level 0)
            n.left = self.maketree(level + 1, n)
            n.right = self.maketree(level + 1, n)
        else: #iff leaf
            n.addleaf()
            ###MAKE FUNCITION HERE###
            ###Let it return a list of position maps!!

        return n
        

    def printroot(self):
        if self.root != None:
            print(self.root.nodex, "root")
        else:
            print("Nothinghere")
    def printree(self, node): #PRINT AS IF DFS
        if self.root != None:
            print(node.nodex)
            if(node.left or node.right):
                self.printree(node.left)
                self.printree(node.right)
            else:
                print("leaf")

    # def getNode(self):
    
    #     pass#returns node data of given specificiation

    # def trace_map(self, leafnodeID):
    #     #starts at root, and searches for the given leaf node and returns the node object
    #     #node object can then be traced to loook for the desired datablock/make a map

    #     pass
    def getleafs(self):
        #returns all the leaf nodes avaiable
        return(BNode.blocknum)
        

    #the tree should be in charge of receving inputs and giving outputs, and MAYBE scrambling as apart of the access function
        



tree = OTree(10)

tree.printree(tree.root)