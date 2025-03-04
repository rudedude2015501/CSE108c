#Rudy Chave 
#3/3/25
#file contains the access and setup functions for Path ORAM
from ast import List
from tkinter import NO
import Tree as T
import numpy

PositionMap = {} #Holds Blocks and which leaf node mapped to, so [B1] : Leaf X, [B2] : Leaf X

PathMap = {} #Holds leaf nodes and the path to take them, so Leaf X : path = root/node/node.../Leaf X

Stash = [] #list of buckets
###SERVER OPERATION###

#setup, simply calls for the tree to be made using the Tree.py Otreaa.setup funciton
#input: block size output: object to the tree
#returs a tree function to be used, and a file to store the data
def Initialization(entrycardinality:int): #entryBsize, the size of the entries to the nearest power of 2, entrycardinality: the amount of entries to be proccesed
    Rpath = T.OTree(entrycardinality/4) #four blocks per bucket, therfore the total amount of blocks divded by four
    return Rpath


#CLIENT OPERATION

#access() #client operation

#input: operation, address, data if neccesary)
#output success of operation
#the access function gets the position of leaf that the deisred block is assgined to
#randomly assings the leaf node to a new location, and updates the position map
#function searches stash and tree for block
#updates block if found, or returns it if read only
#then pushes block and as many other blocks onto that path, starting at leaf

def Access(op,addr, data):
    #Remap Block
    x = PositionMap[addr]
    PositionMap[addr] = (numpy.random.randint(1,T.BNode.nodecount))

    #look for block in stash and 
    for node in PathMap[x]:#fore each node in the path
        buck = ReadBucket(node) #get the bbucket in the node
        for block in buck: #for eacch block in the bucket
            Stash.append(block) #add it to stash 
    
    for block in Stash:
            if block.addr == addr:
                 Block = block
                 break
    #write operations 
    if op == "w": 
         Block.changedata(data)
    #write back and pack in stash
        #for each level amoung the path, starting from root
        #   senddata = as many blocks that have the same path as x (path P(x) goe just before the leaf node, therefore it can find intersections)
        #   select either the full sendata list or select Z blocks from sendatat list
        #   remove sendatat from the stash
        #   write the senddata to the specified bucket
    for Node in PathMap[x]:
        SendData = [] #list that we will send over through writebucket
        SendData.clear()
        for block in Stash:
            leaf = block.leafmap
            if PathMap[leaf] == PathMap[x]:
                SendData.append(block)
                #Stash.remove(block)
        if len(SendData) > 4: #truncates it for a single bucket
            SendData = SendData[0:4]
        for item in SendData: #removes blocks from Stash
            Stash.remove(item)
        WriteBucket(Node)

        
    


### SERVER ACCESS FUNCTIONS ###

#read bucket
#blocks at a bucket are requested and read, decrypted
#then re-encrypted and sent back
#returns all blocks of given bucket
def ReadBucket(bucket)-> list[T.realBlock]:
    listl = []
    return listl

#write bucket
def WriteBucket(buckets):
     pass