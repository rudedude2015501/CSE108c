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

    def handle_client(self, client_socket):
        try:
            print("Client connected. Waiting for data...")
            while True:
                # Receive the length of the incoming message
                message_length_bytes = client_socket.recv(4)
                if not message_length_bytes:
                    print("Client disconnected.")
                    break

                # Unpack the message length
                message_length = struct.unpack('!I', message_length_bytes)[0]
                print(f"Receiving message of length {message_length} bytes...")

                # Receive the actual message
                message = client_socket.recv(message_length)
                if not message:
                    print("No data received from client.")
                    break

                print(f"Received message from client: {message}")

                # Echo the message back to the client
                print("Sending response back to client...")
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