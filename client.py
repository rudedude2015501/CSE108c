import socket
import struct
import json
from Tree import realBlock

class Client:
    def __init__(self, host='127.0.0.1', port=65432):
        self.host = host
        self.port = port
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.connect((self.host, self.port))

    def _recv_all(self, sock, n):
        """Helper to read exactly n bytes from socket"""
        data = bytearray()
        while len(data) < n:
            packet = sock.recv(n - len(data))
            if not packet:
                return None
            data.extend(packet)
        return data

    def send_request(self, operation: str, bucket_id: str, data=None):
        # Serialize only necessary fields
        if data:
            serialized_data = []
            for block in data:
                serialized_block = {
                    'addr': block.addr,
                    'leafmap': block.leafmap,
                    'data': block.data.hex()  # Convert bytes to hex string for JSON
                }
                serialized_data.append(serialized_block)
            request_data = json.dumps(serialized_data)
        else:
            request_data = None

        request = {
            'operation': operation,
            'bucket_id': bucket_id,
            'data': request_data
        }
        request_bytes = json.dumps(request).encode()
        
        # Send request
        self.socket.send(struct.pack('!I', len(request_bytes)))  # Length first
        self.socket.send(request_bytes)                         # Then data

        # Get response
        raw_resplen = self._recv_all(self.socket, 4)
        if not raw_resplen:
            raise ConnectionError("Server disconnected")
        resp_len = struct.unpack('!I', raw_resplen)[0]
        return self._recv_all(self.socket, resp_len)

    def close(self):
        self.socket.close()

def test_path_oram():
    client = Client()
    
    blocks = [realBlock(addr=i, leafmap=i%16+1, data=f"Data{i}".encode()) 
              for i in range(1, 11)]
    
    # Test write
    for block in blocks:
        client.send_request('write', str(block.leafmap), [block])  # Changed .leafmap to .val
    
    # Test read
    response = client.send_request('read', '5')
    print("Blocks in leaf 5:", json.loads(response.decode()))
    
    client.close()

if __name__ == "__main__":
    test_path_oram()