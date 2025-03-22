# CSE108c
CSE108c UCSC Path ORAM and SEAL implementation 

Block_cipher.py = creates functions to add the padding to the bytes and a Node class for the ORAM implementation. It also includes a function to take a node object into bytes using JSON and base64

client.py = creates the client class 

server.py = creates a server class

Encryption_funcs.py = file holding encryption functions for the project

Node.py = to create a full working binary tree

readccsv.py =  file is meant to make parsing and converting the snapshot of a CSV file into blocks that can be accessed and read across a client,server model

SEAL implementation

How to run - go into inout-oram copy

1.Go into seal directory 

2. run g++ -o test_qrsr seal_attacks.cpp -I.. -I../core -I../threadpool -I../utils -I./spdlog/include -I/opt/homebrew/opt/openssl@3/include -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto -std=c++17
  
4. ./test_qrsr 




