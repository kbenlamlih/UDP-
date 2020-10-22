#!/usr/bin/env python

__author__      = 'Radoslaw Matusiak - Modified by Tim719'
__copyright__   = 'Copyright (c) 2016 Radoslaw Matusiak'
__license__     = 'MIT'
__version__     = '0.1'


"""
TCP/UDP proxy.
"""

import argparse
import logging
import select
import socket
import threading
from random import random as rnd
from numpy import random
from time import sleep

FORMAT = '%(asctime)-15s %(levelname)-10s %(message)s'
logging.basicConfig(format=FORMAT)
LOGGER = logging.getLogger()

g_src = ""
g_dst = ""
data_port = 0
loss_percentage = 0.0
mock_delay = 0.0
stop_thread = False

data_thread = None

def LOCAL_DATA_HANDLER(data, address):
    ip, port = address
    LOGGER.debug('[{}:{}] Src -> Dst: {}'.format(ip, port, data))

    if data == b'FIN\n':
        global stop_thread
        stop_thread = True
        LOGGER.info("Stopping listening threads")

    return data

def REMOTE_DATA_HANDLER(data, address):
    ip, port = address
    LOGGER.debug('[{}:{}] Dst -> Src: {}'.format(ip, port, data))

    if b'SYN-ACK-' in data:
        global g_src, g_dst, data_thread, stop_thread, loss_percentage

        port = data.split(b'-')[2].strip()

        dst_address, *_ = g_dst.split(':')
        src_address, *_ = g_src.split(':')

        data_dst = dst_address + ":" + port.decode(encoding='utf-8')
        data_src = src_address + ":" + data_port

        LOGGER.info('Running new Proxy thread between Src {} and Dst {}'.format(data_src, data_dst))

        data_thread = threading.Thread(target=udp_proxy, args=(data_src, data_dst, loss_percentage))
        data_thread.start()

        forged_syn_ack = "SYN-ACK-" + str(data_port) + "\n"
        return forged_syn_ack.encode(encoding='utf-8')
    return data

BUFFER_SIZE = 2 ** 10  # 1024. Keep buffer size as power of 2.


def udp_proxy(src, dst, loss_percentage=0.0):
    """Run UDP proxy.
    
    Arguments:
    src -- Source IP address and port string. I.e.: '127.0.0.1:8000'
    dst -- Destination IP address and port. I.e.: '127.0.0.1:8888'
    """

    global stop_thread, mock_delay

    LOGGER.debug('Starting UDP proxy...')
    LOGGER.debug('Src: {}'.format(src))
    LOGGER.debug('Dst: {}'.format(dst))

    if loss_percentage > 0:
        LOGGER.info("Starting proxy with loss percentage set to %.2f" % loss_percentage)
    
    if mock_delay > 0:
        LOGGER.info("Mean mock delay set to %.2f" % mock_delay)
    
    proxy_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    proxy_socket.bind(ip_to_tuple(src))
    
    client_address = None
    server_address = ip_to_tuple(dst)
    
    LOGGER.debug('Looping proxy (press Ctrl-Break to stop)...')

    while not stop_thread:
        data, address = proxy_socket.recvfrom(BUFFER_SIZE)

        if mock_delay > 0:
            delay = random.exponential(scale=mock_delay)
            LOGGER.debug("Sleeping for %.4fs" % delay)
            sleep(delay)
        
        if client_address == None:
            client_address = address

        if address == client_address:
            data = LOCAL_DATA_HANDLER(data, address)
            if loss_percentage <= rnd():
                proxy_socket.sendto(data, server_address)
            else:
                LOGGER.info("----- Discarding packet -----")
        elif address == server_address:
            data = REMOTE_DATA_HANDLER(data, address)
            proxy_socket.sendto(data, client_address)
            client_address = None
        else:
            LOGGER.warning('Unknown address: {}'.format(str(address)))
    
    LOGGER.info("End of thread from {} to {}".format(src, dst))
    proxy_socket.close()  


def ip_to_tuple(ip):
    """Parse IP string and return (ip, port) tuple.
    
    Arguments:
    ip -- IP address:port string. I.e.: '127.0.0.1:8000'.
    """
    ip, port = ip.split(':')
    return (ip, int(port))


def main():
    global g_src, g_dst, data_port, data_thread, loss_percentage, mock_delay
    """Main method."""
    parser = argparse.ArgumentParser(description='UPD proxy.')
    
    parser.add_argument('-s', '--src', required=True, help='Source IP and port, i.e.: 127.0.0.1:8000. This should be the ip:port exposed to the client.')
    parser.add_argument('-d', '--dst', required=True, help='Destination IP and port, i.e.: 127.0.0.1:8888. This should be the ip:port to connect to the server.')
    parser.add_argument('-p', '--port', required=True, help='Port that will be provided to the client for data connection.')
    parser.add_argument('-L', '--loss-percentage', required=False, type=float, help='Loss percentage, i.e : 0.5 if you want a 50%% packet loss.', default=0.0)
    parser.add_argument('-D', '--delay', required=False, type=float, default=0.0, help='Delay (as exponential law parameter), i.e : 0.5 if you want an average delay of 0.5s')
    
    args = parser.parse_args()
    
    LOGGER.setLevel(logging.NOTSET)

    g_src = args.src
    g_dst = args.dst
    data_port = args.port
    loss_percentage = args.loss_percentage
    mock_delay = args.delay
    
    udp_proxy(g_src, g_dst, loss_percentage=0.0)

    if data_thread is not None:
        LOGGER.info("Waiting for thread to join")
        data_thread.join()  


if __name__ == '__main__':
    main()