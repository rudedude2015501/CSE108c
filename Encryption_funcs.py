##Rudy Chavez##
#2/13/24#
#File holding encryption functions for the proejct#

from tarfile import NUL
from tkinter import W
import cryptography
import os

import cryptography.hazmat
import cryptography.hazmat.primitives
import cryptography.hazmat.primitives.ciphers
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes, BlockCipherAlgorithm

#may use given cryptography algorithim given in the library
#cryptography.hazmat.primitives.ciphers.algorithms.AES
#use os for key and store it in a text file client side
# block = row xored w/ PRF(r) || r and k is secret

#for AES: generate key, save key to file, and test with it

# key = os.urandom(32)
# iv = os.urandom(16)
# cipher = Cipher(algorithms.AES(key), modes.CBC(iv))
# encryptor = cipher.encryptor()
# ct = encryptor.update(b"a secret message") + encryptor.finalize()
# print(ct)
# decryptor = cipher.decryptor()
# a = decryptor.update(ct) + decryptor.finalize()
# print(a)
class encscheme():

    
    def __init__(self, dir="keys.txt") -> None:
        self.key,self.iv = generate_key(dir)
    
    # function that initalizes the cipher to the keys and iv
    # Note: we may also place initcipher(dir="keys.txt")
    ##returns and encrypt and decrypt object from the cryptography library
    def initcipher(self): #need a func to read key and iv to place into function input
        self.cipher = Cipher(algorithms.AES(self.key), modes.CBC(self.iv))
        
        self.encryptor = self.cipher.encryptor()
        self.decryptor = self.cipher.decryptor()
        
    #message text must be a multiple of the block size
    def encrypt(self, messagetxt: bytes):
        #make function that ads some urandom padding based off of if the message is a multiple of the block size found in cipher
        ciphertext = self.encryptor.update(messagetxt) + self.encryptor.finalize()
        return ciphertext

    def dencrypt(self, ciphertxt: bytes):
        messagetxt = self.decryptor.update(ciphertxt) + self.decryptor.finalize()
        return messagetxt

#generates key and iv for the encryption module and saves them for the user to secure
def generate_key(dir="keys.txt"):
    
    key = os.urandom(32) #will use key and IV for now
    iv = os.urandom(16)
    f = open(dir,W) #open file where keys will be saved, this must be made secure in a later interation
    f.write(str(key) + "\n")
    f.write(str(iv) + "\n\n")
    f.close
    
    return key, iv



# key,iv = generate_key()

# enc,dec = initcipher(key,iv)

# st = encrypt(b"a secret message", enc)
# print(st)
# dt = dencrypt(st,dec)
# print(dt)

