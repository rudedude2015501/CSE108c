#Rudy Chavez 2/12/25#
##Module designed to create a working binary tree
import os
from math import exp, log2
from tkinter import NO
from tokenize import Exponent
from turtle import left
import base64
import json
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
    def __str__(self) -> str:
        s = str(self.nodeID) + " level:" + str(self.nodeheight) + " nodex:" + str(self.nodex)
        return s
    def serialize_bucket(self):
        # convert each block to a serializable dictionary
        serializable_bucket = []
        for block in self.bucket:
            block_dict = {
                'addr': block.addr,
                'leafmap': block.leafmap,
                'data': base64.b64encode(block.data).decode('utf-8') if block.data else None
            }
            serializable_bucket.append(block_dict)
        
        # serialize the list of block dictionaries to a JSON string
        return json.dumps(serializable_bucket)
    @classmethod
    def deserialize_bucket(cls, serialized_bucket):
        """
        Deserialize a JSON string back into a list of realBlock objects.
        """
        # Parse the JSON string
        bucket_data = json.loads(serialized_bucket)
        
        # Convert back to realBlock objects
        deserialized_bucket = []
        for block_dict in bucket_data:
            block = realBlock(
                addr=block_dict['addr'], 
                val=block_dict['leafmap'], 
                data=base64.b64decode(block_dict['data'].encode('utf-8')) if block_dict['data'] else b""
            )
            deserialized_bucket.append(block)
        
        return deserialized_bucket
    def update_bucket_from_serialized(self, serialized_bucket):
        # clear existing bucket
        self.bucket.clear()
        
        # add deserialized blocks
        deserialized_blocks = self.deserialize_bucket(serialized_bucket)
        self.bucket.extend(deserialized_blocks)

class realBlock():
    bsize = 32 #bytes aka a lot of bits, hopefully a factor of 16 perhaps each block
    def __init__(self,addr=0,leafmap=0,data:bytes = b"") -> None: 
        if addr == 0:
            self.addr = id(self)
        else:
            self.addr = addr
        self.leafmap = leafmap #leaf that this block is mapped to
        self.data = None #the encrypted data that the client uses
        if data != b"": #let data be a set of bytes
            self.data = data
        else:
            self.data = os.urandom(16)

        #might make each node 32 bytees to make 64 bits, allowing for the block to be used
    def changeleaf(self,new_leaf):
        self.leafmap = new_leaf
    # changed .leafmap = val to leafmap = leafmap
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

    #does a DFS search for a given leaf and returns the path as a list
    def getpath(self,leaf:int,node:BNode):
        leafpath = []
        leafpath.clear()
        self.__getpath__(leaf,node,leafpath)
        return leafpath

    def __getpath__(self,leaf:int,node,leafpath:list):
        leafpath.append(node.bucket) #us node.nodeID to check
        if node.nodex != leaf and node.nodex == 0: #if its not the leaf
            if self.__getpath__(leaf,node.left,leafpath) == True:
                return True
            elif self.__getpath__(leaf,node.right,leafpath) == True:
                return True
            leafpath.pop()
        elif node.nodex > 0 and node.nodex != leaf:#if another leaf
            leafpath.pop()
            return False
        elif node.nodex == leaf: #if it is the leaf
            return True

    #the tree should be in charge of receving inputs and giving outputs, and MAYBE scrambling as apart of the access function
    def get_path(self, leaf: int) -> list[BNode]:
        """Returns path from root to the specified leaf node."""
        path = []
        current = self.root
        binary_path = format(leaf - 1, f'0{self.height}b')  # 0-based index
        for bit in binary_path:
            path.append(current)
            current = current.left if bit == '0' else current.right
        path.append(current)  # Include the leaf node
        return path
    def is_in_subtree(self, target_leaf: int, subtree_root: BNode) -> bool:
        """Checks if target_leaf is in the subtree rooted at subtree_root."""
        target_path = self.get_path(target_leaf)
        return subtree_root in target_path

#testing
# t = OTree(20)
# print("height is: ",t.height)
# x = t.getpath(5,t.root)
# print(x)
# x = t.getpath(6,t.root)
# print(x)
# x = t.getpath(1,t.root)
# print(x)