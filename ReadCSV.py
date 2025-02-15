##Rudy Chavez CSE108c 2.12.25##

#this file is meant to make parsing and converting the snapshot of a csv file into blocks that can be accesed and read accross a clint,server model
#each tuple will be apart of a block of size Z which will be apart of a larger tuple for each Node in the Path ORAM impelmentation 

import csv

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
        self.index = 0
        self.reader = csv.reader(self.dir, delimiter=',')
        pass

    #reads the first x rows default of 10
    def readdata(self,rows=10):
        with open(self.dir) as csvfile:
            