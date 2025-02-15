##Rudy Chavez CSE108c 2.12.25##
import csv

with open('Crimes12.csv') as csvfile:
    reader = csv.reader(csvfile, delimiter=',')
    i = 0
    for row in reader:
        print(', '.join(row))
        print('\n')
        i += 1
        if i == 10:
            break
##this little script just helps me make me more familiar with parsing through a csv file

