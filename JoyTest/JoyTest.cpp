// JoyTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")


#define STRICT
#define DIRECTINPUT_VERSION 0x0800
#define _CRT_SECURE_NO_DEPRECATE
#ifndef _WIN32_DCOM
#define _WIN32_DCOM
#endif

#include <windows.h>
#include <commctrl.h>
#include <basetsd.h>
#include <dinput.h>
#include <dinputd.h>
#include <assert.h>
#include <oleauto.h>
#include <shellapi.h>
#include <winnt.h>
#include <winuser.h>
#include<synchapi.h>
#include <windows.h>

#pragma warning( disable : 4996 ) // disable deprecated warning 
#include <strsafe.h>
#pragma warning( default : 4996 )
#include "resource.h"
#pragma comment(lib, "runtimeobject.lib")

extern "C" {
    IMAGE_DOS_HEADER __ImageBase;
}
//EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)


LPDIRECTINPUT8 di;
HRESULT hr;


BOOL CALLBACK    EnumJoysticksCallback(const DIDEVICEINSTANCE* pdidInstance, VOID* pContext);


//LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

//}

#define SAFE_DELETE(p)  { if(p) { delete (p);     (p)=NULL; } }
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }


LPDIRECTINPUT8          g_pDI = NULL;
LPDIRECTINPUTDEVICE8    g_pJoystick = NULL;
LPDIRECTINPUTEFFECT     g_pEffect = NULL;



struct DI_ENUM_CONTEXT
{
    DIJOYCONFIG* pPreferredJoyCfg;
    bool bPreferredJoyCfgValid;
};


void clear_screen(char fill = ' ') {
    COORD tl = { 0,0 };
    CONSOLE_SCREEN_BUFFER_INFO s;
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(console, &s);
    DWORD written, cells = s.dwSize.X * s.dwSize.Y;
    FillConsoleOutputCharacter(console, fill, cells, tl, &written);
    FillConsoleOutputAttribute(console, s.wAttributes, cells, tl, &written);
    SetConsoleCursorPosition(console, tl);
}


HRESULT force_feedback(LPDIRECTINPUTDEVICE8 g_pDevice) {
    
    
    

    // This application needs only one effect: Applying raw forces.
    DWORD rgdwAxes[2] = { DIJOFS_X, DIJOFS_Y };
    LONG rglDirection[2] = { DI_FFNOMINALMAX, DI_FFNOMINALMAX};
    DICONSTANTFORCE cf;
    cf.lMagnitude= DI_FFNOMINALMAX;

    DIEFFECT eff;
    ZeroMemory(&eff, sizeof(eff));
    eff.dwSize = sizeof(DIEFFECT);
//    eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    eff.dwDuration = INFINITE;
    eff.dwSamplePeriod = 0;
    eff.dwGain = DI_FFNOMINALMAX;
    eff.dwTriggerButton = DIEB_NOTRIGGER;
    eff.dwTriggerRepeatInterval = 0;
    eff.cAxes = 2;
    eff.rgdwAxes = rgdwAxes;
    eff.rglDirection = rglDirection;
    eff.lpEnvelope = 0;
    eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
    eff.lpvTypeSpecificParams = &cf;
    eff.dwStartDelay = 0;

    // Create the prepared effect
    if (FAILED(hr = g_pDevice->CreateEffect(GUID_ConstantForce,
        &eff, &g_pEffect, NULL)))
    {
        return hr;
    }

    if (NULL == g_pEffect)
        return E_FAIL;

    g_pEffect->Start(1, 0);

    return S_OK;
}

int main()
{
    HWND hwnd=nullptr;
    const wchar_t CLASS_NAME[] = L"Sample Window Class";
    WNDCLASSEX wx = {};

    DIJOYSTATE2 js;           // DInput joystick state 


    std::cout << HINST_THISCOMPONENT << std::endl;

    wx.cbSize = sizeof(WNDCLASSEX);
    wx.lpfnWndProc = DefWindowProc;
    //wx.lpfnWndProc = pWndProc;        // function which will handle messages
    wx.hInstance = HINST_THISCOMPONENT;
    //wx.hInstance = GetModuleHandle(NULL);
    wx.lpszClassName = CLASS_NAME;
    if (RegisterClassEx(&wx)) {
        hwnd = CreateWindowEx(0, CLASS_NAME, L"dummy_name", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
    }

    //HINSTANCE hinst2 = (HINSTANCE)GetWindowLong(hwnd, -6);

    hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (VOID**)&g_pDI, NULL);
    //hr = DirectInput8Create(HINST_THISCOMPONENT, DIRECTINPUT_VERSION, IID_IDirectInput8, (VOID**)&g_pDI, NULL);

    DIJOYCONFIG PreferredJoyCfg = { 0 };
    DI_ENUM_CONTEXT enumContext;
    enumContext.pPreferredJoyCfg = &PreferredJoyCfg;
    enumContext.bPreferredJoyCfgValid = false;

    IDirectInputJoyConfig8* pJoyConfig = NULL;
    hr = g_pDI->QueryInterface(IID_IDirectInputJoyConfig8, (void**)&pJoyConfig);

    PreferredJoyCfg.dwSize = sizeof(PreferredJoyCfg);
    if (SUCCEEDED(pJoyConfig->GetConfig(0, &PreferredJoyCfg, DIJC_GUIDINSTANCE))) // This function is expected to fail if no joystick is attached
        enumContext.bPreferredJoyCfgValid = true;
    SAFE_RELEASE(pJoyConfig);

    hr = g_pDI->EnumDevices(DI8DEVCLASS_GAMECTRL,
        EnumJoysticksCallback,
        &enumContext, DIEDFL_ATTACHEDONLY| DIEDFL_FORCEFEEDBACK);


    if (NULL == g_pJoystick)
    {
        std::cout << "No Joysticks" << std::endl;
    }
    else
    {
        std::cout << "Joystick found" << std::endl;
    }

    hr = g_pJoystick->SetDataFormat(&c_dfDIJoystick2);
    //hr = g_pJoystick->SetCooperativeLevel(hwnd, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
    //hr = g_pJoystick->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
    hr = g_pJoystick->SetCooperativeLevel(hwnd, DISCL_EXCLUSIVE | DISCL_BACKGROUND);

    DIPROPDWORD dipdw;

    // Since we will be playing force feedback effects, we should disable the
    // auto-centering spring.
    dipdw.diph.dwSize = sizeof(DIPROPDWORD);
    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dipdw.diph.dwObj = 0;
    dipdw.diph.dwHow = DIPH_DEVICE;
    dipdw.dwData = FALSE;

    if (FAILED(hr = g_pJoystick->SetProperty(DIPROP_AUTOCENTER, &dipdw.diph)))
        return hr;

    hr = g_pJoystick->Acquire();
    while (hr == DIERR_INPUTLOST)
        hr = g_pJoystick->Acquire();

    hr = force_feedback(g_pJoystick);


    while (true){
        hr = g_pJoystick->Poll();
        if (FAILED(hr))
        {
            // DInput is telling us that the input stream has been
            // interrupted. We aren't tracking any state between polls, so
            // we don't have any special reset that needs to be done. We
            // just re-acquire and try again.
            hr = g_pJoystick->Acquire();
            while (hr == DIERR_INPUTLOST)
                hr = g_pJoystick->Acquire();

            continue;
        }
        
        // Get the input's device state
        if (FAILED(hr = g_pJoystick->GetDeviceState(sizeof(DIJOYSTATE2), &js)))
            continue; // The device should have been acquired during the Poll()
        clear_screen();
        std::cout << "X: " << js.lX << std::endl;
        std::cout << "Y: " << js.lY << std::endl;
        std::cout << "IRz: " << js.lRz << std::endl;
        std::cout << "rglSlider: " << js.rglSlider[0] << std::endl;
        //Sleep(100);
   
    }


   

    return 0;
}



// ---------------------------------------------------------------------------- -
// Name: EnumJoysticksCallback()
// Desc: Called once for each enumerated joystick. If we find one, create a
//       device interface on it so we can play with it.
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCE * pdidInstance,
    VOID * pContext)
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
// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
