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
import time
import zmq
import argparse
import threading

class iothread:
    def __init__(self):
        self.thread = None
        self.publish = None
        self.interrupted = False

        self.context = zmq.Context()
        self.zsub  = self.context.socket(zmq.SUB)
        self.zreq  = self.context.socket(zmq.REQ)
        self.zpush = self.context.socket(zmq.PUSH)

    def run(self):
        self.interrupted = False
        self.thread = threading.Thread(target=self.__thread, name="run")
        self.thread.start()
        
    def join(self):
        self.thread.join()
        
    def shutdown(self):
        self.interrupted = True
        self.context.shutdown()

    def push(self, msg:bytes):
        if self.zpush:
            self.zpush.send(msg)
            
    def subscribe(self, subject:str):
        if self.zsub:
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
                for socket, event in poll.poll(10):
                    msg = socket.recv()
                    if socket == self.zsub and self.publish:
                        self.publish(msg)
        except Exception as e:
            print ("exception:", e)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--uid', action='store', required=True)
    parser.add_argument('--req', action='store', required=True)
    parser.add_argument('--sub', action='store', required=True)
    parser.add_argument('--push', action='store', required=True)

    # args = parser.parse_args('--uid "index_0" --req "tcp://localhost:5557" --sub "tcp://localhost:5558" --push "tcp://localhost:5559"'.split())
    args = parser.parse_args()

    args.uid = args.uid.strip(' \'"')
    args.req = args.req.strip(' \'"')
    args.sub = args.sub.strip(' \'"')
    args.push = args.push.strip(' \'"')

    keepgoing = True
    def publish(msg:bytes):
        print(f'Publish: ', msg.decode())
        if msg == b'stop':
            keepgoing = False

    io = iothread()
    io.zreq.connect(args.req) 
    io.zsub.connect(args.sub) 
    io.zpush.connect(args.push)
    io.zsub.identity = args.uid.encode()
    io.zreq.identity = args.uid.encode()
    io.zpush.identity = args.uid.encode()
    io.run()
    io.publish = publish
    io.push('ready'.encode())
    
    index = 0
    while keepgoing:
        index += 1
        io.push(f'PUSH: inxex {index}'.encode())
        
        time.sleep(1)
        if index % 4 == 0:
            repsone = io.request('call echo: Hello'.encode())
            print(f'Repsone: ', repsone.decode())
            
    io.shutdown()
    io.join()
    print('Finished!')
