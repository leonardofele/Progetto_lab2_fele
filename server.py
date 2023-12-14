#!/usr/bin/env python3

import argparse
from encodings import utf_8
import os
import struct
import socket
import signal
import threading
import subprocess
import logging
import concurrent.futures



HOST = "127.0.0.1"
PORT = 58477

Description = "Server per il progetto"

logging.basicConfig(filename='server.log',
                    level=logging.DEBUG, datefmt='%d/%m/%y %H:%M:%S',
                    format='%(asctime)s - %(levelname)s - %(message)s')


def recv_all(conn, n):
    # Funzione per ricevere tutti i byte
    chunks = b''
    bytes_recd = 0
    while bytes_recd < n:
        # Riceve blocchi di al più 1024 byte
        chunk = conn.recv(min(n - bytes_recd, 1024))
        if len(chunk) == 0:
            raise RuntimeError("Connessione socket interrotta")
        chunks += chunk
        bytes_recd = bytes_recd + len(chunk)
    return chunks


def handle_client(client_socket, log_lock,type_lock, caposec, capolet): 
    # Gestione del client
    # manda riferimento al codice per le pipe
    try:
        total_bytes = 0
        data = recv_all(client_socket,4)
        log_type = struct.unpack("!i",data)[0] # per la gestione dei tipi di connessione ho usato la convezione che se è 0 allora è di tipo A se è 1 è di tipo B 
        if log_type == 0:
            log_type = "Tipo A"
        else: log_type = "Tipo B"
        with log_lock:
            logging.info(f"Connessione  di Tipo: {log_type}")
        if log_type == "Tipo A":
            with type_lock:
                data0 = recv_all(client_socket, 4)
                len_sequenza = struct.unpack("!i",data0)[0]
                data = recv_all(client_socket, len_sequenza)
               
                os.write(capolet,data0)
                os.write(capolet,data)
                total_bytes = len_sequenza
        elif log_type == "Tipo B":
            with type_lock:
                while True:
                    data0 = recv_all(client_socket, 4)
                    len_sequenza = struct.unpack("!i",data0)[0]
                    
                    if len_sequenza == 0:
                        break
                    data = recv_all(client_socket, len_sequenza)
                    os.write(caposec,data0)
                    os.write(caposec,data)
                    total_bytes += len_sequenza
                client_socket.sendall(struct.pack("!i",total_bytes)) # mando al client 
        log_msg = f"{log_type}: {total_bytes} bytes"
        with log_lock:
            logging.info(log_msg)
    except Exception as e:
        logging.error(f"Errore nella gestione del client: {e}") # gestisco gli errori facendo stampare {e} come errore nel file.log
    finally:
        client_socket.close()

def main(t,v,r,w,host=HOST,port=PORT):

    # apro le pipe
    try:
        os.mkfifo("caposc")
    except FileExistsError:
        pass
    try:
        os.mkfifo("capolet")
    except FileExistsError:
        pass
        
    #avvio archivio
    str_w = str(w)
    str_r = str(r)
    command =["./archivio", str_w,str_r]
    if v:
        command = ["valgrind", "--leak-check=full", "--show-leak-kinds=all", "--log-file=valgrind-%p.log" ,"./archivio", str_w,str_r]
    process = subprocess.Popen(command)
    
    caposc = os.open("caposc",os.O_WRONLY )    
    capolet = os.open("capolet",os.O_WRONLY)
    
    log_lock = threading.Lock() # è una lock che uso per scrivere nei file di log in modo da sincronizzare il tutto e non avere problemi
    Type_lock = threading.Lock() # questa è la lock per il server 
    #creo un pull di processi
    with concurrent.futures.ThreadPoolExecutor(max_workers=t) as executor:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            try:
                s.bind((host, port))
                s.listen()
                #logging.info(f"Server in ascolto su {host}, {port}")
                while True:
                    cliet, addr = s.accept()
                    executor.submit(handle_client, cliet, log_lock, Type_lock, caposc, capolet) #avvio i tread 
            except KeyboardInterrupt:
                #logging.info("Terminazione del server.")
                s.shutdown(socket.SHUT_RDWR)
            process.send_signal(signal.SIGTERM)
            #os.kill(process.pid, signal.SIGTERM)

            if os.path.exists("caposc"):
                os.close(caposc)
                os.unlink('caposc')
            if os.path.exists("capolet"):
                os.close(capolet)
                os.unlink('capolet')
            
            while True:
                if process.poll()!=None:break # qui aspetto archivio perchè vogio che termini prima archivio che il server
                
                

#Gestione argomenti
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=Description, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('t', help='numero di thread', type = int)  
    parser.add_argument('-r', help='lettori', type = int, default=3)
    parser.add_argument('-w', help='scrittori', type = int,default=3)
    parser.add_argument('-a', help='host address', type = str, default=HOST)  
    parser.add_argument('-p', help='port', type = int, default=PORT)
    parser.add_argument('-v', action='store_true',help='Esegui archivio con Valgrind')  
    args = parser.parse_args() # Fa partire il parser
    main(args.t,args.v,args.r,args.w,args.a,args.p)