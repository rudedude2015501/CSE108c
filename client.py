import socket
import struct
from block_cipher import Node, to_bytes, from_bytes
class Client:
    def __init__(self, host='127.0.0.1', port=65432):
        self.host = host
        self.port = port
        self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.client_socket.connect((self.host, self.port))

    def send_node(self, node):
        """
        Sends a Node object to the server and receives a response.
        """
        try:
            # Serialize the node into bytes
            node_bytes = to_bytes(node)

            # Send the length of the message first
            self.client_socket.send(struct.pack('!I', len(node_bytes)))

            # Send the serialized node
            self.client_socket.send(node_bytes)

            # Receive the length of the response
            response_length_bytes = self.client_socket.recv(4)
            response_length = struct.unpack('!I', response_length_bytes)[0]

            # Receive the response
            response_bytes = self.client_socket.recv(response_length)

            # Deserialize the response into a Node object
            return from_bytes(response_bytes)
        except Exception as e:
            print(f"Error: {e}")
            return None

    def close(self):
        """
        Closes the client connection.
        """
        self.client_socket.close()

if __name__ == "__main__":
    client = Client()

    # Create a node
    node = Node(block_id=123, data=b"hello, world!", leaf_label=456)

    # Send the node to the server
    response_node = client.send_node(node)
    print("Server response:", response_node)

    client.close()