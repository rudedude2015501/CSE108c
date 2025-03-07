#Rudy Chavez 3/6/25
#meant to create and implement the Path ORAM being adjustable
#also stores function used for creating leackage within a data block

from math import log2
import Tree as T
#Object to make the adjustable ORAM
class AdjORAM():
    BucketSize = 4
    BitsLeaked = 0
    def __init__(self,BitsToLeak:int = 0, ValuesToStore:int =30) -> None:
        #for now, just to get a bare minimum going
        #stats broken down by bits to leak (stats per PathORAM)
        AdjORAM.BitsLeaked = BitsToLeak
        self.ValPerLeak = ValuesToStore/ (2**BitsToLeak)
        self.BucketsPerTree = self.ValPerLeak/AdjORAM.BucketSize
        #sizes determined, now we make a PathORAM instance for 2**bits leaked
        Orams = list()
        for i in range(0,2**BitsToLeak): #makes the list of buckets for 2**a leaked bits
            Orams.append(T.OTree(self.BucketsPerTree))
        self.ORAMS = Orams
    #gets the path using the adj bitval, need to do some additonal parcing to find the bit
    def getPath(self, leaf:int, node:T.BNode,bitval:int):
        oram = self.ORAMS[bitval]
        return oram.getpath(leaf,node)
    
#Object to compile blocks to include the leaked bits
class AdjBlocks(T.realBlock):
    def __init__(self, addr=0, val=0, data: bytes = b"",bitval:int = 0) -> None:
        super().__init__(addr, val, data)
        self.bitval = bitval

#parse info such that we can find the adjustable bit at MSB
def splicebits():
    
    pass