#Rudy Chave 
#3/3/25
#file contains the access and setup functions for Path ORAM
from math import log2
import ReadCSV
import Tree as T
import numpy

###GLOBALS###

#will have X amount of blocks, and Z=4 block space per bucket therefore we need at least X/Z buckets, say 10 buckets
# we won't get 10 buckets but it will give us a tree that is fitting for this and the stash
# either way the height is still log2(10) as an integer
bucketHeight = int(log2(10)) #should be about 6
PositionMap = {} #Holds Blocks and which leaf node mapped to, so [B1] : Leaf X, [B2] : Leaf X
Stash = [] #list of buckets

#reads from Crimes12.csv and returns the blocks, each assigned leaves, evenly distributed
def ReadBlocks(bucketHeight, blocksWanted):
    leaves = 2**bucketHeight
    
    PositionMap = {}
    r = ReadCSV.datareader("Crimes12.csv")
    rawdata = r.readdata(10) #returns 10 byte strings
    blocks=[]
    blocks.clear()
    for row in rawdata:
        leaf=numpy.random.randint(1,leaves)
        B = T.realBlock(val=leaf,data=row)
        blocks.append(B)
        PositionMap[B.addr] = leaf

    
    return blocks,PositionMap
         

###SERVER OPERATION###

#setup, simply calls for the tree to be made using the Tree.py Otreaa.setup funciton
#input: block size output: object to the tree
#returs a tree function to be used, and a file to store the data
def Initialization(entrycardinality:int): #entryBsize, the size of the entries to the nearest power of 2, entrycardinality: the amount of entries to be proccesed
    Rpath = T.OTree(entrycardinality/4) #four blocks per bucket, therfore the total amount of blocks divded by four
    return Rpath


#CLIENT OPERATION

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
    PathMap = callPath(x) #also must search through STASH
    for node in PathMap:#fore each node in the path
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
    for Node in PathMap:
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
#decrypts and reads a given bucket, returns its blocks
def ReadBucket(bucket)-> list[T.realBlock]:
    listl = []
    return listl

#encryptes bucket data, and sends it to server to store along given bucket
#may need to edit input parameters
def WriteBucket(buckets):
     pass

#gets the entire path of x leaf, tells server to do a DFS search and return the entire path
def callPath(leafx:int):
     #calls server to have tree do getPath
     #see Tree.py -> Otree.getpath
     return []
     pass