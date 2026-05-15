#pragma once
#include "pch.h"

inline FName LogCategory;

#define Log(format, ...) ((void(*&)(const wchar_t*, int32, const FName&, uint8, const wchar_t*, ...)) Sarah::Offsets::FMsgLogf)(/*TEXT(__FILE__), __LINE__*/ nullptr, 0, LogCategory, 4, format, ##__VA_ARGS__);

class Utils
{
    static inline void* _NpFH = nullptr;
public:
    template <class _Ot = void*>
    static void Hook(uint64_t _Ptr, void* _Detour, _Ot& _Orig = _NpFH)
    {
        MH_CreateHook((LPVOID)_Ptr, _Detour, (LPVOID*)(std::is_same_v<_Ot, void*> ? nullptr : &_Orig));
    }

    static __declspec(noinline) void _Hook(UClass *Class, uint32_t _Ind, void* _Detour, void** _Orig)
    {
        auto _Vt = Class->DefaultObject->VTable;
        DWORD _Vo;
        VirtualProtect(_Vt + _Ind, 8, PAGE_EXECUTE_READWRITE, &_Vo);
        if (_Orig)
            *_Orig = (void *) _Vt[_Ind];
        _Vt[_Ind] = _Detour;
        VirtualProtect(_Vt + _Ind, 8, _Vo, &_Vo);
    }

    template <typename _Ct, typename _Ot = void*>
    static void Hook(uint32_t _Ind, void* _Detour, _Ot& _Orig = _NpFH)
    {
        _Hook(_Ct::StaticClass(), _Ind, _Detour, std::is_same_v<_Ot, void*> ? nullptr : (void **) &_Orig);
    }

    static __declspec(noinline) void _HookEvery(UClass *Class, uint32_t _Ind, void* _Detour)
    {
        for (int i = 0; i < UObject::GObjects->Num(); i++) {
            auto Obj = UObject::GObjects->GetByIndex(i);
            if (Obj && Obj->IsA(Class)) {
                DWORD _Vo;
                VirtualProtect(Obj->VTable + _Ind, 8, PAGE_EXECUTE_READWRITE, &_Vo);
                Obj->VTable[_Ind] = _Detour;
                VirtualProtect(Obj->VTable + _Ind, 8, _Vo, &_Vo);
            }
        }
    }


    template <typename _Ct>
    static void HookEvery(uint32_t _Ind, void* _Detour)
    {
        return _HookEvery(_Ct::StaticClass(), _Ind, _Detour);
    }

    template <typename _Ct, typename _Ot = void*>
    static void ExecHookEvery(const char* ShortName, void* _Detour, UEAllocatedMap<UFunction*, _Ot>* _OrigMap = nullptr)
    {
        for (int i = 0; i < UObject::GObjects->Num(); i++)
        {
            auto Obj = UObject::GObjects->GetByIndex(i);
            if (Obj && Obj->IsDefaultObject() && Obj->IsA<_Ct>())
            {
                auto _Fn = Obj->Class->FindFunction(ShortName);
                if (_OrigMap)
                    (*_OrigMap)[_Fn] = _Fn->ExecFunction;
                ExecHook(_Fn, _Detour);
            }
        }
    }

    template <typename _Is>
    static void Patch(uintptr_t ptr, _Is byte)
    {
        DWORD og;
        VirtualProtect(LPVOID(ptr), sizeof(_Is), PAGE_EXECUTE_READWRITE, &og);
        *(_Is*)ptr = byte;
        VirtualProtect(LPVOID(ptr), sizeof(_Is), og, &og);
    }

    static UObject* InternalFindObject(const wchar_t *ObjectPath, UClass* Class)
    {
        auto StaticFindObjectInternal = (UObject * (*)(UClass*, UObject*, const wchar_t*, bool)) Sarah::Offsets::StaticFindObject;
        return StaticFindObjectInternal(Class, nullptr, ObjectPath, false);
    }

    static UObject* InternalLoadObject(const wchar_t* ObjectPath, UClass* InClass, UObject* Outer = nullptr)
    {
        auto StaticLoadObjectInternal = (UObject * (*)(UClass*, UObject*, const TCHAR*, const TCHAR*, uint32_t, UObject*, bool)) Sarah::Offsets::StaticLoadObject;
        return StaticLoadObjectInternal(InClass, Outer, ObjectPath, nullptr, 0, nullptr, false);
    }

    static UObject* FindObject(const wchar_t*, UClass*);

    template <typename _Ot>
    static _Ot* FindObject(const wchar_t* ObjectPath, UClass* Class = _Ot::StaticClass())
    {
        return (_Ot*)FindObject(ObjectPath, Class);
    }

    template <typename _Ot>
    static _Ot* FindObject(UEAllocatedWString ObjectPath, UClass* Class = _Ot::StaticClass())
    {
        return (_Ot*)FindObject(ObjectPath.c_str(), Class);
    }

    static TArray<AActor*> GetAll(UClass* Class);

    template <typename _At = AActor>
    static TArray<_At*> GetAll(UClass* Class)
    {
        return GetAll(Class);
    }

    template <typename _At = AActor>
    static TArray<_At*> GetAll()
    {
        return GetAll(_At::StaticClass());
    }

    template<typename Type>
    static Type* GetRandomObjectOfClass()
    {
        TArray<Type*> AllObjs = GetAll<Type>();

        if (AllObjs.Num() < 1)
            return nullptr;

        return AllObjs[UKismetMathLibrary::RandomIntegerInRange(0, AllObjs.Num() - 1)];
    }

    static AActor* SpawnActor(UClass* Class, FTransform& Transform, AActor* Owner = nullptr);

    static AActor* SpawnActor(UClass* Class, FVector& Loc, FRotator& Rot, AActor* Owner = nullptr);

    template <typename T = AActor>
    static T* SpawnActorUnfinished(UClass* Class, FVector Loc, FRotator Rot = {}, AActor* Owner = nullptr)
    {
        FTransform Transform(Loc, Rot);

        return (T*)UGameplayStatics::BeginDeferredActorSpawnFromClass(UWorld::GetWorld(), Class, Transform, ESpawnActorCollisionHandlingMethod::AlwaysSpawn, Owner);
    }

    template <typename T = AActor>
    static T* FinishSpawnActor(AActor* Actor, FVector Loc, FRotator Rot)
    {
        FTransform Transform(Loc, Rot);

        return (T*)UGameplayStatics::FinishSpawningActor(Actor, Transform);
    };


    template <typename T>
    static T* SpawnActor(UClass* Class, FVector Loc, FRotator Rot = {}, AActor* Owner = nullptr)
    {
        return (T*)SpawnActor(Class, Loc, Rot, Owner);
    }

    template <typename T>
    static T* SpawnActor(UClass* Class, FTransform& Transform, AActor* Owner = nullptr)
    {
        return (T*)SpawnActor(Class, Transform, Owner);
    }

    template <typename T>
    static T* SpawnActor(FVector Loc, FRotator Rot = {}, AActor* Owner = nullptr)
    {
        return (T*)SpawnActor(T::StaticClass(), Loc, Rot, Owner);
    }

    template <typename T>
    static T* SpawnActor(FTransform& Transform, AActor* Owner = nullptr)
    {
        return (T*)SpawnActor(T::StaticClass(), Transform, Owner);
    }


    static __declspec(noinline) void _ExecHook(UFunction* _Fn, void* _Detour, void** _Orig)
    {
        if (!_Fn)
            return;
        if (_Orig != nullptr)
            *_Orig = _Fn->ExecFunction;

        _Fn->ExecFunction = reinterpret_cast<UFunction::FNativeFuncPtr>(_Detour);
    }

    static __declspec(noinline) void _ExecHook(const wchar_t *_Pt, void* _Detour, void** _Orig)
    {
        UFunction* _Fn = FindObject<UFunction>(_Pt);
        if (!_Fn)
            return;
        if (_Orig != nullptr)
            *_Orig = _Fn->ExecFunction;

        _Fn->ExecFunction = reinterpret_cast<UFunction::FNativeFuncPtr>(_Detour);
    }

    template <typename _Ot = void*>
    static void ExecHook(UFunction* _Fn, void* _Detour, _Ot& _Orig = _NpFH)
    {
        _ExecHook(_Fn, _Detour, std::is_same_v<_Ot, void*> ? nullptr : (void**) &_Orig);
    }

    template <typename _Ot = void*>
    static void ExecHook(const wchar_t* _Pt, void* _Detour, _Ot& _Orig = _NpFH)
    {
        _ExecHook(_Pt, _Detour, std::is_same_v<_Ot, void*> ? nullptr : (void**) &_Orig);
    }


    template <typename _It>
    static _It* GetInterface(UObject* Object)
    {
        return ((_It * (*)(UObject*, UClass*)) (Sarah::Offsets::ImageBase + 0xdddbe0))(Object, _It::StaticClass());
    }

    static double precision(double f, double places)
    {
        double n = pow(10., places);
        return round(f * n) / n;
    }

    static float EvaluateScalableFloat(FScalableFloat& Float);
};

class alignas(0x8) FOutputDevice
{
public:
    bool bSuppressEventTag;
    bool bAutoEmitLineTerminator;
};

struct FOutParmRec
{
    FProperty* Property;
    uint8* PropAddr;
    FOutParmRec* NextOutParm;
};

class FFrame : public FOutputDevice
{
public:
    void** VTable;
    UFunction* Node;
    UObject* Object;
    uint8* Code;
    uint8* Locals;
    FProperty* MostRecentProperty;
    uint8_t* MostRecentPropertyAddress;
    uint8* MostRecentPropertyContainer;
    TArray<void*> FlowStack;
    FFrame* PreviousFrame;
    FOutParmRec* OutParms;
    uint8_t _Padding1[0x20]; // wtf else do they store here
    FField* PropertyChainForCompiledIn;	
    UFunction* CurrentNativeFunction;
    FFrame* PreviousTrackingFrame;
    bool bArrayContextFailed;

public:
    void StepCompiledIn(void* const Result = nullptr);


    void* StepCompiledInRefInternal(void*);

    template <typename T>
    T& StepCompiledInRef()
    {
        T TempVal{};
        return *(T*)StepCompiledInRefInternal(&TempVal);
    }

    void IncrementCode();
};
static_assert(offsetof(FFrame, Object) == 0x18, "FFrame::Object offset is wrong!");
static_assert(offsetof(FFrame, Code) == 0x20, "FFrame::Code offset is wrong!");
static_assert(offsetof(FFrame, PropertyChainForCompiledIn) == 0x88, "FFrame::PropertyChainForCompiledIn offset is wrong!");


inline std::vector<void(*)()> _HookFuncs;
#define DefHookOg(_Rt, _Name, ...) static inline _Rt (*_Name##OG)(##__VA_ARGS__); static _Rt _Name(##__VA_ARGS__); 
#define DefUHookOg(_Name) static inline void (*_Name##OG)(UObject*, FFrame&); static void _Name(UObject*, FFrame&); 
#define DefUHookOgRet(_Rt, _Name) static inline void (*_Name##OG)(UObject*, FFrame&, _Rt*); static void _Name(UObject *, FFrame&, _Rt*);
#define InitHooks static void Hook(); static int _AddHook() { _HookFuncs.push_back(Hook); return 0; }; static inline auto _HookAdder = _AddHook();
#define callOG(_Tr, _Fn, _Th, ...) ([&](){ _Fn->ExecFunction = (UFunction::FNativeFuncPtr) _Th##OG; _Tr->_Th(##__VA_ARGS__); _Fn->ExecFunction = (UFunction::FNativeFuncPtr) _Th; })()
#define callOGWithRet(_Tr, _Fn, _Th, ...) ([&](){ _Fn->ExecFunction = (UFunction::FNativeFuncPtr) _Th##OG; auto _Rt = _Tr->_Th(##__VA_ARGS__); _Fn->ExecFunction = (UFunction::FNativeFuncPtr) _Th; return _Rt; })()
