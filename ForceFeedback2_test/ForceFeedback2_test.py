
import PyForceFeedback2
import time

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