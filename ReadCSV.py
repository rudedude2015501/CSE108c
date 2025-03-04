##Rudy Chavez CSE108c 2.12.25##

#this file is meant to make parsing and converting the snapshot of a csv file into blocks that can be accesed and read accross a clint,server model
#each tuple will be apart of a block of size Z which will be apart of a larger tuple for each Node in the Path ORAM impelmentation 

import csv
from os import close, read

# with open('Crimes12.csv') as csvfile:
#     reader = csv.reader(csvfile, delimiter=',')
#     i = 0
#     for row in reader:
#         print(', '.join(row))
#         print('\n')
#         i += 1
#         if i == 10:
#             break
##this little script just helps me make me more familiar with parsing through a csv file


#For now, each row of the csv file will be used for a single tuple in order ot make this easier
#this may change in order to accomodate easier search algoritihms such as a SEAL implemtation 

#reads the data from a given directory up to specified rows
class datareader():

    #oepns the csv file to be read
    def __init__(self, dir="") -> None:
        if (dir == ""):
            print("ERROR: INVALID PATH FORE CSV FILE\n")
            quit(-1)
        self.dir = dir
        self.index = 1
        
        pass

    #Either make a ge function that opens the file and keeps it open, or just use next on a local cvs.reader thing, because there may be some errors if
    #object is init but the file is not open

    #could be used to yeild infuvidual blocks
    # def gen_data(self):
    #     yield next(self.reader)
    
    #used mostly for init the tree
    #returns list of lists of the csv file, reads 10 at a time
    def readdata(self,rows=10):
        outlist = []
        with open(self.dir) as csvfile:
            reader = csv.reader(csvfile, delimiter=',')
            for i in range(self.index):
                next(reader)
            for i in range(rows):
                #make block object???? NAH JUST KEEP IT AS str or byte value
                outlist.append(next(reader)) #returns as list
                self.index += 1
        csvfile.close()
        return outlist

    # def readgen(self):
    #     with open(self.dir) as csvfile:
    #         reader = csv.reader(csvfile,delimiter=',')
    #         for l in range(self.index):
    #             next(reader)
    #         yield(next(reader))
    #     csvfile.close()
    #     self.index += 1
    
    
