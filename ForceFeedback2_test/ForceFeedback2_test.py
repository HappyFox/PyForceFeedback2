
import PyForceFeedback2


import pyvjoy


vjoy = pyvjoy.VJoyDevice(1)


def map_axis(val):
    return   (val * 0x8000 // 0xffff)



pov_idx = [0, 4500, 9000, 13500, 18000, 22500, 27000, 31500]


try:
    print("init")
    PyForceFeedback2.init()
    print("inited")

    while True:
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
finally:
    PyForceFeedback2.release()