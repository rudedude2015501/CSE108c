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
        self.nodeheight = 0
        for i in range(0,BNode.blocknum): #fills bucket with dummy blocks
            self.bucket.append(realBlock())


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
    def __init__(self,val=0,data=None) -> None:
        self.addr = id(self)
        self.leafmap = val #leaf that this block is mapped to
        self.data = None #the encrypted data that the client uses
        if data != None:
            self.data = data
        else:
            self.data = bytes(os.urandom(16))
        #might make each node 32 bytees to make 64 bits, allowing for the block to be used
    def changeleaf(self,newval):
        self.leafmap = newval
    def changedata(self,newdata):
        self.data = newdata

    #makes realblock with given data & padding

#height of tree is L and 2^L leaves (which will be used to map our stuff)
#idealy we need L = log2(N) in which N is the total number of blocks for the server
class OTree():
    def __init__(self, BucketNumber) -> None:
        #The tree initalizes itself and also makes a positiion map of itself for the client to use
        self.height = int(log2(BucketNumber))
        self.BucketNumber = BucketNumber
        self.leafnumber = 2**self.height
        self.posmap = []
        self.root = self.maketree()

    #makes tree and fills with dummy info, stash not being made yet 
    def maketree(self,level=0, pNode=None):
        # if stack == None:
        #     stack = []
        
        n = BNode()
        # stack.append(n.nodeID)
        n.parent = pNode
        n.nodeheight = level
        if(level < self.height - 1): #to the n-1 level (since we are counting level 0)
            #this method is very similar to a dfs code using preorder going from most left node to most right node
            n.left = self.maketree(level + 1, n)
            # stack.pop()
            n.right = self.maketree(level + 1, n)
            # stack.pop()
        else: #iff leafY
            n.addleaf()
            # self.posmap.append(stack.copy()) #adds path to map
            
        return n
        

    def printroot(self):
        if self.root != None:
            print(self.root.nodex, "root")
        else:
            print("Nothinghere")
    def printree(self, node): #PRINT AS IF DFS
        if self.root != None:
            print(node.nodex, " level: ",node.nodeheight)
            if(node.left or node.right):
                self.printree(node.left)
                self.printree(node.right)
            else:
                print("leaf")

    def getroot(self):
        return self.root    

    #the tree should be in charge of receving inputs and giving outputs, and MAYBE scrambling as apart of the access function
t = OTree(20)
t.printree(t.root)