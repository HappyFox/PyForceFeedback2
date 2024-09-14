
import PyForceFeedback2
import time

#joy = PyForceFeedback2.test()

try:
    print("init")
    PyForceFeedback2.init()
    print("inited")

    while True:
        print("FORCE POLL!!")
        #breakpoint()
        #print(PyForceFeedback2.test())
        print(PyForceFeedback2.poll())
        time.sleep(0.5)
finally:
    PyForceFeedback2.release()