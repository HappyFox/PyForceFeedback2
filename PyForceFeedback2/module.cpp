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

std::tuple<long, long, long, long, buttons>poll() {
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

        unsigned char pressed = 128;


        buttons but = buttons(
            js.rgbButtons[0] && pressed,
            js.rgbButtons[1] && pressed,
            js.rgbButtons[2] && pressed,
            js.rgbButtons[3] && pressed,
            js.rgbButtons[4] && pressed,
            js.rgbButtons[5] && pressed,
            js.rgbButtons[6] && pressed,
            js.rgbButtons[7] && pressed
        );

         return { js.lX,js.lY, js.lRz, js.rglSlider[0], but};
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


PYBIND11_MODULE(PyForceFeedback2, m) {
    m.def("init", &init, R"pbdoc(
        Initialize the joystick.
    )pbdoc");
    m.def("poll", &poll, R"pbdoc(
        Poll and return the axis values.
    )pbdoc");
    m.def("release", &release, R"pbdoc(
        Release the Joystick.
    )pbdoc");



#ifdef VERSION_INFO
    m.attr("__version__") = VERSION_INFO;
#else
    m.attr("__version__") = "dev";
#endif
}