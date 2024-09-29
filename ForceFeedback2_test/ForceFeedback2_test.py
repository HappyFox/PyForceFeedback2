
import asyncio
import collections
import math
import select
import socket
import struct

import PyForceFeedback2
import pyvjoy





def map_axis(val):
    return   (val * 0x8000 // 0xffff)



pov_idx = [0, 4500, 9000, 13500, 18000, 22500, 27000, 31500]


Settings = collections.namedtuple("Settings", ["sensitivity"])


async def joy_poller(settings):
    vjoy = pyvjoy.VJoyDevice(1)
    vjoy.reset()
    while True:
        await asyncio.sleep(0) # Yeild
        joy_state = PyForceFeedback2.poll()

        but_val = 0
        for idx, but_state in enumerate(joy_state.buttons):
            if but_state:
                but_val += 1 << idx

        vjoy.data.wAxisX = map_axis(joy_state.x)
        vjoy.data.wAxisY = map_axis(joy_state.y)
        vjoy.data.wAxisZRot = map_axis(joy_state.r_z)
        vjoy.data.wAxisZ = map_axis(joy_state.throttle)
        
        if joy_state.pov is not None:
            but_val += 1 << (pov_idx.index(joy_state.pov) + 8)
                
        vjoy.data.lButtons = but_val

        vjoy.update()

async def force_feed_back(settings):
    PyForceFeedback2.acquire()
    x_y_force = PyForceFeedback2.ConstantForce()
    
    

    svr = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
    svr.bind(("127.0.0.1", 10001))	
    svr.setblocking(False)

    loop = asyncio.get_running_loop()
    
    min_val = 100
    max_val = 0

    
    while True:

        #We want the latest update, so get everything until we run out..
        buff = None    

        read, _, _ = await loop.run_in_executor(None, select.select, [svr], [],[])
        while True:
            buff, _ = svr.recvfrom(1024) 
            read, _, _ = await loop.run_in_executor(None, select.select, [svr], [],[], 0)
            if not read:
                break
                
        late_g, long_g, vert_g = struct.unpack("<fff", buff[68:80])
       
        if vert_g > max_val:
            max_val = vert_g

        if vert_g < min_val:
            min_val = vert_g

        lat_dir = max( -10000, min(10000, int(math.tanh(late_g/1.6) * 10000)))
        long_dir = max( -10000, min(10000, int(math.tanh(long_g/1.6) * 10000)))
             
        await loop.run_in_executor(None,x_y_force.set_direction, lat_dir, long_dir )
         
        print(f"{min_val:2.2f}: {vert_g:2.2f}: {max_val:2.2f} : lat: {late_g:2.2f} long:{long_g:2.2f}")
        

    

async def main():
    settings = Settings(sensitivity=1000)
    async with asyncio.TaskGroup() as tg:
        task1 = tg.create_task(joy_poller(settings))
        task2 = tg.create_task(force_feed_back(settings))

if __name__ == "__main__":
    try:
        print("init")
        PyForceFeedback2.init()
        asyncio.run(main())
        print("inited")
    finally:
        PyForceFeedback2.release()
