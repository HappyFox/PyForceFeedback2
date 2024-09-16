
import asyncio
import math
import socket
import struct

import PyForceFeedback2
import pyvjoy


vjoy = pyvjoy.VJoyDevice(1)


def map_axis(val):
    return   (val * 0x8000 // 0xffff)



pov_idx = [0, 4500, 9000, 13500, 18000, 22500, 27000, 31500]



async def joy_poller():
    
    cf = PyForceFeedback2.ConstantForce()
    cf.init(False, True)
    cf.magnitude =0

    angle = 0
    while True:
        await asyncio.sleep(0)
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
        else:
            vjoy.data.bHats = -1
        
        vjoy.data.lButtons = but_val

        vjoy.update()

async def force_feed_back():
    PyForceFeedback2.acquire()
    cf = PyForceFeedback2.ConstantForce()
    cf.init(False, True)
    cf.magnitude =0
    angle = 0

    svr = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
    svr.bind(("127.0.0.1", 10001))	
    svr.setblocking(False)

    loop = asyncio.get_running_loop()
    
    min_val = 100
    max_val = 0

    while True:
        angle = (angle +0.05) % math.tau

        buff, _ = await loop.sock_recvfrom(svr, 1024)

        vert_g, = struct.unpack("<f", buff[68:72])

        if vert_g > max_val:
            max_val = vert_g

        if vert_g < min_val:
            min_val = vert_g


        vert_g = max(-2, vert_g)
        vert_g = min(2, vert_g)

        cf.magnitude = int(vert_g * 5000)
        print(f"{min_val:.2f}: {vert_g:.2f}: {max_val:.2f} : {cf.magnitude}")
        

    

async def main():
    async with asyncio.TaskGroup() as tg:
        task1 = tg.create_task(joy_poller())
        task2 = tg.create_task(force_feed_back())

if __name__ == "__main__":
    try:
        print("init")
        PyForceFeedback2.init()
        asyncio.run(main())
        print("inited")
    finally:
        PyForceFeedback2.release()
