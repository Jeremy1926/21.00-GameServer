#include "pch.h"
#include "Utils.h"

__declspec(noinline) UObject* Utils::FindObject(const wchar_t* ObjectPath, UClass* Class)
{
    auto Object = InternalFindObject(ObjectPath, Class);
    return Object ? Object : InternalLoadObject(ObjectPath, Class);
}

TArray<AActor*> Utils::GetAll(UClass* Class)
{
    TArray<AActor*> ret;

    if (Class->IsSubclassOf(AActor::StaticClass()))
    {
        UGameplayStatics::GetAllActorsOfClass(UWorld::GetWorld(), Class, &ret);
    }
    else
    {
        for (int i = 0; i < UObject::GObjects->Num(); i++)
        {
            UObject* Obj = UObject::GObjects->GetByIndex(i);

            if (!Obj || !Obj->IsA(Class))
                continue;

            ret.Add((AActor*)Obj); // i cba to change func to template
        }
    }

    return ret;
}

/*// Basic.hpp
FName::FName(FString str) {
    *this = UKismetStringLibrary::Conv_StringToName(str);
}*/

UObject* SDK::InternalGet(FSoftObjectPtr* Ptr, UClass* Class)
{
    /*if (!Ptr)
        return nullptr;

    auto Asset = Ptr->Get();
    if (!Asset && Ptr->ObjectID.AssetPathName.ComparisonIndex > 0)
    {
        Asset = Utils::InternalLoadObject(Ptr->ObjectID.AssetPathName.GetRawWString().c_str(), Class);

        while (auto Redirector = Asset->Cast<UObjectRedirector>())
            Asset = Redirector->DestinationObject;
    }
    return Asset;*/
    auto LoadSynchronous = (UObject * (*)(FSoftObjectPtr*)) (Sarah::Offsets::ImageBase + 0x1089d6c);
    auto Asset = LoadSynchronous(Ptr);
    return Asset->Cast<UObject>(Class);
}

enum ESpawnActorNameMode : uint8
{
    Required_Fatal,
    Required_ErrorAndReturnNull,
    Required_ReturnNull,
    Requested
};

struct FActorSpawnParameters
{
public:
    FName Name;

    AActor* Template;
    AActor* Owner;
    APawn* Instigator;
    ULevel* OverrideLevel;
    UChildActorComponent* OverrideParentComponent;
    ESpawnActorCollisionHandlingMethod SpawnCollisionHandlingOverride;

private:
    uint8	bRemoteOwned : 1;
public:
    uint8	bNoFail : 1;
    uint8	bDeferConstruction : 1;
    uint8	bAllowDuringConstructionScript : 1;
    ESpawnActorNameMode NameMode;
    EObjectFlags ObjectFlags;
};

AActor* Utils::SpawnActor(UClass* Class, FTransform& Transform, AActor* Owner)
{
#ifndef _DEBUG
    FActorSpawnParameters addr{};

    addr.Owner = Owner;
    addr.bDeferConstruction = false;
    addr.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    addr.NameMode = ESpawnActorNameMode::Requested;

    return ((AActor * (*)(UWorld*, UClass*, FTransform const*, FActorSpawnParameters*))(Sarah::Offsets::ImageBase + 0xe44e90))(UWorld::GetWorld(), Class, &Transform, &addr);
#else
    return UGameplayStatics::FinishSpawningActor(UGameplayStatics::BeginDeferredActorSpawnFromClass(UWorld::GetWorld(), Class, Transform, ESpawnActorCollisionHandlingMethod::AlwaysSpawn, Owner), Transform);
#endif
}

AActor* Utils::SpawnActor(UClass* Class, FVector& Loc, FRotator& Rot, AActor* Owner)
{
    FTransform Transform(Loc, Rot);
    return SpawnActor(Class, Transform, Owner);
}

__declspec(noinline) void FFrame::StepCompiledIn(void* const Result)
{
    if (Code)
    {
        ((void (*)(FFrame*, UObject*, void* const)) Sarah::Offsets::Step)(this, Object, Result);
    }
    else
    {
        FField* _Prop = PropertyChainForCompiledIn;
        PropertyChainForCompiledIn = _Prop->Next;
        ((void (*)(FFrame*, void* const, FField*)) Sarah::Offsets::StepExplicitProperty)(this, Result, _Prop);
    }
}

__declspec(noinline) void *FFrame::StepCompiledInRefInternal(void* _Tm)
{
    MostRecentPropertyAddress = nullptr;
    MostRecentPropertyContainer = nullptr;

    if (Code)
    {
        ((void (*)(FFrame*, UObject*, void* const)) Sarah::Offsets::Step)(this, Object, _Tm);
    }
    else
    {
        FField* _Prop = PropertyChainForCompiledIn;
        PropertyChainForCompiledIn = _Prop->Next;
        ((void (*)(FFrame*, void* const, FField*)) Sarah::Offsets::StepExplicitProperty)(this, _Tm, _Prop);
    }

    return MostRecentPropertyAddress ? MostRecentPropertyAddress : _Tm;
}


void FFrame::IncrementCode() 
{
    if (Code)
        Code++;
}

double FRotator::UnwindDegrees(double Angle) {

    Angle = fmod(Angle, 360.0f); // rat

    if (Angle < 0.)
        Angle += 360.;

    if (Angle > 180.)
        Angle -= 360.;

    return Angle;
}

double FRotator::ClampAxis(double Angle)
{
    Angle = fmod(Angle, 360.); // rat

    if (Angle < 0.)
        Angle += 360.;
    return Angle;
}

FRotator FQuat::Rotator()
{
    const double SingularityTest = Z * X - W * Y;
    const double YawY = 2. * (W * Z + X * Y);
    const double YawX = (1. - 2. * ((Y * Y) + (Z * Z)));

    const double SINGULARITY_THRESHOLD = 0.4999995;
    const double RAD_TO_DEG = 57.29577951308232;
    FRotator RotatorFromQuat{};

    if (SingularityTest < -SINGULARITY_THRESHOLD)
    {
        RotatorFromQuat.Pitch = -90.;
        RotatorFromQuat.Yaw = atan2(YawY, YawX) * RAD_TO_DEG;
        RotatorFromQuat.Roll = FRotator::NormalizeAxis(-RotatorFromQuat.Yaw - (2. * atan2(X, W) * RAD_TO_DEG));
    }
    else if (SingularityTest > SINGULARITY_THRESHOLD)
    {
        RotatorFromQuat.Pitch = 90.;
        RotatorFromQuat.Yaw = atan2(YawY, YawX) * RAD_TO_DEG;
        RotatorFromQuat.Roll = FRotator::NormalizeAxis(RotatorFromQuat.Yaw - (2. * atan2(X, W) * RAD_TO_DEG));
    }
    else
    {
        RotatorFromQuat.Pitch = asin(2. * SingularityTest) * RAD_TO_DEG;
        RotatorFromQuat.Yaw = atan2(YawY, YawX) * RAD_TO_DEG;
        RotatorFromQuat.Roll = atan2(-2. * (W * X + Y * Z), (1. - 2. * ((X * X) + (Y * Y)))) * RAD_TO_DEG;
    }

    return RotatorFromQuat;
}

void UC::_TArrayAdd(void*& Data, int32& NumElements, int32& MaxElements, int32 _Elem_Sz, const void* Element) 
{
    if (NumElements + 1 > MaxElements) Data = FMemory::Realloc(Data, (MaxElements = NumElements + 1) * _Elem_Sz);

    __movsb(PBYTE(Data) + (NumElements * _Elem_Sz), (const PBYTE)Element, _Elem_Sz);
    NumElements++;
}

void UC::_TArrayRemove(void*& Data, int32& NumElements, int32 _Elem_Sz, int32 _Idx)
{
    NumElements--; 

    for (int i = _Idx; i < NumElements; i++) {
        //Data[i] = Data[i + 1]; 
        __movsb(PBYTE(__int64(Data) + _Elem_Sz * i), (const PBYTE) (__int64(Data) + _Elem_Sz * (i + 1)), _Elem_Sz);
    }
}

FVector_NetQuantize10 FVector::Quantize10()
{
    FVector_NetQuantize10 ret;
    ret.X = Utils::precision(X, 1);
    ret.Y = Utils::precision(Y, 1);
    ret.Z = Utils::precision(Z, 1);
    return ret;
}

FVector_NetQuantize100 FVector::Quantize100()
{
    FVector_NetQuantize100 ret;
    ret.X = Utils::precision(X, 2);
    ret.Y = Utils::precision(Y, 2);
    ret.Z = Utils::precision(Z, 2);
    return ret;
}

// probably scuffed but wtv!
FVector_NetQuantizeNormal FVector::QuantizeNormal()
{
    FVector_NetQuantizeNormal ret;
    ret.X = X > 0 ? X / 100000 : -(-X / 100000);
    ret.Y = Y > 0 ? Y / 100000 : -(-Y / 100000);
    ret.Z = Z > 0 ? Z / 100000 : -(-Z / 100000);
    return ret;
}

float Utils::EvaluateScalableFloat(FScalableFloat& Float)
{
    if (!Float.Curve.CurveTable)
        return Float.Value;

    float Out;
    UDataTableFunctionLibrary::EvaluateCurveTableRow(Float.Curve.CurveTable, Float.Curve.RowName, (float)0, nullptr, &Out, FString());
    return Out;
}