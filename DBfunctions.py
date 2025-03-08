#Rudy Chave 
#3/3/25
#file contains the access and setup functions for Path ORAM
# DBfunction.py
from math import log2
import ReadCSV
import Tree as T
import numpy
from server import Server
from Encryption_funcs import encscheme 
import ast  # for safely converting string to list
from ReadCSV import datareader
from Tree import OTree, BNode, realBlock
###GLOBALS###
enc = encscheme() # initialize encscheme()
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
def Initialization(totalBlocks:int): #entryBsize, the size of the entries to the nearest power of 2, entrycardinality: the amount of entries to be proccesed
    Rpath = T.OTree(totalBlocks/4) #four blocks per bucket, therfore the total amount of blocks divded by four
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
    
    for block in Stash: #seraches stash along with requested path
            if block.addr == addr:
                Block = block
                break
            else:
                Block = None
    if Block == None: #if block is not found in stash or tree
        return -1
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
def ReadBucket(node) -> list[T.realBlock]:
    """
    Reads and decrypts blocks from the given node's bucket.

    :param node: The node containing the bucket to read from
    :return: List of realBlock objects
    """
    enc.initcipher()  # Initialize encryption

    # Fetch serialized bucket data from the server
    server_instance = Server()  # Create a server instance

    serialized_bucket = server_instance.get(node.nodeID)

    
    if not serialized_bucket:  # Handle empty or missing bucket
        return []

    # Deserialize the bucket
    bucket = node.deserialize_bucket(serialized_bucket)

    # Decrypt each block and return the list of decrypted realBlock objects
    decrypted_blocks = []
    for block in bucket.blocks:
        decrypted_data = enc.decrypt(block.data)
        if decrypted_data is not None:
            decrypted_blocks.append(realBlock(data=decrypted_data))
    
    return decrypted_blocks
    

#encryptes bucket data, and sends it to server to store along given bucket
#may need to edit input parameters
def WriteBucket(node, SendData):
    enc.initcipher()  # Initialize encryption
    server_instance = Server() 
    # Fill the bucket with the new SendData while maintaining the limit
    node.bucket.clear()
    for block in SendData[:BNode.blocknum]:  # Ensure we only store up to blocknum
        encrypted_data = enc.encrypt(block.data)
        block.data = encrypted_data
        node.bucket.append(block)
    
    # Serialize the updated bucket
    serialized_bucket = node.serialize_bucket()
    
    # Store the bucket data on the server
    server_instance.set(node.nodeID, serialized_bucket)

def callPath(leaf: int, tree: OTree) -> list[BNode]:
    """Fetch path using OTree's structure."""
    return tree.get_path(leaf)


