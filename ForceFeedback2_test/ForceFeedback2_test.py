
import asyncio
import collections
import math
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
    long_force = PyForceFeedback2.ConstantForce(PyForceFeedback2.AXIS_Y)
    long_force.magnitude =0

    lat_force = PyForceFeedback2.ConstantForce(PyForceFeedback2.AXIS_X)
    lat_force.magnitude =0
    

    svr = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
    svr.bind(("127.0.0.1", 10001))	
    svr.setblocking(False)

    loop = asyncio.get_running_loop()
    
    min_val = 100
    max_val = 0

    def set_lat(val):
        lat_force.magnitude = max( -10000, min(10000, int(math.tanh(val/1.6) * 10000)))

    def set_long(val):
        long_force.magnitude = max( -10000, min(10000, int(math.tanh(val/1.6) * 10000)))


    while True:

        #We want the latest update, so get everything until we run out..
        buff = None    
        keep_going = True
        while buff is None or keep_going:
            try:
                buff, _ = svr.recvfrom(1024)  
            except BlockingIOError:
                keep_going = False
                await asyncio.sleep(0)
        
        
        late_g, long_g, vert_g = struct.unpack("<fff", buff[68:80])
       
        if vert_g > max_val:
            max_val = vert_g

        if vert_g < min_val:
            min_val = vert_g

             
        async with asyncio.TaskGroup() as tg:

            async def ex_lat(fun, g):
                await loop.run_in_executor(None, fun, g)

            task1 = tg.create_task(ex_lat(set_lat, late_g))
            task2 = tg.create_task(ex_lat(set_long, long_g))
         
        print(f"{min_val:2.2f}: {vert_g:2.2f}: {max_val:2.2f} : {long_force.magnitude} : lat: {late_g:2.2f} long:{long_g:2.2f}")
        

    

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
