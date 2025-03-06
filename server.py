import socket
import struct
import os
from messages import Node, to_bytes, from_bytes
from Tree import OTree, BNode, realBlock
class Server:
    def __init__(self, host='127.0.0.1', port=65432):
        self.host = host
        self.port = port
        print(f"Starting server on {self.host}:{self.port}")
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.bind((self.host, self.port))
        self.server_socket.listen(1)
        print(f"Server listening on {self.host}:{self.port}")
         # Initialize the tree with dummy blocks
        self.tree = OTree(BucketNumber=10)
        self.encrypted_storage = {}
    def get(self, bucket_id: int):
        return self.encrypted_storage.get(bucket_id, None)
    def store(self, bucket_id: int, encrypted_data: bytes):
        """
        Stores the encrypted bucket data on the server.
        :param bucket_id: The ID of the bucket.
        :param encrypted_data: The encrypted data (bytes).
        """
        self.encrypted_storage[bucket_id] = encrypted_data
    def write_bucket(self, bucket_id, encrypted_data):
        self.store(bucket_id, encrypted_data)
    def read_bucket(self, bucket_id):
        return self.get(bucket_id)
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
                
                # Echo back to client
                client_socket.send(message_length_bytes + message)
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

if __name__ == "__main__":
    server = Server()
    server.start()