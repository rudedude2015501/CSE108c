##Rudy Chavez##
#2/13/24#
#File holding encryption functions for the proejct#

from tkinter import W
import cryptography
import os

import cryptography.hazmat
import cryptography.hazmat.primitives
import cryptography.hazmat.primitives.ciphers
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

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


#generates key and iv for the encryption module and saves them for the user to secure
def generate_key(dir="keys.txt"):
    
    key = os.urandom(32) #will use key and IV for now
    iv = os.urandom(16)
    f = open(dir,W) #open file where keys will be saved, this must be made secure in a later interation
    f.write(str(key) + "\n")
    f.write(str(iv) + "\n\n")
    f.close
    
    return key, iv

# function that initalizes the cipher to the keys and iv
# Note: we may also place initcipher(dir="keys.txt")
##returns and encrypt and decrypt object from the cryptography library
def initcipher(key,iv): #need a func to read key and iv to place into function input
    cipher = Cipher(algorithms.AES(key), modes.CBC(iv))
    encryptor = cipher.encryptor()
    decryptor = cipher.decryptor()
    return encryptor,decryptor


#message text must be a multiple of the block size
def encrypt(messagetxt: bytes, encryptor: cryptography.hazmat.primitives.ciphers.CipherContext):
    #make function that ads some urandom padding based off of if the message is a multiple of the block size found in cipher
    ciphertext = encryptor.update(messagetxt) + encryptor.finalize()
    return ciphertext

def dencrypt(ciphertxt: bytes, decryptor: cryptography.hazmat.primitives.ciphers.CipherContext):
    messagetxt = decryptor.update(ciphertxt) + decryptor.finalize()
    return messagetxt

key,iv = generate_key()

enc,dec = initcipher(key,iv)

st = encrypt(b"a secret message", enc)
print(st)
dt = dencrypt(st,dec)
print(dt)
