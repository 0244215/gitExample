import socket, sys

host = "192.168.51.1"  # <-- tu IP del servidor
port = 5000
msg = "Hola soy Zhivago"

with socket.create_connection((host, port)) as s:
    s.sendall(msg.encode())
    print("Message sent:", msg)
    data = s.recv(1024)
    print("Received:", data.decode())
