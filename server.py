# server.py file
import socket
import struct
from messages import Node, to_bytes, from_bytes
from Tree import OTree, BNode, realBlock
from Encryption_funcs import encscheme
import numpy
import ast

class Server:
    def __init__(self, host='127.0.0.1', port=65432):
        self.host = host
        self.port = port
        print(f"Starting server on {self.host}:{self.port}")
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.bind((self.host, self.port))
        self.server_socket.listen(1)
        print(f"Server listening on {self.host}:{self.port}")
        self.tree = OTree(BucketNumber=10)
        self.encrypted_storage = {}
        self.enc = encscheme()  # Initialize encryption scheme

    def get(self, bucket_id: int):
        return self.encrypted_storage.get(bucket_id, None)

    def set(self, bucket_id: int, encrypted_data: bytes):
        """Stores encrypted bucket data."""
        self.encrypted_storage[bucket_id] = encrypted_data

    def handle_client(self, client_socket):
        """Handles client communication."""
        try:
            print("Client connected. Waiting for data...")
            while True:
                # Receive message length
                message_length_bytes = client_socket.recv(4)
                if not message_length_bytes:
                    print("Client disconnected.")
                    break
                
                message_length = struct.unpack('!I', message_length_bytes)[0]
                print(f"Receiving message of length {message_length} bytes...")
                
                # Receive the full message
                message = b''
                while len(message) < message_length:
                    chunk = client_socket.recv(message_length - len(message))
                    if not chunk:
                        print("Connection lost while receiving message.")
                        return
                    message += chunk
                
                print(f"Received message: {message}")
                
                # Decode the received message (this is an example, and you would likely need to implement this)
                request = ast.literal_eval(message.decode('utf-8'))
                operation = request['operation']
                addr = request['addr']
                data = request.get('data', None)

                if operation == 'read':
                    result = self.ReadBucket(addr)
                elif operation == 'write':
                    result = self.WriteBucket(addr, data)
                else:
                    result = 'Invalid operation'

                # Send back the response
                response = str(result).encode('utf-8')
                response_length = struct.pack('!I', len(response))
                client_socket.send(response_length + response)

        except Exception as e:
            print(f"Error: {e}")
        finally:
            client_socket.close()
            print("Client connection closed.")

    def start(self):
        print("Server started. Waiting for client connections...")
        while True:
            client_socket, addr = self.server_socket.accept()
            print(f"Connected to client at {addr}")
            self.handle_client(client_socket)

    # Read the bucket from storage and decrypt
    def ReadBucket(self, bucket_id):
        encrypted_data = self.get(bucket_id)
        if not encrypted_data:
            return "No data found"
        self.enc.initcipher()  # Ensure cipher is initialized
        decrypted_data = self.enc.decrypt(encrypted_data)  # Decrypt the bucket
        return ast.literal_eval(decrypted_data.decode())  # Convert back to list of blocks

    # Encrypt and write the bucket to storage
    def WriteBucket(self, bucket_id, data):
        self.enc.initcipher()  # Ensure cipher is initialized
        encrypted_data = self.enc.encrypt(str(data).encode())  # Encrypt bucket data
        self.set(bucket_id, encrypted_data)  # Store encrypted data
        return "Data written successfully"


# To start the server
if __name__ == "__main__":
    server = Server()
    server.start()
