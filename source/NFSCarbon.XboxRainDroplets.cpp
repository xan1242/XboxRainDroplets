#include "xrd.h"

//#define USE_D3D_HOOK

static LPDIRECT3DDEVICE9* pDev;
uint32_t* TheGameFlowManagerStatus_A99BBC = (uint32_t*)0x00A99BBC;
bool(__thiscall* View_AmIinATunnel)(void* View, int viewnum) = (bool(__thiscall*)(void*, int))0x007365E0;

struct bVector2
{
    float x;
    float y;
};

struct bVector3
{
    float x;
    float y;
    float z;
    float pad;
};

struct bVector4
{
    float x;
    float y;
    float z;
    float w;
};

struct bMatrix4
{
    bVector4 v0;
    bVector4 v1;
    bVector4 v2;
    bVector4 v3;
};

struct WaveData3
{
    struct bVector3 frequency;
    struct bVector3 amplitude;
};

struct CameraParams
{
    bMatrix4 Matrix;
    bVector3 Position;
    bVector3 Direction;
    bVector3 Target;
    WaveData3 PosNoise[3];
    WaveData3 RotNoise[3];
    bVector3 PosNoise2Value;
    bVector3 RotNoise2Value;
    float FocalDistance;
    float DepthOfField;
    float DOFFalloff;
    float DOFMaxIntensity;
    float NearZ;
    float FarZ;
    float LB_height;
    float SimTimeMultiplier;
    bVector4 FadeColor;
    float TargetDistance;
    unsigned short FieldOfView;
    unsigned short PaddingAngle;
    bVector2 PaddingVector2;
};

bVector3* bNormalize(bVector3* dest, bVector3* v)
{
    float v2;
    float v3;
    float v4;
    float v5;

    v2 = sqrt(((v->z * v->z) + ((v->x * v->x) + (v->y * v->y))));
    if (v2 == 0.0)
    {
        dest->x = 1.0f;
        dest->y = 0.0f;
        dest->z = 0.0f;
    }
    else
    {
        v3 = (1.0f / v2);
        v4 = v->y;
        v5 = v->z;
        dest->x = v->x * v3;
        dest->y = v4 * v3;
        dest->z = v5 * v3;
    }
    return dest;
}

bVector3 UpVector;
bVector3 LeftVector;
bVector3 ForwardVector;

class Camera
{
public:
    CameraParams CurrentKey;
    CameraParams PreviousKey;
    CameraParams VelocityKey;
    BOOL bClearVelocity;
    char Padding_820[3];
    float ElapsedTime;
    int LastUpdateTime;
    int LastDisparateTime;
    int RenderDash;
    float NoiseIntensity;

    bVector3* GetForwardVec()
    {
        D3DXMATRIX v2;
        D3DXMatrixTranspose(&v2, (D3DXMATRIX*)&(CurrentKey.Matrix));
        return bNormalize(&ForwardVector, (bVector3*)v2.m[2]);
    }

    bVector3* GetUpVec()
    {
        D3DXMATRIX v2;
        D3DXMatrixTranspose(&v2, (D3DXMATRIX*)&(CurrentKey.Matrix));
        return bNormalize(&UpVector, (bVector3*)v2.m[1]);
    }

    bVector3* GetLeftVec()
    {
        D3DXMATRIX v2;
        D3DXMatrixTranspose(&v2, (D3DXMATRIX*)&(CurrentKey.Matrix));
        return bNormalize(&LeftVector, (bVector3*)v2.m[0]);
    }
};

void __stdcall OnScreenRain_Update_Hook(void* View)
{
    if ((*TheGameFlowManagerStatus_A99BBC == 6))
    {
        Camera* cam = *(Camera**)(((int)View) + 0x40);

        if (WaterDrops::fTimeStep && !*WaterDrops::fTimeStep && (WaterDrops::ms_numDrops || WaterDrops::ms_numDropsMoving))
            WaterDrops::Clear();

        cam->GetUpVec();

        WaterDrops::up.x = -UpVector.x;
        WaterDrops::up.y = -UpVector.y;
        WaterDrops::up.z = -UpVector.z;

        cam->GetLeftVec();

        WaterDrops::right.x = -LeftVector.x;
        WaterDrops::right.y = -LeftVector.y;
        WaterDrops::right.z = -LeftVector.z;

        WaterDrops::at.x = (*cam).CurrentKey.RotNoise2Value.x;
        WaterDrops::at.y = (*cam).CurrentKey.RotNoise2Value.y;
        WaterDrops::at.z = (*cam).CurrentKey.RotNoise2Value.z;

        WaterDrops::pos.x = (*cam).CurrentKey.PosNoise2Value.x;
        WaterDrops::pos.y = (*cam).CurrentKey.PosNoise2Value.y;
        WaterDrops::pos.z = (*cam).CurrentKey.PosNoise2Value.z;

        WaterDrops::Process(*pDev);
        WaterDrops::ms_rainIntensity = 0.0f;
    }
}

injector::hook_back<bool(__cdecl*)(int EVIEW_ID)> hb_RVMVisible;
bool __cdecl FEngHud_ShouldRearViewMirrorBeVisible(int EVIEW_ID)
{
    if ((*TheGameFlowManagerStatus_A99BBC == 6))
    {
        WaterDrops::Render(*pDev);
    }
    return hb_RVMVisible.fun(EVIEW_ID);
}

void Init()
{
    CIniReader iniReader("");
    WaterDrops::bRadial = iniReader.ReadInteger("MAIN", "RadialMovement", 0) == 0;
    WaterDrops::bGravity = iniReader.ReadInteger("MAIN", "EnableGravity", 1) != 0;

#ifdef USE_D3D_HOOK
    //vtable gets overwritten at startup, so no point in patching it right away
    WaterDrops::bPatchD3D = false;

    //resetting rain
    WaterDrops::ProcessCallback2 = []()
    {
        WaterDrops::ms_rainIntensity = 0.0f;
    };

    //hooking create to get EndScene and Reset
    auto pattern = hook::pattern("E8 ? ? ? ? 6A 2B 6A 2B A3");
    static injector::hook_back<IDirect3D9* (WINAPI*)(UINT)> Direct3DCreate9;
    auto Direct3DCreate9Hook = [](UINT SDKVersion) -> IDirect3D9*
    {
        auto pID3D9 = Direct3DCreate9.fun(SDKVersion);
        auto pVTable = (UINT_PTR*)(*((UINT_PTR*)pID3D9));
        if (!WaterDrops::RealD3D9CreateDevice)
            WaterDrops::RealD3D9CreateDevice = (CreateDevice_t)pVTable[IDirect3D9VTBL::CreateDevice];
        injector::WriteMemory(&pVTable[IDirect3D9VTBL::CreateDevice], &WaterDrops::d3dCreateDevice, true);
        return pID3D9;
    }; Direct3DCreate9.fun = injector::MakeCALL(pattern.get_first(0), static_cast<IDirect3D9 * (WINAPI*)(UINT)>(Direct3DCreate9Hook), true).get(); //0x73088C

    //Patching after vtable is overwritten, using rain function. Also setting the rain intensity here.
    static auto dword_B4AFFC = *hook::get_pattern<uint32_t**>("8B 15 ? ? ? ? D9 82 A0 36 00 00", 2);
    pattern = hook::pattern("C7 05 ? ? ? ? ? ? ? ? B0 01 5E 81 C4 8C 00 00 00 C3");
    static auto dword_AB0BA4 = *pattern.get_first<uint32_t*>(2);
    struct RainDropletsHook
    {
        void operator()(injector::reg_pack& regs)
        {
            *dword_AB0BA4 = 0;
            if (*WaterDrops::pEndScene == (uint32_t)WaterDrops::RealD3D9EndScene)
                injector::WriteMemory(WaterDrops::pEndScene, &WaterDrops::d3dEndScene, true);

            if (*WaterDrops::pReset == (uint32_t)WaterDrops::RealD3D9Reset)
                injector::WriteMemory(WaterDrops::pReset, &WaterDrops::d3dReset, true);

            WaterDrops::ms_rainIntensity = float(**dword_B4AFFC / 20);
        }
    }; injector::MakeInline<RainDropletsHook>(pattern.get_first(0), pattern.get_first(10)); //0x722FA0
#else
    auto pattern = hook::pattern("A1 ? ? ? ? 8B 10 68 ? ? ? ? 50 FF 52 40");
    pDev = *pattern.get_first<LPDIRECT3DDEVICE9*>(1);
    struct ResetHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)pDev;
            WaterDrops::Reset();
        }
    }; injector::MakeInline<ResetHook>(pattern.get_first(0)); //0x72B3C5

    pattern = hook::pattern("D9 9E F0 33 00 00");
    struct RainIntensityHook
    {
        void operator()(injector::reg_pack& regs)
        {
            float f = 0.0f;
            _asm {fstp dword ptr[f]}
            *(float*)(regs.esi + 0x33F0) = f;

            if (View_AmIinATunnel(*(void**)(regs.esi + 0x288), 1))
                WaterDrops::ms_rainIntensity = 0.0f;
            else
                WaterDrops::ms_rainIntensity = *(float*)(regs.esi + 0x28C);
        }
    }; injector::MakeInline<RainIntensityHook>(pattern.get_first(0), pattern.get_first(6)); //0x007B3BBB

    pattern = hook::pattern("6A 01 E8 ? ? ? ? 83 C4 04 84 C0 74 2B E8 ? ? ? ? A1 ? ? ? ?");
    hb_RVMVisible.fun = injector::MakeCALL(pattern.get_first(2), FEngHud_ShouldRearViewMirrorBeVisible, true).get();

    //OnScreenRain::Update(View*)
    pattern = hook::pattern("55 8B EC 83 E4 F0 83 EC 24 A1 ? ? ? ? 53 56"); // 0x007C5AD0
    injector::MakeJMP(pattern.get_first(0), OnScreenRain_Update_Hook);
    TheGameFlowManagerStatus_A99BBC = *pattern.count(1).get(0).get<uint32_t*>(0x68);
#endif
    //hiding original droplets
    //eRenderRainDrops
    pattern = hook::pattern("A1 ? ? ? ? 8B 08 81 EC 8C 00 00 00 85 C9 75 09"); // 0x00722CB0
    injector::MakeNOP(pattern.get_first(0xF), 2, true);

    //Sim::Internal::mLastFrameTime
    pattern = hook::pattern("A1 ? ? ? ? 6A 01 6A 1C C7 44 24");
    WaterDrops::fTimeStep = *pattern.get_first<float*>(1);

    //View::AmIinATunnel(int viewnum)
    pattern = hook::pattern("8B 41 6C 85 C0 74 1C 8B 4C 24 04"); //0x007365E0
    View_AmIinATunnel = (bool(__thiscall*)(void*, int))pattern.get_first(0);
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallback(Init);
    });
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        if (!IsUALPresent()) { InitializeASI(); }
    }
    return TRUE;
}