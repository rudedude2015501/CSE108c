from platform import node
from tkinter import W
import base64
import os
import json

from numpy import block
import Tree
#block_size = 16
#NEED 2 ^ 9 bytes per data
def pad(data: bytes, block_size: int = 16) -> bytes:
    padding_length = block_size - (len(data) % block_size)
    # calculates the number of padding bytes required
    padding = bytes([padding_length] * padding_length)
    # we multiply the padding length by the padding length to get the padding
    # creates a list where padding_length is repeated padding_length times
    return data + padding

def unpad(data: bytes) -> bytes:
    padding_length = data[-1]
    # the last byte is the value of the padding length, so we only read up to - padding_length
    return data[:-padding_length]

class Node:
    def __init__(self, block_id: int, data: bytes, leaf_label: int):
        self.block_id = block_id
        self.data = data
        self.leaf_label = leaf_label

def to_bytes(node) -> bytes:
    """
    Serializes a Node object into bytes using JSON and Base64.
    """
    # Convert the node to a dictionary
    node_dict = {
        'block_id': node.block_id,
        'data': base64.b64encode(node.data).decode('utf-8'),  # Encode binary data as Base64
        'leaf_label': node.leaf_label
    }
    # Serialize the dictionary to JSON and encode as bytes
    return json.dumps(node_dict).encode('utf-8')

def list_to_bytes(Blist) -> bytes:
    """
    Serializes a Node object into bytes using JSON and Base64.
    """
    # Convert the node to a dictionary
    block_list = [obj.__dict__ for obj in Blist]
    for d in block_list:
        d['data'] = base64.b64encode(d['data']).decode('utf-8')
    # Serialize the dictionary to JSON and encode as bytes
    return json.dumps(block_list).encode('utf-8')

def from_bytes(data: bytes) -> Node:
    """
    Deserializes bytes into a Node object using JSON and Base64.
    """
    # Decode the JSON string
    node_dict = json.loads(data.decode('utf-8'))
    # Decode the Base64-encoded data
    node_data = base64.b64decode(node_dict['data'])
    return Node(node_dict['block_id'], node_data, node_dict['leaf_label'])

def list_from_bytes(data: bytes):
    """
    Deserializes bytes into a Node object using JSON and Base64.
    """
    # Decode the JSON string
    block_list = json.loads(data.decode('utf-8'))
    retlist = []
    # Decode the Base64-encoded data
    for d in block_list:
        d['data'] = base64.b64decode(d['data'])
        retlist.append(Tree.realBlock(d['addr'],d['leafmap'],d['data']))
    
    return retlist

#tested and works!!
