#! python
# -*- coding: utf-8 -*-
#
# This file is part of the muiltprocessing distribution.
# Copyright (c) 2023 zero.kwok@foxmail.com
# 
# This is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 3 of
# the License, or (at your option) any later version.
# 
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
# 
# You should have received a copy of the GNU Lesser General Public
# License along with this software; 
# If not, see <http://www.gnu.org/licenses/>.

import os
import zmq
import time
import argparse
import threading
import traceback
import signal 

class iothread:
    def __init__(self, uid:str):
        self.uid = uid
        self.thread = None
        self.publish = None
        self.interrupted = False

        self.context = zmq.Context()
        self.zsub  = self.context.socket(zmq.SUB)
        self.zreq  = self.context.socket(zmq.REQ)
        self.zpush = self.context.socket(zmq.PUSH)

        def InitSocket(socket, uid):
            socket.identity = uid
            socket.setsockopt(zmq.LINGER, 500)

        uid = uid.encode()
        InitSocket(self.zsub, uid)
        InitSocket(self.zreq, uid)
        InitSocket(self.zpush, uid) 

    def run(self):
        self.interrupted = False
        self.thread = threading.Thread(target=self.__thread, name="run")
        self.thread.start()
        
    def join(self):
        self.thread.join()
        
    def shutdown(self):
        self.interrupted = True
        self.zsub.close()
        self.zreq.close()
        self.zpush.close()
        self.context.term()

    def push(self, msg:bytes):
        if self.zpush:
            self.zpush.send(msg)
            
    def subscribe(self, subject:str, callback):
        if self.zsub:
            self.publish = callback
            self.zsub.setsockopt_string(zmq.SUBSCRIBE, subject)

    def request(self, msg:bytes):
        if not self.zreq:
            raise RuntimeError()
        self.zreq.send(msg)
        return self.zreq.recv()

    def __thread(self):
        try:
            poll = zmq.Poller()
            poll.register(self.zsub, zmq.POLLIN)
            
            while not self.interrupted:
                for socket, event in poll.poll(50):
                    msg = socket.recv()
                    if socket == self.zsub and self.publish:
                        self.publish(msg)
        except Exception as e:
            if not self.interrupted:
                print ("Exception:", e)

# 阻止因为跟父进程共用一个终端, 并且终端触发SIGBREAK信号时, 被一起中断的情况
def handle_sigbreak(sig, frame):  
    print('SIGBREAK signal!') 
    signal.signal(signal.SIGBREAK, handle_sigbreak) 
signal.signal(signal.SIGBREAK, handle_sigbreak) 

if __name__ == "__main__":
    try:
        parser = argparse.ArgumentParser()
        parser.add_argument('--uid', action='store', required=True)
        parser.add_argument('--req', action='store', required=True)
        parser.add_argument('--sub', action='store', required=True)
        parser.add_argument('--push', action='store', required=True)

        # args = parser.parse_args('--uid "62a0d894-0625-4922-a154-6e48624733f8" --req "tcp://localhost:5555" --sub "tcp://localhost:5556" --push "tcp://localhost:5557"'.split())
        args = parser.parse_args()
        for key in args.__dict__:
            if type(args.__dict__[key]) == str:
                args.__dict__[key] = args.__dict__[key].strip(' \'"')

        keepgoing = True
        def publish(msg:bytes):
            global keepgoing
            msg = msg.decode()
            if msg == 'STOP' and keepgoing:
                print('STOP: stopping...')
                keepgoing = False

        io = iothread(args.uid)
        io.zreq.connect(args.req) 
        io.zsub.connect(args.sub) 
        io.zpush.connect(args.push)
        io.subscribe('', publish)
        io.run()
        io.push(f'READY'.encode())
        
        index = 0
        while keepgoing:
            index += 1
            io.push(f'inxex {index}'.encode())
            
            time.sleep(2)
            if index % 4 == 0:
                repsone = io.request(f'CALL ECHO: Hello'.encode())
                print(f'REPSONE:', repsone.decode())
                
        io.push(f'STOP'.encode())
        io.shutdown()
        io.join()
        
        print('FINISHED!')
    except Exception as e:
        traceback.print_exc()