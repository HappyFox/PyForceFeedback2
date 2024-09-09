
import PyForceFeedback2
import time

print("init")
PyForceFeedback2.init()
print("inited")

while True:
    print(PyForceFeedback2.poll())
    time.sleep(0.5)