import socket
import struct
import json
import ast
from Tree import OTree, BNode, realBlock
from Encryption_funcs import encscheme

class Server:
    def __init__(self, host='127.0.0.1', port=65432):
        self.host = host
        self.port = port
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.bind((self.host, self.port))
        self.server_socket.listen(1)
        self.tree = OTree(BucketNumber=10)  # Initialize OTree with 10 buckets
        self.encrypted_storage = {}
        self.enc = encscheme()

    def get(self, bucket_id: bytes) -> bytes:
        return self.encrypted_storage.get(bucket_id, None)

    def set(self, bucket_id: bytes, encrypted_data: bytes):
        self.encrypted_storage[bucket_id] = encrypted_data

    def handle_client(self, client_socket):
        try:
            while True:
                # Read message length
                raw_msglen = self._recv_all(client_socket, 4)
                if not raw_msglen:
                    break
                msglen = struct.unpack('!I', raw_msglen)[0]

                # Read message data
                request = json.loads(self._recv_all(client_socket, msglen).decode())

                # Process request
                if request['operation'] == 'read':
                    bucket_id = request['bucket_id'].encode()
                    encrypted_data = self.get(bucket_id)
                    response = encrypted_data if encrypted_data else b''
                elif request['operation'] == 'write':
                    bucket_id = request['bucket_id'].encode()
                    encrypted_data = request['data'].encode()
                    self.set(bucket_id, encrypted_data)
                    response = b'OK'
                else:
                    response = b'Invalid operation'

                # Send response
                client_socket.send(struct.pack('!I', len(response)))  # Length first
                client_socket.send(response) 

        except Exception as e:
            print(f"Error: {e}")
        finally:
            client_socket.close()

    def ReadBucket(self, bucket_id: bytes):
        encrypted_data = self.get(bucket_id)
        if not encrypted_data:
            return []
        self.enc.initcipher()
        decrypted = self.enc.decrypt(encrypted_data)
        # Deserialize using leafmap parameter
        return [realBlock(**b) for b in json.loads(decrypted.decode())]

    def WriteBucket(self, bucket_id: bytes, blocks: list[realBlock]):
        self.enc.initcipher()
        data = json.dumps([b.__dict__ for b in blocks]).encode()
        encrypted = self.enc.encrypt(data)
        self.set(bucket_id, encrypted)
    def start(self):
        while True:
            client_socket, addr = self.server_socket.accept()
            print(f"Connection from {addr}")
            self.handle_client(client_socket)
    def _recv_all(self, sock, n):
        """Helper to receive exactly n bytes from socket"""
        data = bytearray()
        while len(data) < n:
            packet = sock.recv(n - len(data))
            if not packet:
                return None
            data.extend(packet)
        return data

if __name__ == "__main__":
    server = Server()
    server.start()