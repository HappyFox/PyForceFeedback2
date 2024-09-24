#include <pybind11/pybind11.h>

#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")



#include <Windows.h>

#include <basetsd.h>
#include <dinput.h>
#include <dinputd.h>


#include <stdexcept>
#include <tuple>
#include <list>
#include <strsafe.h>
#include <iostream>

#include <pybind11/stl.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h> 
#include <pybind11/chrono.h>

#define DIRECTINPUT_VERSION 0x0800


using namespace pybind11::literals;
namespace py = pybind11;


extern "C" {
    IMAGE_DOS_HEADER __ImageBase;
}
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)

#define SAFE_DELETE(p)  { if(p) { delete (p);     (p)=NULL; } }
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }

LPDIRECTINPUT8          g_pDI = NULL;
LPDIRECTINPUTDEVICE8    g_pJoystick = NULL; 
LPDIRECTINPUTEFFECT     g_pEffect = NULL;
HWND hwnd = nullptr;


typedef std::tuple <bool, bool, bool, bool, bool, bool, bool, bool> buttons;

struct DI_ENUM_CONTEXT
{
    DIJOYCONFIG* pPreferredJoyCfg;
    bool bPreferredJoyCfgValid;
};


struct _JoyState {
    _JoyState(
        const long x, 
        const long y, 
        const long Rz, 
        const long throttle,
        py::object buttons,
        py::object pov
    ) : x(x), y(y), Rz(Rz), throttle(throttle), buttons(buttons), pov(pov) { }
    const long x, y, Rz, throttle;
    py::object buttons, pov;
};


BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCE* pdidInstance,
    VOID* pContext)
{
    DI_ENUM_CONTEXT* pEnumContext = (DI_ENUM_CONTEXT*)pContext;
    HRESULT hr;


    // Skip anything other than the perferred joystick device as defined by the control panel.  
    // Instead you could store all the enumerated joysticks and let the user pick.
    if (pEnumContext->bPreferredJoyCfgValid &&
        !IsEqualGUID(pdidInstance->guidInstance, pEnumContext->pPreferredJoyCfg->guidInstance))
        return DIENUM_CONTINUE;

    // Obtain an interface to the enumerated joystick.
    hr = g_pDI->CreateDevice(pdidInstance->guidInstance, &g_pJoystick, NULL);

    // If it failed, then we can't use this joystick. (Maybe the user unplugged
    // it while we were in the middle of enumerating it.)
    if (FAILED(hr))
        return DIENUM_CONTINUE;

    // Stop enumeration. Note: we're just taking the first joystick we get. You
    // could store all the enumerated joysticks and let the user pick.
    return DIENUM_STOP;
}

void init() {
    HRESULT hr;
    const wchar_t CLASS_NAME[] = L"PyForceFeedback2 Message Window Class";
    WNDCLASSEX wx = {};
    wx.cbSize = sizeof(WNDCLASSEX);
    wx.lpfnWndProc = DefWindowProc;
    wx.hInstance = HINST_THISCOMPONENT;
    wx.lpszClassName = CLASS_NAME;
    if (RegisterClassEx(&wx)) {
        hwnd = CreateWindowEx(0, CLASS_NAME, L"dummy_name", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
    }
    
    if (NULL == hwnd) {
        throw std::exception("Unable to Start Message window");
    }

    if (FAILED(hr = DirectInput8Create(GetModuleHandle(NULL), 0x0800, IID_IDirectInput8, (VOID**)&g_pDI, NULL))) {
        throw std::exception("Unable to initialize DirectX");
    }

    DIJOYCONFIG PreferredJoyCfg = { 0 };
    DI_ENUM_CONTEXT enumContext;
    enumContext.pPreferredJoyCfg = &PreferredJoyCfg;
    enumContext.bPreferredJoyCfgValid = false;

    IDirectInputJoyConfig8* pJoyConfig = NULL;
    if (FAILED(hr = g_pDI->QueryInterface(IID_IDirectInputJoyConfig8, (void**)&pJoyConfig))) {
        throw std::exception("Unable to query joysticks");
    }

    PreferredJoyCfg.dwSize = sizeof(PreferredJoyCfg);
    if (SUCCEEDED(pJoyConfig->GetConfig(0, &PreferredJoyCfg, DIJC_GUIDINSTANCE))) // This function is expected to fail if no joystick is attached
        enumContext.bPreferredJoyCfgValid = true;
    else
        throw std::exception("Unable to get Joystick config");

    SAFE_RELEASE(pJoyConfig);

    if (FAILED(hr = g_pDI->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, &enumContext, DIEDFL_ATTACHEDONLY | DIEDFL_FORCEFEEDBACK))) {
        throw std::exception("Unable to find a joystick with force feedback.");
    }

    if (NULL == g_pJoystick)
    {
        throw std::exception("Unable to find a joystick with force feedback.");
    }
    
    if (FAILED(hr = g_pJoystick->SetDataFormat(&c_dfDIJoystick2))) {
        throw std::exception("Unable to set the data format");
    }
    if (FAILED(hr = g_pJoystick->SetCooperativeLevel(hwnd, DISCL_EXCLUSIVE | DISCL_BACKGROUND))) {
        throw std::exception("Unable to set the Cooperative level to exclusive + background.");
    }

}


py::object build_py_joy_state(DIJOYSTATE2 js) {
    py::object PyForceFeedback2 = py::module::import("PyForceFeedback2");
    py::object joy_state = PyForceFeedback2.attr("JoyState");

    unsigned char pressed = 128;

    py::tuple buttons = py::make_tuple(
        (bool)js.rgbButtons[0] && pressed,
        (bool)js.rgbButtons[1] && pressed,
        (bool)js.rgbButtons[2] && pressed,
        (bool)js.rgbButtons[3] && pressed,
        (bool)js.rgbButtons[4] && pressed,
        (bool)js.rgbButtons[5] && pressed,
        (bool)js.rgbButtons[6] && pressed,
        (bool)js.rgbButtons[7] && pressed
    );

    py::object pov = py::none();
    if (!(LOWORD(js.rgdwPOV[0]) == 0xFFFF)) {
        pov = py::int_(js.rgdwPOV[0]);
    }

    return joy_state(js.lX, js.lY, js.lRz, js.rglSlider[0], buttons, pov);
}


void acquire() {
    HRESULT hr;
    if (FAILED(hr = g_pJoystick->Acquire())) {
        throw std::runtime_error("Runtime error acquiring joystick.");
    }
}

py::object poll(){
    HRESULT hr;
    DIJOYSTATE2 js;

    if (NULL == g_pJoystick) {
        throw std::exception("Not initialized.");
    }

    while (true) {
        hr = g_pJoystick->Poll();
        if (FAILED(hr))
        {
            // DInput is telling us that the input stream has been
            // interrupted. We aren't tracking any state between polls, so
            // we don't have any special reset that needs to be done. We
            // just re-acquire and try again.s
            hr = g_pJoystick->Acquire();
            while (hr == DIERR_INPUTLOST)
                hr = g_pJoystick->Acquire();

            continue;
        }
        // Get the input's device state
        if (FAILED(hr = g_pJoystick->GetDeviceState(sizeof(DIJOYSTATE2), &js)))
            continue;

        return build_py_joy_state(js);
    }
}


void release() {
    if (g_pJoystick)
        g_pJoystick->Unacquire();

    // Release any DirectInput objects.
    SAFE_RELEASE(g_pEffect);
    SAFE_RELEASE(g_pJoystick);
    SAFE_RELEASE(g_pDI);
}





class _ConstantForce {
public:
    _ConstantForce() {
        //DWORD rgdwAxes[2] = { DIJOFS_X, DIJOFS_Y };
        //LONG rglDirection[2] = { 0, DI_FFNOMINALMAX };
        HRESULT hr;
        this->di_cf.lMagnitude = 10000;
        ZeroMemory(&this->eff, sizeof(this->eff));

        this->eff.dwSize = sizeof(DIEFFECT);
        this->eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
        this->eff.dwDuration = INFINITE;
        this->eff.dwSamplePeriod = 0;
        this->eff.dwGain = DI_FFNOMINALMAX;
        this->eff.dwTriggerButton = DIEB_NOTRIGGER;
        this->eff.dwTriggerRepeatInterval = 0;
        this->eff.cAxes = 2;
        this->eff.rgdwAxes = this->rgdwAxes;
        this->eff.rglDirection = this->rglDirection;
        this->eff.lpEnvelope = 0;
        this->eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
        this->eff.lpvTypeSpecificParams = &this->di_cf;
        this->eff.dwStartDelay = 0;
        
        if (FAILED(hr = g_pJoystick->CreateEffect(GUID_ConstantForce, &this->eff, &this->pdiEffect, NULL))) {
            throw std::exception("Unable to set the Cooperative level to exclusive + background.");
        }
        
        this->pdiEffect->Start(1, 0);
    };


    void set_direction(LONG x, LONG y) {
        HRESULT hr;
        
        DICONSTANTFORCE cf;

        LONG rglDirection[2] = { x, y };
        
        DIEFFECT eff;
        ZeroMemory(&eff, sizeof(eff));
        eff.dwSize = sizeof(DIEFFECT);
        eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
        eff.cAxes = 2;
        eff.rglDirection = rglDirection;
        eff.lpEnvelope = 0;
        eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
        eff.lpvTypeSpecificParams = &cf;
        eff.dwStartDelay = 0;
        cf.lMagnitude = (DWORD)sqrt((double)x * (double)x + (double)y * (double)y);
        hr = this->pdiEffect->SetParameters(&eff, DIEP_DIRECTION |
            DIEP_TYPESPECIFICPARAMS |
            DIEP_START);
    }

    void set_magnitude(long magnitude) {
        HRESULT hr;
        if (!(magnitude <= DI_FFNOMINALMAX) && (magnitude >= -DI_FFNOMINALMAX)){
            throw std::exception("Magnitude is beyond DI_FFNOMINALMAX");
        }
        this->di_cf.lMagnitude = magnitude;
        hr = this->pdiEffect->SetParameters(&this->eff, DIEP_TYPESPECIFICPARAMS);


    };

    long get_magnitude() {
        return this->di_cf.lMagnitude;
    };

    DWORD rgdwAxes[2] = { DIJOFS_X, DIJOFS_Y };
    LONG rglDirection[2] = { 0, 0 };
    
    DICONSTANTFORCE di_cf;
    DIEFFECT eff;
    LPDIRECTINPUTEFFECT  pdiEffect;

};


PYBIND11_MODULE(PyForceFeedback2, m) {
    py::class_<_JoyState>(m, "JoyState")
        .def(py::init<const long, const long, const long, const long, py::object, py::object>())
        .def_readonly("x", &_JoyState::x)
        .def_readonly("y", &_JoyState::y)
        .def_readonly("r_z", &_JoyState::Rz)
        .def_readonly("throttle", &_JoyState::throttle)
        .def_readonly("buttons", &_JoyState::buttons)
        .def_readonly("pov", &_JoyState::pov)
        .def("__repr__",
            [](const _JoyState& a) {

                return "<JoyState: '" + std::to_string(a.x) + ", " + std::to_string(a.y) +", " + std::to_string(a.Rz) + ", " + std::to_string(a.throttle) + ", " + py::repr(a.buttons).cast<std::string>() + "," + py::repr(a.pov).cast<std::string>() + "'>";
            });

    py::class_<_ConstantForce>(m, "ConstantForce")
        .def(py::init<>())
        .def("set_direction", &_ConstantForce::set_direction)
        .def("set_magnitude", &_ConstantForce::set_magnitude)
        .def("get_magnitude", &_ConstantForce::get_magnitude)
        .def_property("magnitude", &_ConstantForce::get_magnitude, &_ConstantForce::set_magnitude);

    m.def("init", &init, R"pbdoc(
        Initialize the joystick.
    )pbdoc");
    m.def("poll", &poll, R"pbdoc(
        Poll and return the axis values.
    )pbdoc");
    m.def("acquire", &acquire, R"pbdoc(
        Acquire the joystick, exception on failure.
    )pbdoc");
    m.def("release", &release, R"pbdoc(
        Release the Joystick.
    )pbdoc");

    m.attr("AXIS_X") = py::int_(DIJOFS_X);
    m.attr("AXIS_Y") = py::int_(DIJOFS_Y);
    m.attr("DI_FFNOMINALMAX") = py::int_(DI_FFNOMINALMAX);

#ifdef VERSION_INFO
    m.attr("__version__") = VERSION_INFO;
#else
    m.attr("__version__") = "dev";
#endif
}